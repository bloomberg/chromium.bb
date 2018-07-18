// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/message_center/message_center_scroll_bar.h"

#include "base/metrics/histogram_macros.h"

namespace {

enum class ScrollActionReason {
  kUnknown,
  kByMouseWheel,
  kByTouch,
  kByArrowKey,
  kCount,
};

void CollectScrollActionReason(ScrollActionReason reason) {
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.MessageCenter.ScrollActionReason", reason,
                            ScrollActionReason::kCount);
}

}  // namespace

namespace ash {

MessageCenterScrollBar::MessageCenterScrollBar()
    : views::OverlayScrollBar(false) {}

bool MessageCenterScrollBar::OnKeyPressed(const ui::KeyEvent& event) {
  if (!stats_recorded_ &&
      (event.key_code() == ui::VKEY_UP || event.key_code() == ui::VKEY_DOWN)) {
    CollectScrollActionReason(ScrollActionReason::kByMouseWheel);
    stats_recorded_ = true;
  }
  return views::OverlayScrollBar::OnKeyPressed(event);
}

bool MessageCenterScrollBar::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (!stats_recorded_) {
    CollectScrollActionReason(ScrollActionReason::kByMouseWheel);
    stats_recorded_ = true;
  }
  return views::OverlayScrollBar::OnMouseWheel(event);
}

void MessageCenterScrollBar::OnGestureEvent(ui::GestureEvent* event) {
  if (!stats_recorded_ && (event->type() == ui::ET_GESTURE_SCROLL_BEGIN)) {
    CollectScrollActionReason(ScrollActionReason::kByTouch);
    stats_recorded_ = true;
  }
  return views::OverlayScrollBar::OnGestureEvent(event);
}

}  // namespace ash
