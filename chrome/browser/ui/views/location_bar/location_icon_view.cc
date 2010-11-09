// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/location_bar/location_icon_view.h"

LocationIconView::LocationIconView(const LocationBarView* location_bar)
    : ALLOW_THIS_IN_INITIALIZER_LIST(click_handler_(this, location_bar)) {
}

LocationIconView::~LocationIconView() {
}

bool LocationIconView::OnMousePressed(const views::MouseEvent& event) {
  // We want to show the dialog on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void LocationIconView::OnMouseReleased(const views::MouseEvent& event,
                                       bool canceled) {
  click_handler_.OnMouseReleased(event, canceled);
}
