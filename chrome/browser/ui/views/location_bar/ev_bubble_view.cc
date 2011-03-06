// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/ev_bubble_view.h"

EVBubbleView::EVBubbleView(const int background_images[],
                           int contained_image,
                           const SkColor& color,
                           LocationBarView* location_bar)
    : IconLabelBubbleView(background_images, contained_image, color),
      ALLOW_THIS_IN_INITIALIZER_LIST(click_handler_(this, location_bar)) {
  SetElideInMiddle(true);
}

EVBubbleView::~EVBubbleView() {
}

bool EVBubbleView::OnMousePressed(const views::MouseEvent& event) {
  // We want to show the dialog on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void EVBubbleView::OnMouseReleased(const views::MouseEvent& event,
                                   bool canceled) {
  click_handler_.OnMouseReleased(event, canceled);
}

