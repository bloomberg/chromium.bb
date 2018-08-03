// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/cast_system_gesture_dispatcher.h"

#include "base/logging.h"

namespace chromecast {

CastSystemGestureDispatcher::CastSystemGestureDispatcher() {}

CastSystemGestureDispatcher::~CastSystemGestureDispatcher() {
  DCHECK(gesture_handlers_.empty());
}

void CastSystemGestureDispatcher::AddGestureHandler(
    CastGestureHandler* handler) {
  gesture_handlers_.insert(handler);
}

void CastSystemGestureDispatcher::RemoveGestureHandler(
    CastGestureHandler* handler) {
  gesture_handlers_.erase(handler);
}

bool CastSystemGestureDispatcher::CanHandleSwipe(
    CastSideSwipeOrigin swipe_origin) {
  for (auto* gesture_handler : gesture_handlers_) {
    if (gesture_handler->CanHandleSwipe(swipe_origin)) {
      return true;
    }
  }
  return false;
}

void CastSystemGestureDispatcher::HandleSideSwipeBegin(
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    if (gesture_handler->CanHandleSwipe(swipe_origin)) {
      gesture_handler->HandleSideSwipeBegin(swipe_origin, touch_location);
    }
  }
}
void CastSystemGestureDispatcher::HandleSideSwipeContinue(
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    if (gesture_handler->CanHandleSwipe(swipe_origin)) {
      gesture_handler->HandleSideSwipeContinue(swipe_origin, touch_location);
    }
  }
}
void CastSystemGestureDispatcher::HandleSideSwipeEnd(
    CastSideSwipeOrigin swipe_origin,
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    if (gesture_handler->CanHandleSwipe(swipe_origin)) {
      gesture_handler->HandleSideSwipeEnd(swipe_origin, touch_location);
    }
  }
}

void CastSystemGestureDispatcher::HandleTapDownGesture(
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    gesture_handler->HandleTapDownGesture(touch_location);
  }
}

void CastSystemGestureDispatcher::HandleTapGesture(
    const gfx::Point& touch_location) {
  for (auto* gesture_handler : gesture_handlers_) {
    gesture_handler->HandleTapGesture(touch_location);
  }
}

}  // namespace chromecast