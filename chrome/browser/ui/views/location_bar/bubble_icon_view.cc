// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"

#include "chrome/browser/command_updater.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/animation/button_ink_drop_delegate.h"
#include "ui/views/bubble/bubble_delegate.h"

BubbleIconView::BubbleIconView(CommandUpdater* command_updater, int command_id)
    : image_(new views::ImageView()),
      command_updater_(command_updater),
      command_id_(command_id),
      active_(false),
      suppress_mouse_released_action_(false),
      ink_drop_delegate_(new views::ButtonInkDropDelegate(this, this)) {
  AddChildView(image_);
  image_->set_interactive(false);
  image_->EnableCanvasFlippingForRTLUI(true);
  image_->SetAccessibilityFocusable(true);

  // TODO(varkha): Provide standard ink drop dimensions in LayoutConstants.
  const int kInkDropLargeSize = 32;
  const int kInkDropLargeCornerRadius = 5;
  const int kInkDropSmallSize = 24;
  const int kInkDropSmallCornerRadius = 2;
  ink_drop_delegate_->SetInkDropSize(
      kInkDropLargeSize, kInkDropLargeCornerRadius, kInkDropSmallSize,
      kInkDropSmallCornerRadius);
}

BubbleIconView::~BubbleIconView() {
}

bool BubbleIconView::IsBubbleShowing() const {
  // If the bubble is being destroyed, it's considered showing though it may be
  // already invisible currently.
  return GetBubble() != NULL;
}

void BubbleIconView::SetImage(const gfx::ImageSkia* image_skia) {
  image_->SetImage(image_skia);
}

const gfx::ImageSkia& BubbleIconView::GetImage() const {
  return image_->GetImage();
}

void BubbleIconView::SetTooltipText(const base::string16& tooltip) {
  image_->SetTooltipText(tooltip);
}

void BubbleIconView::GetAccessibleState(ui::AXViewState* state) {
  image_->GetAccessibleState(state);
  state->role = ui::AX_ROLE_BUTTON;
}

bool BubbleIconView::GetTooltipText(const gfx::Point& p,
                                    base::string16* tooltip) const {
  return !IsBubbleShowing() && image_->GetTooltipText(p, tooltip);
}

gfx::Size BubbleIconView::GetPreferredSize() const {
  return image_->GetPreferredSize();
}

void BubbleIconView::Layout() {
  View::Layout();
  image_->SetBoundsRect(GetLocalBounds());
  ink_drop_delegate_->OnLayout();
}

bool BubbleIconView::OnMousePressed(const ui::MouseEvent& event) {
  // If the bubble is showing then don't reshow it when the mouse is released.
  suppress_mouse_released_action_ = IsBubbleShowing();
  if (!suppress_mouse_released_action_ && event.IsOnlyLeftMouseButton())
    ink_drop_delegate_->OnAction(views::InkDropState::ACTION_PENDING);

  // We want to show the bubble on mouse release; that is the standard behavior
  // for buttons.
  return true;
}

void BubbleIconView::OnMouseReleased(const ui::MouseEvent& event) {
  // If this is the second click on this view then the bubble was showing on the
  // mouse pressed event and is hidden now. Prevent the bubble from reshowing by
  // doing nothing here.
  if (suppress_mouse_released_action_) {
    suppress_mouse_released_action_ = false;
    return;
  }
  if (!event.IsLeftMouseButton())
    return;

  const bool activated = HitTestPoint(event.location());
  ink_drop_delegate_->OnAction(
      activated ? views::InkDropState::ACTIVATED : views::InkDropState::HIDDEN);
  if (activated)
    ExecuteCommand(EXECUTE_SOURCE_MOUSE);
}

bool BubbleIconView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_SPACE && event.key_code() != ui::VKEY_RETURN)
    return false;

  ink_drop_delegate_->OnAction(views::InkDropState::ACTIVATED);
  ExecuteCommand(EXECUTE_SOURCE_KEYBOARD);
  return true;
}

void BubbleIconView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  View::ViewHierarchyChanged(details);
  if (details.is_add && GetNativeTheme())
    UpdateIcon();
}

void BubbleIconView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateIcon();
}

void BubbleIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    ink_drop_delegate_->OnAction(views::InkDropState::ACTIVATED);
    ExecuteCommand(EXECUTE_SOURCE_GESTURE);
    event->SetHandled();
  }
}

void BubbleIconView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  image_->SetPaintToLayer(true);
  image_->SetFillsBoundsOpaquely(false);
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(false);
  layer()->Add(ink_drop_layer);
  layer()->StackAtBottom(ink_drop_layer);
}

void BubbleIconView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  layer()->Remove(ink_drop_layer);
  SetFillsBoundsOpaquely(true);
  SetPaintToLayer(false);
  image_->SetFillsBoundsOpaquely(true);
  image_->SetPaintToLayer(false);
}

void BubbleIconView::OnWidgetDestroying(views::Widget* widget) {
  widget->RemoveObserver(this);
}

void BubbleIconView::OnWidgetVisibilityChanged(views::Widget* widget,
                                               bool visible) {
  // |widget| is a bubble that has just got shown / hidden.
  if (!visible)
    ink_drop_delegate_->OnAction(views::InkDropState::DEACTIVATED);
}

void BubbleIconView::ExecuteCommand(ExecuteSource source) {
  OnExecuting(source);
  if (command_updater_)
    command_updater_->ExecuteCommand(command_id_);
}

gfx::VectorIconId BubbleIconView::GetVectorIcon() const {
  return gfx::VectorIconId::VECTOR_ICON_NONE;
}

bool BubbleIconView::SetRasterIcon() {
  return false;
}

void BubbleIconView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::BubbleDelegateView* bubble = GetBubble();
  if (bubble)
    bubble->OnAnchorBoundsChanged();
  ink_drop_delegate_->OnLayout();
}

gfx::Point BubbleIconView::CalculateInkDropCenter() const {
  return GetLocalBounds().CenterPoint();
}

bool BubbleIconView::ShouldShowInkDropHover() const {
  // BubbleIconView views don't show hover effect.
  return false;
}

void BubbleIconView::UpdateIcon() {
  if (SetRasterIcon())
    return;

  const int icon_size =
      ui::MaterialDesignController::IsModeMaterial() ? 16 : 18;
  const ui::NativeTheme* theme = GetNativeTheme();
  SkColor icon_color =
      active_
          ? theme->GetSystemColor(ui::NativeTheme::kColorId_CallToActionColor)
          : color_utils::DeriveDefaultIconColor(theme->GetSystemColor(
                ui::NativeTheme::kColorId_TextfieldDefaultColor));
  image_->SetImage(
      gfx::CreateVectorIcon(GetVectorIcon(), icon_size, icon_color));
}

void BubbleIconView::SetActiveInternal(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  UpdateIcon();
}
