// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_ALMOST_IDLE_DECORATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_ALMOST_IDLE_DECORATOR_H_

#include "chrome/browser/performance_manager/common/page_almost_idle_data.h"

#include "base/time/time.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/observers/coordination_unit_graph_observer.h"

namespace performance_manager {

namespace testing {
class PageAlmostIdleDecoratorTestUtils;
}  // namespace testing

// The PageAlmostIdle decorator is responsible for determining when a page has
// reached an "almost idle" state after initial load, based on CPU and network
// quiescence, as well as an absolute timeout. This state is then updated on
// PageNodes in a graph.
class PageAlmostIdleDecorator : public GraphObserver {
 public:
  PageAlmostIdleDecorator();
  ~PageAlmostIdleDecorator() override;

  // GraphObserver implementation:
  bool ShouldObserve(const NodeBase* coordination_unit) override;
  void OnProcessPropertyChanged(
      ProcessNodeImpl* process_node,
      resource_coordinator::mojom::PropertyType property_type,
      int64_t value) override;
  void OnPageEventReceived(PageNodeImpl* page_node,
                           resource_coordinator::mojom::Event event) override;
  void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) override;
  void OnIsLoadingChanged(PageNodeImpl* page_node) override;

 protected:
  using LoadIdleState = PageAlmostIdleData::LoadIdleState;

  friend class PageAlmostIdleDecoratorTestHelper;
  friend class testing::PageAlmostIdleDecoratorTestUtils;

  // The amount of time a page has to be idle post-loading in order for it to be
  // considered loaded and idle. This is used in UpdateLoadIdleState
  // transitions.
  static constexpr base::TimeDelta kLoadedAndIdlingTimeout =
      base::TimeDelta::FromSeconds(1);

  // The maximum amount of time post-DidStopLoading a page can be waiting for
  // an idle state to occur before the page is simply considered loaded anyways.
  // Since PageAlmostIdle is intended as an "initial loading complete" signal,
  // it needs to eventually terminate. This is strictly greater than the
  // kLoadedAndIdlingTimeout.
  //
  // This is taken as the 95th percentile of tab loading times on desktop
  // (see SessionRestore.ForegroundTabFirstLoaded). This ensures that all tabs
  // eventually transition to loaded, even if they keep the main task queue
  // busy, or continue loading content.
  static constexpr base::TimeDelta kWaitingForIdleTimeout =
      base::TimeDelta::FromMinutes(1);

  // These are called when properties/events affecting the load-idle state are
  // observed. Frame and Process variants will eventually all redirect to the
  // appropriate Page variant, where the real work is done.
  void UpdateLoadIdleStateFrame(FrameNodeImpl* frame_node);
  void UpdateLoadIdleStatePage(PageNodeImpl* page_node);
  void UpdateLoadIdleStateProcess(ProcessNodeImpl* process_node);

  // Helper function for transitioning to the final state.
  void TransitionToLoadedAndIdle(PageNodeImpl* page_node);

  // Convenience accessors for state associated with a |page_node|.
  static PageAlmostIdleData* GetOrCreateData(PageNodeImpl* page_node);
  static PageAlmostIdleData* GetData(PageNodeImpl* page_node);
  static bool IsIdling(const PageNodeImpl* page_node);

 private:
  DISALLOW_COPY_AND_ASSIGN(PageAlmostIdleDecorator);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_ALMOST_IDLE_DECORATOR_H_
