// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/page_almost_idle_decorator_test_utils.h"

#include "base/run_loop.h"
#include "chrome/browser/performance_manager/decorators/page_almost_idle_decorator.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/resource_coordinator_clock.h"

namespace performance_manager {
namespace testing {

namespace {

// Helper observer for detecting a change to page almost idle.
class PageAlmostIdleObserverForTesting : public GraphObserver {
 public:
  PageAlmostIdleObserverForTesting(base::RunLoop* run_loop,
                                   PageNodeImpl* page_node)
      : run_loop_(run_loop), page_node_(page_node) {
    page_node_->AddObserver(this);
  }

  ~PageAlmostIdleObserverForTesting() override {
    page_node_->RemoveObserver(this);
  }

  // GraphObserver implementation:
  bool ShouldObserve(const NodeBase* coordination_unit) override {
    return false;
  }
  void OnPageAlmostIdleChanged(PageNodeImpl* page_node) override {
    if (page_node == page_node_ && page_node->page_almost_idle())
      run_loop_->Quit();
  }

 private:
  base::RunLoop* run_loop_ = nullptr;
  PageNodeImpl* page_node_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(PageAlmostIdleObserverForTesting);
};

}  // namespace

// static
void PageAlmostIdleDecoratorTestUtils::DrivePageToLoadedAndIdle(
    PageNodeImpl* page_node) {
  using PropertyType = resource_coordinator::mojom::PropertyType;
  auto* frame_node = page_node->GetMainFrameNode();
  auto* process_node = frame_node->GetProcessNode();

  if (page_node->page_almost_idle())
    return;

  // If no navigation has yet started, then initiate one. This will ensure that
  // the PageAlmostIdleData comes into existence.
  auto* data = PageAlmostIdleDecorator::GetData(page_node);
  if (!data) {
    page_node->OnMainFrameNavigationCommitted(
        ResourceCoordinatorClock::NowTicks(), 0, "foo.com");
    data = PageAlmostIdleDecorator::GetData(page_node);
    DCHECK(data);
  }

  // Walk through the state machine, tickling only those functions needed in
  // order to bring the state machine to its final state.
  switch (data->load_idle_state_) {
    case PageAlmostIdleData::LoadIdleState::kLoadedAndIdle:
      NOTREACHED();
      break;
    case PageAlmostIdleData::LoadIdleState::kLoadingNotStarted:
      DCHECK(!page_node->is_loading());
      page_node->SetIsLoading(true);
      FALLTHROUGH;
    case PageAlmostIdleData::LoadIdleState::kLoading:
      DCHECK(page_node->is_loading());
      page_node->SetIsLoading(false);
      FALLTHROUGH;
    case PageAlmostIdleData::LoadIdleState::kLoadedNotIdling:
      if (!frame_node->network_almost_idle())
        frame_node->SetNetworkAlmostIdle(true);
      if (!process_node->GetPropertyOrDefault(
              PropertyType::kMainThreadTaskLoadIsLow, 0)) {
        process_node->SetMainThreadTaskLoadIsLow(true);
      }
      FALLTHROUGH;
    case PageAlmostIdleData::LoadIdleState::kLoadedAndIdling:
    default:
      break;
  }

  // Run the loop until the state change occurs, as all of the above calls
  // simply caused messages to be posted.
  base::RunLoop run_loop;
  PageAlmostIdleObserverForTesting observer(&run_loop, page_node);
  run_loop.Run();

  DCHECK(page_node->page_almost_idle());
}

}  // namespace testing
}  // namespace performance_manager
