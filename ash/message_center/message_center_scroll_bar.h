// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MESSAGE_CENTER_MESSAGE_CENTER_SCROLL_BAR_H_
#define ASH_MESSAGE_CENTER_MESSAGE_CENTER_SCROLL_BAR_H_

#include "ui/events/event.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"

namespace ash {

// The scroll bar for message center. This is basically views::OverlayScrollBar
// but also records the metrics for the type of scrolling. Only the first event
// after the message center opens is recorded.
class MessageCenterScrollBar : public views::OverlayScrollBar {
 public:
  MessageCenterScrollBar();

 private:
  // View overrides:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;

  // ui::EventHandler overrides:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // False if no event is recorded yet. True if the first event is recorded.
  bool stats_recorded_ = false;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterScrollBar);
};

}  // namespace ash

#endif  // ASH_MESSAGE_CENTER_MESSAGE_CENTER_SCROLL_BAR_H_
