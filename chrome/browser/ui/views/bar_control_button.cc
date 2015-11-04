// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bar_control_button.h"

#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/animation/ink_drop_animation_controller.h"
#include "ui/views/animation/ink_drop_animation_controller_factory.h"
#include "ui/views/border.h"

namespace {

// Extra space around the buttons to increase their event target size.
const int kButtonExtraTouchSize = 4;

}  // namespace

BarControlButton::BarControlButton(views::ButtonListener* listener)
    : views::ImageButton(listener),
      id_(gfx::VectorIconId::VECTOR_ICON_NONE),
      ink_drop_animation_controller_(
          views::InkDropAnimationControllerFactory::
              CreateInkDropAnimationController(this)) {
  const int kInkDropLargeSize = 32;
  const int kInkDropLargeCornerRadius = 4;
  const int kInkDropSmallSize = 24;
  const int kInkDropSmallCornerRadius = 2;
  ink_drop_animation_controller_->SetInkDropSize(
      gfx::Size(kInkDropLargeSize, kInkDropLargeSize),
      kInkDropLargeCornerRadius,
      gfx::Size(kInkDropSmallSize, kInkDropSmallSize),
      kInkDropSmallCornerRadius);
}

BarControlButton::~BarControlButton() {}

void BarControlButton::SetIcon(
    gfx::VectorIconId id,
    const base::Callback<SkColor(void)>& get_text_color_callback) {
  id_ = id;
  get_text_color_callback_ = get_text_color_callback;

  SetBorder(views::Border::CreateEmptyBorder(
      kButtonExtraTouchSize, kButtonExtraTouchSize, kButtonExtraTouchSize,
      kButtonExtraTouchSize));
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
}

void BarControlButton::OnThemeChanged() {
  SkColor icon_color =
      color_utils::DeriveDefaultIconColor(get_text_color_callback_.Run());
  gfx::ImageSkia image = gfx::CreateVectorIcon(id_, 16, icon_color);
  SetImage(views::CustomButton::STATE_NORMAL, &image);
  image = gfx::CreateVectorIcon(id_, 16, SkColorSetA(icon_color, 0xff / 2));
  SetImage(views::CustomButton::STATE_DISABLED, &image);
}

void BarControlButton::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  OnThemeChanged();
}

void BarControlButton::Layout() {
  ImageButton::Layout();

  ink_drop_animation_controller_->SetInkDropCenter(
      GetLocalBounds().CenterPoint());
}

void BarControlButton::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(false);
  layer()->Add(ink_drop_layer);
  layer()->StackAtBottom(ink_drop_layer);
}

void BarControlButton::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  layer()->Remove(ink_drop_layer);
  SetFillsBoundsOpaquely(true);
  SetPaintToLayer(false);
}

bool BarControlButton::OnMousePressed(const ui::MouseEvent& event) {
  if (IsTriggerableEvent(event)) {
    ink_drop_animation_controller_->AnimateToState(
        views::InkDropState::ACTION_PENDING);
  }

  return ImageButton::OnMousePressed(event);
}

void BarControlButton::OnGestureEvent(ui::GestureEvent* event) {
  views::InkDropState ink_drop_state = views::InkDropState::HIDDEN;
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      ink_drop_state = views::InkDropState::ACTION_PENDING;
      // The ui::ET_GESTURE_TAP_DOWN event needs to be marked as handled so
      // that subsequent events for the gesture are sent to |this|.
      event->SetHandled();
      break;
    case ui::ET_GESTURE_LONG_PRESS:
      ink_drop_state = views::InkDropState::SLOW_ACTION_PENDING;
      break;
    case ui::ET_GESTURE_TAP:
      ink_drop_state = views::InkDropState::QUICK_ACTION;
      break;
    case ui::ET_GESTURE_LONG_TAP:
      ink_drop_state = views::InkDropState::SLOW_ACTION;
      break;
    case ui::ET_GESTURE_END:
    case ui::ET_GESTURE_TAP_CANCEL:
      ink_drop_state = views::InkDropState::HIDDEN;
      break;
    default:
      return;
  }
  ink_drop_animation_controller_->AnimateToState(ink_drop_state);

  ImageButton::OnGestureEvent(event);
}

void BarControlButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (!HitTestPoint(event.location()))
    ink_drop_animation_controller_->AnimateToState(views::InkDropState::HIDDEN);

  ImageButton::OnMouseReleased(event);
}

void BarControlButton::NotifyClick(const ui::Event& event) {
  ink_drop_animation_controller_->AnimateToState(
      views::InkDropState::QUICK_ACTION);

  ImageButton::NotifyClick(event);
}
