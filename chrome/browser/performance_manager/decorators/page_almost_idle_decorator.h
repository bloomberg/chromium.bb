// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_ALMOST_IDLE_DECORATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_ALMOST_IDLE_DECORATOR_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/graph/node_base.h"
#include "chrome/browser/performance_manager/graph/page_node_impl.h"
#include "chrome/browser/performance_manager/graph/process_node_impl.h"
#include "chrome/browser/performance_manager/observers/graph_observer.h"

namespace performance_manager {

// The PageAlmostIdle decorator is responsible for determining when a page has
// reached an "almost idle" state after initial load, based on CPU and network
// quiescence, as well as an absolute timeout. This state is then updated on
// PageNodes in a graph.
class PageAlmostIdleDecorator : public GraphObserver {
 public:
  class Data;

  PageAlmostIdleDecorator();
  ~PageAlmostIdleDecorator() override;

  // GraphObserver implementation:
  void OnRegistered() override;
  bool ShouldObserve(const NodeBase* node) override;
  void OnNetworkAlmostIdleChanged(FrameNodeImpl* frame_node) override;
  void OnIsLoadingChanged(PageNodeImpl* page_node) override;
  void OnMainFrameNavigationCommitted(PageNodeImpl* page_node) override;
  void OnMainThreadTaskLoadIsLow(ProcessNodeImpl* process_node) override;

 protected:
  friend class PageAlmostIdleDecoratorTest;

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

  static bool IsIdling(const PageNodeImpl* page_node);

 private:
  DISALLOW_COPY_AND_ASSIGN(PageAlmostIdleDecorator);
};

class PageAlmostIdleDecorator::Data {
 public:
  // The state transitions for the PageAlmostIdle signal. In general a page
  // transitions through these states from top to bottom.
  enum class LoadIdleState {
    // The initial state. Can only transition to kLoading from here.
    kLoadingNotStarted,
    // Loading has started. Almost idle signals are ignored in this state.
    // Can transition to kLoadedNotIdling and kLoadedAndIdling from here.
    kLoading,
    // Loading has completed, but the page has not started idling. Can only
    // transition to kLoadedAndIdling from here.
    kLoadedNotIdling,
    // Loading has completed, and the page is idling. Can transition to
    // kLoadedNotIdling or kLoadedAndIdle from here.
    kLoadedAndIdling,
    // Loading has completed and the page has been idling for sufficiently long.
    // This is the final state. Once this state has been reached a signal will
    // be emitted and no further state transitions will be tracked. Committing a
    // new non-same document navigation can start the cycle over again.
    kLoadedAndIdle
  };

  static Data* GetOrCreateForTesting(PageNodeImpl* page_node);
  static Data* GetForTesting(PageNodeImpl* page_node);
  static bool DestroyForTesting(PageNodeImpl* page_node);

  // Marks the point in time when the DidStopLoading signal was received,
  // transitioning to kLoadedAndNotIdling or kLoadedAndIdling. This is used as
  // the basis for the kWaitingForIdleTimeout.
  base::TimeTicks loading_stopped_;

  // Marks the point in time when the last transition to kLoadedAndIdling
  // occurred. Used for gating the transition to kLoadedAndIdle.
  base::TimeTicks idling_started_;

  // A one-shot timer used for transitioning between kLoadedAndIdling and
  // kLoadedAndIdle.
  base::OneShotTimer idling_timer_;

  // Initially at kLoadingNotStarted. Transitions through the states via calls
  // to UpdateLoadIdleState. Is reset to kLoadingNotStarted when a non-same
  // document navigation is committed.
  LoadIdleState load_idle_state_ = LoadIdleState::kLoadingNotStarted;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_DECORATORS_PAGE_ALMOST_IDLE_DECORATOR_H_
