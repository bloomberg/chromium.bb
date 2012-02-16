// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/partial_screenshot_event_filter.h"

#include "ash/wm/partial_screenshot_view.h"

namespace ash {
namespace internal {

PartialScreenshotEventFilter::PartialScreenshotEventFilter()
    : view_(NULL) {
}

PartialScreenshotEventFilter::~PartialScreenshotEventFilter() {
  view_ = NULL;
}

bool PartialScreenshotEventFilter::PreHandleKeyEvent(
    aura::Window* target, aura::KeyEvent* event) {
  if (!view_)
    return false;

  if (event->key_code() == ui::VKEY_ESCAPE)
    view_->Cancel();

  // Always handled: other windows shouldn't receive input while we're
  // taking a screenshot.
  return true;
}

bool PartialScreenshotEventFilter::PreHandleMouseEvent(
    aura::Window* target, aura::MouseEvent* event) {
  return false;  // Not handled.
}

ui::TouchStatus PartialScreenshotEventFilter::PreHandleTouchEvent(
    aura::Window* target, aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;  // Not handled.
}

ui::GestureStatus PartialScreenshotEventFilter::PreHandleGestureEvent(
    aura::Window* target, aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;  // Not handled.
}

void PartialScreenshotEventFilter::Activate(PartialScreenshotView* view) {
  view_ = view;
}

void PartialScreenshotEventFilter::Deactivate() {
  view_ = NULL;
}

}  // namespace internal
}  // namespace ash
