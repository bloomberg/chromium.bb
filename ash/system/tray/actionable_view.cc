// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/actionable_view.h"

#include "ash/ash_constants.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/gfx/canvas.h"

namespace ash {

// static
const char ActionableView::kViewClassName[] = "tray/ActionableView";

ActionableView::ActionableView()
    : has_capture_(false) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

ActionableView::~ActionableView() {
}

void ActionableView::OnPaintFocus(gfx::Canvas* canvas) {
  gfx::Rect rect(GetFocusBounds());
  rect.Inset(1, 1, 3, 2);
  canvas->DrawSolidFocusRect(rect, kFocusBorderColor);
}

gfx::Rect ActionableView::GetFocusBounds() {
  return GetLocalBounds();
}

const char* ActionableView::GetClassName() const {
  return kViewClassName;
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

void ActionableView::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

void ActionableView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
  state->name = accessible_name_;
}

void ActionableView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (HasFocus())
    OnPaintFocus(canvas);
}

void ActionableView::OnFocus() {
  View::OnFocus();
  // We render differently when focused.
  SchedulePaint();
}

void ActionableView::OnBlur() {
  View::OnBlur();
  // We render differently when focused.
  SchedulePaint();
}

void ActionableView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP && PerformAction(*event))
    event->SetHandled();
}

}  // namespace ash
