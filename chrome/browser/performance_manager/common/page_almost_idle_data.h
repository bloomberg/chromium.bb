// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_COMMON_PAGE_ALMOST_IDLE_DATA_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_COMMON_PAGE_ALMOST_IDLE_DATA_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace performance_manager {

namespace testing {
class PageAlmostIdleDecoratorTestUtils;
}  // namespace testing

// Data storage for the PageAlmostIdleDecorator. An instance of this class is
// stored internally in each PageNodeImpl, but is only accessible to a
// PageAlmostIdleDecorator. An object of this type will come into existence as
// a page starts a main frame navigation, and it will be torn down once it has
// worked through the state machine and transitioned to "loaded and idle".
// TODO(chrisha): Fold this back into private impl details of
// PageAlmostIdleDecorator once the NodeAttachedData solution is in place.
class PageAlmostIdleData {
 public:
  PageAlmostIdleData();
  ~PageAlmostIdleData();

 protected:
  friend class PageAlmostIdleDecorator;
  friend class testing::PageAlmostIdleDecoratorTestUtils;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(PageAlmostIdleData);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_COMMON_PAGE_ALMOST_IDLE_DATA_H_
