// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/actionable_view.h"

#include "ash/common/ash_constants.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"

namespace ash {

// static
const char ActionableView::kViewClassName[] = "tray/ActionableView";

ActionableView::ActionableView(SystemTrayItem* owner)
    : views::CustomButton(this), destroyed_(nullptr), owner_(owner) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_has_ink_drop_action_on_click(false);
}

ActionableView::~ActionableView() {
  if (destroyed_)
    *destroyed_ = true;
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
  if (state() != STATE_DISABLED && event.key_code() == ui::VKEY_SPACE) {
    NotifyClick(event);
    return true;
  }
  return CustomButton::OnKeyPressed(event);
}

void ActionableView::SetAccessibleName(const base::string16& name) {
  accessible_name_ = name;
}

void ActionableView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
  state->name = accessible_name_;
}

void ActionableView::OnPaint(gfx::Canvas* canvas) {
  CustomButton::OnPaint(canvas);
  if (HasFocus())
    OnPaintFocus(canvas);
}

void ActionableView::OnFocus() {
  CustomButton::OnFocus();
  // We render differently when focused.
  SchedulePaint();
}

void ActionableView::OnBlur() {
  CustomButton::OnBlur();
  // We render differently when focused.
  SchedulePaint();
}

void ActionableView::CloseSystemBubble() {
  DCHECK(owner_);
  owner_->system_tray()->CloseSystemBubble();
}

void ActionableView::ButtonPressed(Button* sender, const ui::Event& event) {
  bool destroyed = false;
  destroyed_ = &destroyed;
  const bool action_performed = PerformAction(event);
  if (destroyed)
    return;
  destroyed_ = nullptr;

  if (action_performed) {
    AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED,
                   ui::LocatedEvent::FromIfValid(&event));
  } else {
    AnimateInkDrop(views::InkDropState::HIDDEN,
                   ui::LocatedEvent::FromIfValid(&event));
  }
}

}  // namespace ash
