// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/home_card_gesture_manager.h"

#include "athena/home/home_card_constants.h"
#include "ui/events/event.h"

namespace athena {

namespace {

// The maximum height, in pixels, of a home card with final state
// VISIBLE_MINIMIZED.
const int kMinimizedFinalStateMaxHeight = 50 + kHomeCardMinimizedHeight;

// The maximum height, in pixels, of an initially centered home card with final
// state VISIBLE_MINIMIZED.
const int kMinimizedFinalStateMaxHeightInitiallyCentered =
    90 + kHomeCardMinimizedHeight;

// The minimum height, as a percentage of the screen height, of a home card with
// final state VISIBLE_CENTERED.
const float kCenteredFinalStateMinScreenRatio = 0.5f;

// The minimum height, as a percentage of the screen height, of an initially
// minimized home card with final state VISIBLE_CENTERED.
const float kCenteredFinalStateMinScreenRatioInitiallyMinimized = 0.3f;

}

HomeCardGestureManager::HomeCardGestureManager(Delegate* delegate,
                                               const gfx::Rect& screen_bounds)
    : delegate_(delegate),
      original_state_(HomeCard::HIDDEN),
      y_offset_(0),
      last_estimated_height_(0),
      screen_bounds_(screen_bounds) {}

HomeCardGestureManager::~HomeCardGestureManager() {}

void HomeCardGestureManager::ProcessGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      y_offset_ = event->location().y();
      original_state_ = HomeCard::Get()->GetState();
      DCHECK_NE(HomeCard::HIDDEN, original_state_);
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_END:
      event->SetHandled();
      delegate_->OnGestureEnded(GetFinalState(), false);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      UpdateScrollState(*event);
      break;
    case ui::ET_SCROLL_FLING_START: {
      const ui::GestureEventDetails& details = event->details();
      const float kFlingCompletionVelocity = 100.0f;
      HomeCard::State final_state = GetFinalState();

      // When the user does not drag far enough to switch the final state, but
      // a fling happens at the end of the gesture, the state should change
      // based on the direction of the fling.
      // Checking |final_state| == |original_state| may cause unexpected results
      // for gestures where the user flings in the opposite direction that they
      // moved the home card (e.g. drag home card up from minimized state and
      // then fling down)
      // TODO(mukai): Consider this case once reported.
      bool is_fling = ::fabs(details.velocity_y()) > kFlingCompletionVelocity;
      if (final_state == original_state_ && is_fling) {
        if (details.velocity_y() > 0) {
          final_state = std::min(HomeCard::VISIBLE_MINIMIZED,
                                 static_cast<HomeCard::State>(final_state + 1));
        } else {
          final_state = std::max(HomeCard::VISIBLE_CENTERED,
                                 static_cast<HomeCard::State>(final_state - 1));
        }
      }
      delegate_->OnGestureEnded(final_state, is_fling);
      break;
    }
    default:
      // do nothing.
      break;
  }
}

HomeCard::State HomeCardGestureManager::GetFinalState() const {
  int max_height = (original_state_ == HomeCard::VISIBLE_CENTERED)
      ? kMinimizedFinalStateMaxHeightInitiallyCentered
      : kMinimizedFinalStateMaxHeight;
  if (last_estimated_height_ < max_height)
    return HomeCard::VISIBLE_MINIMIZED;

  float ratio = (original_state_ == HomeCard::VISIBLE_MINIMIZED)
      ? kCenteredFinalStateMinScreenRatioInitiallyMinimized
      : kCenteredFinalStateMinScreenRatio;
  if (last_estimated_height_ < screen_bounds_.height() * ratio)
    return HomeCard::VISIBLE_BOTTOM;

  return HomeCard::VISIBLE_CENTERED;
}

void HomeCardGestureManager::UpdateScrollState(const ui::GestureEvent& event) {
  last_estimated_height_ =
      screen_bounds_.height() - event.root_location().y() + y_offset_;

  if (last_estimated_height_ <= kHomeCardMinimizedHeight) {
    delegate_->OnGestureProgressed(
        HomeCard::VISIBLE_BOTTOM, HomeCard::VISIBLE_MINIMIZED, 1.0f);
    return;
  }

  HomeCard::State bigger_state = HomeCard::VISIBLE_BOTTOM;
  float smaller_height = kHomeCardMinimizedHeight;
  float bigger_height = kHomeCardHeight;
  if (last_estimated_height_ > kHomeCardHeight) {
    bigger_state = HomeCard::VISIBLE_CENTERED;
    smaller_height = kHomeCardHeight;
    bigger_height = screen_bounds_.height();
  }

  // The finger is between two states.
  float progress = (last_estimated_height_ - smaller_height) /
      (bigger_height - smaller_height);
  progress = std::min(1.0f, std::max(0.0f, progress));

  delegate_->OnGestureProgressed(
      static_cast<HomeCard::State>(bigger_state + 1),
      bigger_state,
      progress);
}

}  // namespace athena
