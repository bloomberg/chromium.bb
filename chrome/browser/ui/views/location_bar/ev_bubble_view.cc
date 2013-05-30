// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/ev_bubble_view.h"


EVBubbleView::EVBubbleView(const int background_images[],
                           int contained_image,
                           const gfx::Font& font,
                           int font_y_offset,
                           SkColor color,
                           LocationBarView* location_bar)
    : IconLabelBubbleView(background_images, contained_image, font,
                          font_y_offset, color, true),
      page_info_helper_(this, location_bar) {
}

EVBubbleView::~EVBubbleView() {
}

gfx::Size EVBubbleView::GetMinimumSize() {
  // Height will be ignored by the LocationBarView.
  gfx::Size minimum(GetPreferredSize());
  static const int kMinBubbleWidth = 150;
  minimum.SetToMax(gfx::Size(kMinBubbleWidth, 0));
  return minimum;
}

bool EVBubbleView::OnMousePressed(const ui::MouseEvent& event) {
  // We want to show the dialog on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void EVBubbleView::OnMouseReleased(const ui::MouseEvent& event) {
  page_info_helper_.ProcessEvent(event);
}

void EVBubbleView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    page_info_helper_.ProcessEvent(*event);
    event->SetHandled();
  }
}
