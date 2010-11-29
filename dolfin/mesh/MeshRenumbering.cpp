// Copyright (C) 2010 Anders Logg.
// Licensed under the GNU LGPL Version 2.1.
//
// First added:  2010-11-27
// Last changed: 2010-11-29

#include <algorithm>
#include <boost/scoped_array.hpp>

#include <dolfin/log/log.h>
#include <dolfin/common/Timer.h>
#include "Cell.h"
#include "Mesh.h"
#include "MeshTopology.h"
#include "MeshGeometry.h"
#include "MeshRenumbering.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
void MeshRenumbering::renumber_by_color(Mesh& mesh)
{
  //info(TRACE, "Renumbering mesh by cell colors.");
  info("Renumbering mesh by cell colors.");

  info(mesh);

  // Check that mesh has been colored
  MeshFunction<uint>* cell_colors = mesh.data().mesh_function("cell colors");
  if (!cell_colors)
    error("Unable to renumber mesh by colors. Mesh has not been colored.");

  // Get mesh topology and geometry
  MeshTopology& topology = mesh.topology();
  MeshGeometry& geometry = mesh.geometry();

  // Issue warning if connectivity other than cell-vertex exists
  const uint D = topology.dim();
  for (uint d0 = 0; d0 <= D; d0++)
    for (uint d1 = 0; d1 <= D; d1++)
      if (!(d0 == D && d1 == 0) && topology(d0, d1).size() > 0)
        warning("Clearing connectivity data %d - %d.", d0, d1);

  // Clean connectivity since it may be invalid after renumbering
  mesh.clean();

  // Start timer
  Timer timer("Renumber mesh");

  // Get connectivity and coordinates
  MeshConnectivity& connectivity = topology(D, 0);
  uint* connections = connectivity.connections;
  double* coordinates = geometry.coordinates;

  // Allocate temporary arrays, used for copying data
  const uint connections_size = connectivity._size;
  const uint coordinates_size = geometry.size() * geometry.dim();
  boost::scoped_array<uint> new_connections(new uint[connections_size]);
  boost::scoped_array<double> new_coordinates(new double[coordinates_size]);

  // New vertex indices, -1 if not yet renumbered
  std::vector<int> new_vertex_indices(mesh.num_vertices());
  std::fill(new_vertex_indices.begin(), new_vertex_indices.end(), -1);

  // Iterate over colors
  const std::vector<uint>* num_colored_cells = mesh.data().array("num colored cells");
  assert(num_collored_cells);
  const uint num_colors = num_colored_cells->size();
  assert(num_colors > 0);
  uint connections_offset = 0;
  uint current_vertex = 0;
  for (uint color = 0; color < num_colors; color++)
  {
    // Get the array of cell indices of current color
    const std::vector<uint>* colored_cells = mesh.data().array("colored cells", color);
    assert(colored_cells);

    // Iterate over cells for current color
    for (uint i = 0; i < colored_cells->size(); i++)
    {
      // Current cell
      Cell cell(mesh, (*colored_cells)[i]);

      // Get array of vertices for current cell
      const uint* cell_vertices = cell.entities(0);
      const uint num_vertices = cell.num_entities(0);

      // Iterate over cell vertices
      for (uint j = 0; j < num_vertices; j++)
      {
        const uint vertex_index = cell_vertices[j];

        // Renumber and copy coordinate data
        if (new_vertex_indices[vertex_index] == -1)
        {
          const uint d = mesh.geometry().dim();
          std::copy(coordinates + vertex_index * d,
                    coordinates + (vertex_index + 1) * d,
                    new_coordinates.get() + current_vertex * d);
          new_vertex_indices[vertex_index] = current_vertex++;
        }

        // Renumber and copy connectivity data (must be done after vertex renumbering)
        const uint new_vertex_index = new_vertex_indices[vertex_index];
        assert(new_vertex_index >= 0);
        new_connections[connections_offset++] = new_vertex_index;
      }
    }
  }

  // Check that all vertices have been renumbered
  for (uint i = 0; i < new_vertex_indices.size(); i++)
    if (new_vertex_indices[i] == -1)
      error("Failed to renumber mesh, not all vertices renumbered.");

  // Copy data
  std::copy(new_connections.get(), new_connections.get() + connections_size, connections);
  std::copy(new_coordinates.get(), new_coordinates.get() + coordinates_size, coordinates);

  // Update renumbering data
  uint current_cell = 0;
  for (uint color = 0; color < num_colors; color++)
  {
    // Get the array of cell indices of current color
    std::vector<uint>* colored_cells = mesh.data().array("colored cells", color);

    // Update cell color data
    assert(colored_cells->size() == num_colored_cells[color]);
    for (uint i = 0; i < (*num_colored_cells)[color]; i++)
    {
      (*colored_cells)[i] = current_cell;
      (*cell_colors)[current_cell] = color;
      current_cell++;
    }
  }
}
//-----------------------------------------------------------------------------
