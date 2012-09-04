// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/ev_bubble_view.h"

EVBubbleView::EVBubbleView(const int background_images[],
                           int contained_image,
                           SkColor color,
                           LocationBarView* location_bar)
    : IconLabelBubbleView(background_images, contained_image, color),
      ALLOW_THIS_IN_INITIALIZER_LIST(page_info_helper_(this, location_bar)) {
  SetElideInMiddle(true);
}

EVBubbleView::~EVBubbleView() {
}

bool EVBubbleView::OnMousePressed(const ui::MouseEvent& event) {
  // We want to show the dialog on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void EVBubbleView::OnMouseReleased(const ui::MouseEvent& event) {
  page_info_helper_.ProcessEvent(event);
}

ui::EventResult EVBubbleView::OnGestureEvent(
    const ui::GestureEvent& event) {
  if (event.type() == ui::ET_GESTURE_TAP) {
    page_info_helper_.ProcessEvent(event);
    return ui::ER_CONSUMED;
  }
  return ui::ER_UNHANDLED;
}
