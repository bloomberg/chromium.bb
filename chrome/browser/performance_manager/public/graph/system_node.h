// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_

#include "base/macros.h"

namespace performance_manager {

class Graph;

// The SystemNode represents system-wide state. There is at most one system node
// in a graph.
class SystemNode {
 public:
  SystemNode();
  virtual ~SystemNode();

  // Returns the graph to which this node belongs.
  virtual Graph* GetGraph() const = 0;

  // Returns the private key which is used for indexing this object in the
  // graph. This is an opaque pointer strictly used for implementation.
  virtual const void* GetIndexingKey() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemNode);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_
