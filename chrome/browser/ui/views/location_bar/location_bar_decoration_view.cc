// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_decoration_view.h"

#include "ui/events/event.h"

LocationBarDecorationView::LocationBarDecorationView()
    : could_handle_click_(true) {
  SetAccessibilityFocusable(true);
}

LocationBarDecorationView::~LocationBarDecorationView() {}

bool LocationBarDecorationView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location())) {
    // Do nothing until mouse is released.
    could_handle_click_ = CanHandleClick();
    return true;
  }

  return false;
}

void LocationBarDecorationView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location()) &&
      could_handle_click_ && CanHandleClick()) {
    OnClick();
  }
}

bool LocationBarDecorationView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    OnClick();
    return true;
  }

  return false;
}

void LocationBarDecorationView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    OnClick();
    event->SetHandled();
  }
}

bool LocationBarDecorationView::CanHandleClick() const {
  return true;
}
