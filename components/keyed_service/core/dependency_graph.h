// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_GRAPH_H_
#define COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_GRAPH_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service_export.h"

class DependencyNode;

// Dynamic graph of dependencies between nodes.
class KEYED_SERVICE_EXPORT DependencyGraph {
 public:
  DependencyGraph();
  ~DependencyGraph();

  // Adds/Removes a node from our list of live nodes. Removing will
  // also remove live dependency links.
  void AddNode(DependencyNode* node);
  void RemoveNode(DependencyNode* node);

  // Adds a dependency between two nodes.
  void AddEdge(DependencyNode* depended, DependencyNode* dependee);

  // Topologically sorts nodes to produce a safe construction order
  // (all nodes after their dependees).
  bool GetConstructionOrder(std::vector<DependencyNode*>* order)
      WARN_UNUSED_RESULT;

  // Topologically sorts nodes to produce a safe destruction order
  // (all nodes before their dependees).
  bool GetDestructionOrder(std::vector<DependencyNode*>* order)
      WARN_UNUSED_RESULT;

  // Returns representation of the dependency graph in graphviz format.
  std::string DumpAsGraphviz(
      const std::string& toplevel_name,
      const base::RepeatingCallback<std::string(DependencyNode*)>&
          node_name_callback) const;

 private:
  typedef std::multimap<DependencyNode*, DependencyNode*> EdgeMap;

  // Populates |construction_order_| with computed construction order.
  // Returns true on success.
  bool BuildConstructionOrder() WARN_UNUSED_RESULT;

  // Keeps track of all live nodes (see AddNode, RemoveNode).
  std::vector<DependencyNode*> all_nodes_;

  // Keeps track of edges of the dependency graph.
  EdgeMap edges_;

  // Cached construction order (needs rebuild with BuildConstructionOrder
  // when empty).
  std::vector<DependencyNode*> construction_order_;

  DISALLOW_COPY_AND_ASSIGN(DependencyGraph);
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_DEPENDENCY_GRAPH_H_
