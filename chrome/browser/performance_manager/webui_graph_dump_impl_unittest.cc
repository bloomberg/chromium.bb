// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/webui_graph_dump_impl.h"

#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/graph_test_harness.h"
#include "chrome/browser/performance_manager/graph/mock_graphs.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class WebUIGraphDumpImplTest : public GraphTestHarness {};

TEST_F(WebUIGraphDumpImplTest, Create) {
  GraphImpl graph;
  MockMultiplePagesWithMultipleProcessesGraph mock_graph(&graph);

  base::TimeTicks now = PerformanceManagerClock::NowTicks();

  const GURL kExampleUrl("http://www.example.org");
  mock_graph.page->OnMainFrameNavigationCommitted(now, 1, kExampleUrl);
  mock_graph.other_page->OnMainFrameNavigationCommitted(now, 2, kExampleUrl);

  auto* main_frame = mock_graph.page->GetMainFrameNode();
  main_frame->set_url(kExampleUrl);

  WebUIGraphDumpImpl impl(&graph);

  resource_coordinator::mojom::WebUIGraphPtr returned_graph;
  WebUIGraphDumpImpl::GetCurrentGraphCallback callback =
      base::BindLambdaForTesting(
          [&returned_graph](resource_coordinator::mojom::WebUIGraphPtr graph) {
            returned_graph = std::move(graph);
          });
  impl.GetCurrentGraph(std::move(callback));

  task_env().RunUntilIdle();

  ASSERT_NE(nullptr, returned_graph.get());
  EXPECT_EQ(2u, returned_graph->pages.size());
  for (const auto& page : returned_graph->pages) {
    EXPECT_NE(0u, page->id);
    EXPECT_NE(0u, page->main_frame_id);
  }

  EXPECT_EQ(3u, returned_graph->frames.size());
  // Count the top-level frames as we go.
  size_t top_level_frames = 0;
  for (const auto& frame : returned_graph->frames) {
    if (frame->parent_frame_id == 0) {
      ++top_level_frames;

      // The page's main frame should have an URL.
      if (frame->id == NodeBase::GetSerializationId(main_frame))
        EXPECT_EQ(kExampleUrl, frame->url);
    }
    EXPECT_NE(0u, frame->id);
    EXPECT_NE(0u, frame->process_id);
  }
  // Make sure we have one top-level frame per page.
  EXPECT_EQ(returned_graph->pages.size(), top_level_frames);

  EXPECT_EQ(2u, returned_graph->processes.size());
  for (const auto& page : returned_graph->pages) {
    EXPECT_NE(0u, page->id);
    EXPECT_NE(0u, page->main_frame_id);
    EXPECT_EQ(kExampleUrl, page->main_frame_url);
  }
}

}  // namespace performance_manager
