// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/performance_manager_tab_helper.h"

#include <set>

#include "base/test/bind_test_util.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_test_harness.h"
#include "chrome/browser/performance_manager/render_process_user_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

class PerformancemanagerTabHelperTest : public PerformanceManagerTestHarness {
 public:
  PerformancemanagerTabHelperTest() = default;

 protected:
  static size_t CountAllRenderProcessHosts() {
    size_t num_hosts = 0;
    for (auto it = content::RenderProcessHost::AllHostsIterator();
         !it.IsAtEnd(); it.Advance()) {
      ++num_hosts;
    }

    return num_hosts;
  }
};

TEST_F(PerformancemanagerTabHelperTest, FrameHierarchyReflectsToGraph) {
  SetContents(CreateTestWebContents());

  auto* parent = content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://parent"));
  DCHECK(parent);

  auto* parent_tester = content::RenderFrameHostTester::For(parent);
  auto* child1 = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://child1/"), parent_tester->AppendChild("child1"));
  auto* grandchild =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://grandchild/"),
          content::RenderFrameHostTester::For(child1)->AppendChild(
              "grandchild"));
  auto* child2 = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://child2/"), parent_tester->AppendChild("child2"));

  // Count the RFHs referenced.
  std::set<content::RenderProcessHost*> hosts;
  hosts.insert(main_rfh()->GetProcess());
  hosts.insert(child1->GetProcess());
  hosts.insert(grandchild->GetProcess());
  hosts.insert(child2->GetProcess());

  {
    // There may be more RenderProcessHosts in existence than those used from
    // the RFHs above. The graph may not reflect all of them, as only those
    // observed through the TabHelper will have been reflected in the graph.
    size_t num_hosts = CountAllRenderProcessHosts();
    EXPECT_LE(hosts.size(), num_hosts);
    EXPECT_NE(0u, hosts.size());

    PerformanceManager::GetInstance()->CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([num_hosts](GraphImpl* graph) {
          EXPECT_GE(num_hosts, graph->GetAllProcessNodes().size());
          EXPECT_EQ(4u, graph->GetAllFrameNodes().size());

          ASSERT_EQ(1u, graph->GetAllPageNodes().size());
          auto* page = graph->GetAllPageNodes()[0];

          // TODO(siggi): This may not be safe, as spurious RPHs may be created
          //     during/before navigation but not torn down yet.
          EXPECT_EQ(graph->GetAllProcessNodes().size(),
                    page->GetAssociatedProcessNodes().size());
          EXPECT_GE(num_hosts, page->GetAssociatedProcessNodes().size());

          EXPECT_EQ(4u, page->GetFrameNodes().size());
          ASSERT_EQ(1u, page->main_frame_nodes().size());

          auto* main_frame = page->GetMainFrameNode();
          EXPECT_EQ(2u, main_frame->child_frame_nodes().size());

          for (auto* child_frame : main_frame->child_frame_nodes()) {
            if (child_frame->url().spec() == "https://child1/") {
              ASSERT_EQ(1u, child_frame->child_frame_nodes().size());
              auto* grandchild_frame =
                  *(child_frame->child_frame_nodes().begin());
              EXPECT_EQ("https://grandchild/", grandchild_frame->url().spec());
            } else if (child_frame->url().spec() == "https://child2/") {
              EXPECT_TRUE(child_frame->child_frame_nodes().empty());
            } else {
              FAIL() << "Unexpected child frame: " << child_frame->url().spec();
            }
          }
        }));
    thread_bundle()->RunUntilIdle();
  }

  // Clean up the web contents, which should dispose of the page and frame nodes
  // involved.
  DeleteContents();

  // Allow content/ to settle.
  thread_bundle()->RunUntilIdle();

  {
    size_t num_hosts = CountAllRenderProcessHosts();

    PerformanceManager::GetInstance()->CallOnGraph(
        FROM_HERE, base::BindLambdaForTesting([num_hosts](GraphImpl* graph) {
          EXPECT_GE(num_hosts, graph->GetAllProcessNodes().size());
          EXPECT_EQ(0u, graph->GetAllFrameNodes().size());
          ASSERT_EQ(0u, graph->GetAllPageNodes().size());
        }));

    thread_bundle()->RunUntilIdle();
  }

  // The RenderProcessHosts seem to get leaked, or at least be still alive here,
  // so explicitly detach from them in order to clean up the graph nodes.
  RenderProcessUserData::DetachAndDestroyAll();
}

}  // namespace performance_manager
