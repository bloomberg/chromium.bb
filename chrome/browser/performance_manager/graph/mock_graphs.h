// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_MOCK_GRAPHS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_MOCK_GRAPHS_H_

#include "chrome/browser/performance_manager/graph/graph_test_harness.h"

namespace performance_manager {

class Graph;
class FrameNodeImpl;
class PageNodeImpl;
class ProcessNodeImpl;
class SystemNodeImpl;

// The following coordination unit graph topology is created to emulate a
// scenario when a single page executes in a single process:
//
// Pr  Pg
//  \ /
//   F
//
// Where:
// F: frame(frame_tree_id:0)
// Pr: process(pid:1)
// Pg: page
struct MockSinglePageInSingleProcessGraph {
  explicit MockSinglePageInSingleProcessGraph(Graph* graph);
  ~MockSinglePageInSingleProcessGraph();
  TestNodeWrapper<SystemNodeImpl> system;
  TestNodeWrapper<ProcessNodeImpl> process;
  TestNodeWrapper<PageNodeImpl> page;
  TestNodeWrapper<FrameNodeImpl> frame;
};

// The following coordination unit graph topology is created to emulate a
// scenario where multiple pages are executing in a single process:
//
// Pg  Pr OPg
//  \ / \ /
//   F  OF
//
// Where:
// F: frame(frame_tree_id:0)
// OF: other_frame(frame_tree_id:1)
// Pg: page
// OPg: other_page
// Pr: process(pid:1)
struct MockMultiplePagesInSingleProcessGraph
    : public MockSinglePageInSingleProcessGraph {
  explicit MockMultiplePagesInSingleProcessGraph(Graph* graph);
  ~MockMultiplePagesInSingleProcessGraph();
  TestNodeWrapper<PageNodeImpl> other_page;
  TestNodeWrapper<FrameNodeImpl> other_frame;
};

// The following coordination unit graph topology is created to emulate a
// scenario where a single page that has frames is executing in different
// processes (e.g. out-of-process iFrames):
//
// Pg  Pr
// |\ /
// | F  OPr
// |  \ /
// |__CF
//
// Where:
// F: frame(frame_tree_id:0)
// CF: child_frame(frame_tree_id:2)
// Pg: page
// Pr: process(pid:1)
// OPr: other_process(pid:2)
struct MockSinglePageWithMultipleProcessesGraph
    : public MockSinglePageInSingleProcessGraph {
  explicit MockSinglePageWithMultipleProcessesGraph(Graph* graph);
  ~MockSinglePageWithMultipleProcessesGraph();
  TestNodeWrapper<ProcessNodeImpl> other_process;
  TestNodeWrapper<FrameNodeImpl> child_frame;
};

// The following coordination unit graph topology is created to emulate a
// scenario where multiple pages are utilizing multiple processes (e.g.
// out-of-process iFrames and multiple pages in a process):
//
// Pg  Pr OPg___
//  \ / \ /     |
//   F   OF OPr |
//        \ /   |
//         CF___|
//
// Where:
// F: frame(frame_tree_id:0)
// OF: other_frame(frame_tree_id:1)
// CF: child_frame(frame_tree_id:3)
// Pg: page
// OPg: other_page
// Pr: process(pid:1)
// OPr: other_process(pid:2)
struct MockMultiplePagesWithMultipleProcessesGraph
    : public MockMultiplePagesInSingleProcessGraph {
  explicit MockMultiplePagesWithMultipleProcessesGraph(Graph* graph);
  ~MockMultiplePagesWithMultipleProcessesGraph();
  TestNodeWrapper<ProcessNodeImpl> other_process;
  TestNodeWrapper<FrameNodeImpl> child_frame;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_GRAPH_MOCK_GRAPHS_H_
