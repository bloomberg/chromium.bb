// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/actionable_view.h"

#include "ash/ash_constants.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/gfx/canvas.h"

namespace ash {
namespace internal {

ActionableView::ActionableView()
    : has_capture_(false) {
  set_focusable(true);
}

ActionableView::~ActionableView() {
}

void ActionableView::DrawBorder(gfx::Canvas* canvas, const gfx::Rect& bounds) {
  gfx::Rect rect = bounds;
  rect.Inset(1, 1, 3, 3);
  canvas->DrawRect(rect, kFocusBorderColor);
}

bool ActionableView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    return PerformAction(event);
  }
  return false;
}

bool ActionableView::OnMousePressed(const ui::MouseEvent& event) {
  // Return true so that this view starts capturing the events.
  has_capture_ = true;
  return true;
}

void ActionableView::OnMouseReleased(const ui::MouseEvent& event) {
  if (has_capture_ && GetLocalBounds().Contains(event.location()))
    PerformAction(event);
}

void ActionableView::OnMouseCaptureLost() {
  has_capture_ = false;
}

void ActionableView::SetAccessibleName(const string16& name) {
  accessible_name_ = name;
}

void ActionableView::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable()))
    DrawBorder(canvas, GetLocalBounds());
}

void ActionableView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP && PerformAction(*event))
    event->SetHandled();
}

void ActionableView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
  state->name = accessible_name_;
}

}  // namespace internal
}  // namespace ash
