// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/home_card_gesture_manager.h"

#include "athena/home/home_card_constants.h"
#include "ui/events/event.h"

namespace athena {

HomeCardGestureManager::HomeCardGestureManager(Delegate* delegate,
                                               const gfx::Rect& screen_bounds)
    : delegate_(delegate),
      last_state_(HomeCard::Get()->GetState()),
      y_offset_(0),
      last_estimated_height_(0),
      screen_bounds_(screen_bounds) {}

HomeCardGestureManager::~HomeCardGestureManager() {}

void HomeCardGestureManager::ProcessGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      y_offset_ = event->location().y();
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_END:
      event->SetHandled();
      delegate_->OnGestureEnded(GetClosestState());
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      UpdateScrollState(*event);
      break;
    case ui::ET_SCROLL_FLING_START: {
      const ui::GestureEventDetails& details = event->details();
      const float kFlingCompletionVelocity = 100.0f;
      if (::fabs(details.velocity_y()) > kFlingCompletionVelocity) {
        int step = (details.velocity_y() > 0) ? 1 : -1;
        int new_state = static_cast<int>(last_state_) + step;
        if (new_state >= HomeCard::VISIBLE_CENTERED &&
            new_state <= HomeCard::VISIBLE_MINIMIZED) {
          last_state_ = static_cast<HomeCard::State>(new_state);
        }
        delegate_->OnGestureEnded(last_state_);
      }
      break;
    }
    default:
      // do nothing.
      break;
  }
}

HomeCard::State HomeCardGestureManager::GetClosestState() const {
  // The top position of the bounds for one smaller state than the current
  // one.

  if (last_estimated_height_ <= kHomeCardMinimizedHeight)
    return HomeCard::VISIBLE_MINIMIZED;

  HomeCard::State smaller_state = HomeCard::VISIBLE_MINIMIZED;
  int smaller_height = kHomeCardMinimizedHeight;
  int bigger_height = kHomeCardHeight;
  if (last_estimated_height_ > kHomeCardHeight) {
    smaller_state = HomeCard::VISIBLE_BOTTOM;
    smaller_height = kHomeCardHeight;
    bigger_height = screen_bounds_.height();
  }

  if (last_estimated_height_ - smaller_height <=
      (bigger_height - smaller_height) / 5) {
    return smaller_state;
  }

  return static_cast<HomeCard::State>(smaller_state - 1);
}

void HomeCardGestureManager::UpdateScrollState(const ui::GestureEvent& event) {
  last_estimated_height_ =
      screen_bounds_.height() - event.root_location().y() + y_offset_;

  if (last_estimated_height_ <= kHomeCardMinimizedHeight) {
    delegate_->OnGestureProgressed(
        last_state_, HomeCard::VISIBLE_MINIMIZED, 1.0f);
    last_state_ = HomeCard::VISIBLE_MINIMIZED;
    return;
  }

  HomeCard::State state = HomeCard::VISIBLE_BOTTOM;
  float smaller_height = kHomeCardMinimizedHeight;
  float bigger_height = kHomeCardHeight;
  if (last_estimated_height_ > kHomeCardHeight) {
    state = HomeCard::VISIBLE_CENTERED;
    smaller_height = kHomeCardHeight;
    bigger_height = screen_bounds_.height();
  }

  // The finger is between two states.
  float progress = (last_estimated_height_ - smaller_height) /
      (bigger_height - smaller_height);

  if (last_state_ == state) {
    if (event.details().scroll_y() > 0) {
      state = static_cast<HomeCard::State>(state + 1);
      progress = 1.0f - progress;
    } else {
      last_state_ = static_cast<HomeCard::State>(last_state_ + 1);
    }
  }
  delegate_->OnGestureProgressed(last_state_, state, progress);
  last_state_ = state;
}

}  // namespace athena
