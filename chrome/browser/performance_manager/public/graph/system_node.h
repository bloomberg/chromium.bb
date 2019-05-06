// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_

#include "base/macros.h"

namespace performance_manager {

// The SystemNode represents system-wide state. There is at most one system node
// in a graph.
class SystemNode {
 public:
  SystemNode();
  virtual ~SystemNode();

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemNode);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_SYSTEM_NODE_H_
