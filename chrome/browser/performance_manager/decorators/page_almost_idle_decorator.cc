// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/decorators/page_almost_idle_decorator.h"

#include <algorithm>

#include "chrome/browser/performance_manager/resource_coordinator_clock.h"

namespace performance_manager {

// static
constexpr base::TimeDelta PageAlmostIdleDecorator::kLoadedAndIdlingTimeout;
// static
constexpr base::TimeDelta PageAlmostIdleDecorator::kWaitingForIdleTimeout;

PageAlmostIdleDecorator::PageAlmostIdleDecorator() {
  // Ensure the timeouts make sense relative to each other.
  static_assert(kWaitingForIdleTimeout > kLoadedAndIdlingTimeout,
                "timeouts must be well ordered");
}

PageAlmostIdleDecorator::~PageAlmostIdleDecorator() = default;

bool PageAlmostIdleDecorator::ShouldObserve(const NodeBase* node) {
  switch (node->id().type) {
    case resource_coordinator::CoordinationUnitType::kFrame:
    case resource_coordinator::CoordinationUnitType::kPage:
    case resource_coordinator::CoordinationUnitType::kProcess:
      return true;

    default:
      return false;
  }
  NOTREACHED();
}

void PageAlmostIdleDecorator::OnProcessPropertyChanged(
    ProcessNodeImpl* process_node,
    resource_coordinator::mojom::PropertyType property_type,
    int64_t value) {
  if (property_type ==
      resource_coordinator::mojom::PropertyType::kMainThreadTaskLoadIsLow) {
    UpdateLoadIdleStateProcess(process_node);
  }
}

void PageAlmostIdleDecorator::OnPageEventReceived(
    PageNodeImpl* page_node,
    resource_coordinator::mojom::Event event) {
  // Only the navigation committed event is of interest.
  if (event != resource_coordinator::mojom::Event::kNavigationCommitted)
    return;

  // Reset the load-idle state associated with this page as a new navigation has
  // started.
  auto* data = GetOrCreateData(page_node);
  data->load_idle_state_ = LoadIdleState::kLoadingNotStarted;
  PageNodeImpl::PageAlmostIdleHelper::set_page_almost_idle(page_node, false);
  UpdateLoadIdleStatePage(page_node);
}

void PageAlmostIdleDecorator::OnNetworkAlmostIdleChanged(
    FrameNodeImpl* frame_node) {
  UpdateLoadIdleStateFrame(frame_node);
}

void PageAlmostIdleDecorator::OnIsLoadingChanged(PageNodeImpl* page_node) {
  UpdateLoadIdleStatePage(page_node);
}

void PageAlmostIdleDecorator::UpdateLoadIdleStateFrame(
    FrameNodeImpl* frame_node) {
  // Only main frames are relevant in the load idle state.
  if (!frame_node->IsMainFrame())
    return;

  // Update the load idle state of the page associated with this frame.
  auto* page_node = frame_node->GetPageNode();
  if (!page_node)
    return;
  UpdateLoadIdleStatePage(page_node);
}

void PageAlmostIdleDecorator::UpdateLoadIdleStatePage(PageNodeImpl* page_node) {
  // Once the cycle is complete state transitions are no longer tracked for this
  // page. When this occurs the backing data store is deleted.
  auto* data = GetData(page_node);
  if (data == nullptr)
    return;

  // This is the terminal state, so should never occur.
  DCHECK_NE(LoadIdleState::kLoadedAndIdle, data->load_idle_state_);

  // Cancel any ongoing timers. A new timer will be set if necessary.
  data->idling_timer_.Stop();
  const base::TimeTicks now = ResourceCoordinatorClock::NowTicks();

  // Determine if the overall timeout has fired.
  if ((data->load_idle_state_ == LoadIdleState::kLoadedNotIdling ||
       data->load_idle_state_ == LoadIdleState::kLoadedAndIdling) &&
      (now - data->loading_stopped_) >= kWaitingForIdleTimeout) {
    TransitionToLoadedAndIdle(page_node);
    return;
  }

  // Otherwise do normal state transitions.
  switch (data->load_idle_state_) {
    case LoadIdleState::kLoadingNotStarted: {
      if (!page_node->is_loading())
        return;
      data->load_idle_state_ = LoadIdleState::kLoading;
      return;
    }

    case LoadIdleState::kLoading: {
      if (page_node->is_loading())
        return;
      data->load_idle_state_ = LoadIdleState::kLoadedNotIdling;
      data->loading_stopped_ = now;
      // Let the kLoadedNotIdling state transition evaluate, allowing an
      // effective transition directly from kLoading to kLoadedAndIdling.
      FALLTHROUGH;
    }

    case LoadIdleState::kLoadedNotIdling: {
      if (IsIdling(page_node)) {
        data->load_idle_state_ = LoadIdleState::kLoadedAndIdling;
        data->idling_started_ = now;
      }
      // Break out of the switch statement and set a timer to check for the
      // next state transition.
      break;
    }

    case LoadIdleState::kLoadedAndIdling: {
      // If the page is not still idling then transition back a state.
      if (!IsIdling(page_node)) {
        data->load_idle_state_ = LoadIdleState::kLoadedNotIdling;
      } else {
        // Idling has been happening long enough so make the last state
        // transition.
        if (now - data->idling_started_ >= kLoadedAndIdlingTimeout) {
          TransitionToLoadedAndIdle(page_node);
          return;
        }
      }
      // Break out of the switch statement and set a timer to check for the
      // next state transition.
      break;
    }

    // This should never occur.
    case LoadIdleState::kLoadedAndIdle:
      NOTREACHED();
  }

  // Getting here means a new timer needs to be set. Use the nearer of the two
  // applicable timeouts.
  base::TimeDelta timeout =
      (data->loading_stopped_ + kWaitingForIdleTimeout) - now;
  if (data->load_idle_state_ == LoadIdleState::kLoadedAndIdling) {
    timeout = std::min(timeout,
                       (data->idling_started_ + kLoadedAndIdlingTimeout) - now);
  }

  // It's safe to use base::Unretained here because the graph owns the timer via
  // PageNodeImpl, and all nodes are destroyed *before* this observer during
  // tear down. By the time the observer is destroyed, the timer will have
  // already been destroyed and the associated posted task canceled.
  data->idling_timer_.Start(
      FROM_HERE, timeout,
      base::BindRepeating(&PageAlmostIdleDecorator::UpdateLoadIdleStatePage,
                          base::Unretained(this), page_node));
}

void PageAlmostIdleDecorator::UpdateLoadIdleStateProcess(
    ProcessNodeImpl* process_node) {
  for (auto* frame_node : process_node->GetFrameNodes())
    UpdateLoadIdleStateFrame(frame_node);
}

void PageAlmostIdleDecorator::TransitionToLoadedAndIdle(
    PageNodeImpl* page_node) {
  auto* data = GetData(page_node);
  data->load_idle_state_ = LoadIdleState::kLoadedAndIdle;
  PageNodeImpl::PageAlmostIdleHelper::set_page_almost_idle(page_node, true);

  // Destroy the metadata as there are no more transitions possible. The
  // machinery will start up again if a navigation occurs.
  PageNodeImpl::PageAlmostIdleHelper::DestroyData(page_node);
}

// static
PageAlmostIdleData* PageAlmostIdleDecorator::GetOrCreateData(
    PageNodeImpl* page_node) {
  return PageNodeImpl::PageAlmostIdleHelper::GetOrCreateData(page_node);
}

// static
PageAlmostIdleData* PageAlmostIdleDecorator::GetData(PageNodeImpl* page_node) {
  return PageNodeImpl::PageAlmostIdleHelper::GetData(page_node);
}

// static
bool PageAlmostIdleDecorator::IsIdling(const PageNodeImpl* page_node) {
  // Get the Frame CU for the main frame associated with this page.
  const FrameNodeImpl* main_frame_node = page_node->GetMainFrameNode();
  if (!main_frame_node)
    return false;

  // Get the process CU associated with this main frame.
  const auto* process_node = main_frame_node->GetProcessNode();
  if (!process_node)
    return false;

  // Note that it's possible for one misbehaving frame hosted in the same
  // process as this page's main frame to keep the main thread task low high.
  // In this case the IsIdling signal will be delayed, despite the task load
  // associated with this page's main frame actually being low. In the case
  // of session restore this is mitigated by having a timeout while waiting for
  // this signal.
  return main_frame_node->network_almost_idle() &&
         process_node->GetPropertyOrDefault(
             resource_coordinator::mojom::PropertyType::
                 kMainThreadTaskLoadIsLow,
             0u);
}

}  // namespace performance_manager
