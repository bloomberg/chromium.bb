// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/actionable_view.h"

#include "ash/common/ash_constants.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"

namespace ash {

// static
const char ActionableView::kViewClassName[] = "tray/ActionableView";

ActionableView::ActionableView(SystemTrayItem* owner,
                               TrayPopupInkDropStyle ink_drop_style)
    : views::CustomButton(this),
      destroyed_(nullptr),
      owner_(owner),
      ink_drop_style_(ink_drop_style) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_ink_drop_base_color(kTrayPopupInkDropBaseColor);
  set_ink_drop_visible_opacity(kTrayPopupInkDropRippleOpacity);
  set_has_ink_drop_action_on_click(false);
  set_notify_enter_exit_on_child(true);
}

ActionableView::~ActionableView() {
  if (destroyed_)
    *destroyed_ = true;
}

void ActionableView::OnPaintFocus(gfx::Canvas* canvas) {
  gfx::RectF rect(GetFocusBounds());
  canvas->DrawSolidFocusRect(rect, kFocusBorderColor, kFocusBorderThickness);
}

gfx::Rect ActionableView::GetFocusBounds() {
  return GetLocalBounds();
}

void ActionableView::HandlePerformActionResult(bool action_performed,
                                               const ui::Event& event) {
  AnimateInkDrop(action_performed ? views::InkDropState::ACTION_TRIGGERED
                                  : views::InkDropState::HIDDEN,
                 ui::LocatedEvent::FromIfValid(&event));
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

void ActionableView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_BUTTON;
  node_data->SetName(accessible_name_);
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

std::unique_ptr<views::InkDrop> ActionableView::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(ink_drop_style_, this);
}

std::unique_ptr<views::InkDropRipple> ActionableView::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      ink_drop_style_, this, GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight>
ActionableView::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(ink_drop_style_, this);
}

std::unique_ptr<views::InkDropMask> ActionableView::CreateInkDropMask() const {
  return TrayPopupUtils::CreateInkDropMask(ink_drop_style_, this);
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

  HandlePerformActionResult(action_performed, event);
}

ButtonListenerActionableView::ButtonListenerActionableView(
    SystemTrayItem* owner,
    TrayPopupInkDropStyle ink_drop_style,
    views::ButtonListener* listener)
    : ActionableView(owner, ink_drop_style), listener_(listener) {}

bool ButtonListenerActionableView::PerformAction(const ui::Event& event) {
  listener_->ButtonPressed(this, event);
  return true;
}

}  // namespace ash
