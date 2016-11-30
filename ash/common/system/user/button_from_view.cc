// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/common/system/user/button_from_view.h"

#include "ash/common/ash_constants.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tray_utils.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace tray {

namespace {

// The border color of the user button.
const SkColor kBorderColor = 0xffdcdcdc;

}  // namespace

ButtonFromView::ButtonFromView(views::View* content,
                               views::ButtonListener* listener,
                               TrayPopupInkDropStyle ink_drop_style,
                               bool highlight_on_hover,
                               const gfx::Insets& tab_frame_inset)
    : CustomButton(listener),
      content_(content),
      ink_drop_style_(ink_drop_style),
      highlight_on_hover_(highlight_on_hover),
      button_hovered_(false),
      show_border_(false),
      tab_frame_inset_(tab_frame_inset),
      ink_drop_container_(nullptr) {
  set_has_ink_drop_action_on_click(true);
  set_notify_enter_exit_on_child(true);
  if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
    ink_drop_container_ = new views::InkDropContainerView();
    AddChildView(ink_drop_container_);
    SetLayoutManager(new views::FillLayout());
    SetInkDropMode(InkDropHostView::InkDropMode::ON);
  } else {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 1, 1, 0));
  }
  AddChildView(content_);
  ShowActive();
  // Only make it focusable when we are active/interested in clicks.
  if (listener)
    SetFocusForPlatform();
}

ButtonFromView::~ButtonFromView() {}

void ButtonFromView::ForceBorderVisible(bool show) {
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    return;
  show_border_ = show;
  ShowActive();
}

void ButtonFromView::OnMouseEntered(const ui::MouseEvent& event) {
  button_hovered_ = true;
  ShowActive();
}

void ButtonFromView::OnMouseExited(const ui::MouseEvent& event) {
  button_hovered_ = false;
  ShowActive();
}

void ButtonFromView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (HasFocus()) {
    gfx::RectF rect(GetLocalBounds());
    bool use_md = MaterialDesignController::IsSystemTrayMenuMaterial();
    if (!use_md)
      rect.Inset(gfx::InsetsF(tab_frame_inset_));
    canvas->DrawSolidFocusRect(rect, kFocusBorderColor,
                               use_md ? kFocusBorderThickness : 1);
  }
}

void ButtonFromView::OnFocus() {
  View::OnFocus();
  // Adding focus frame.
  SchedulePaint();
}

void ButtonFromView::OnBlur() {
  View::OnBlur();
  // Removing focus frame.
  SchedulePaint();
}

void ButtonFromView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_BUTTON;
  std::vector<base::string16> labels;
  for (int i = 0; i < child_count(); ++i)
    GetAccessibleLabelFromDescendantViews(child_at(i), labels);
  node_data->SetName(base::JoinString(labels, base::ASCIIToUTF16(" ")));
}

void ButtonFromView::Layout() {
  CustomButton::Layout();
  if (ink_drop_container_)
    ink_drop_container_->SetBoundsRect(GetLocalBounds());
}

void ButtonFromView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  // TODO(bruthig): Rework InkDropHostView so that it can still manage the
  // creation/application of the mask while allowing subclasses to use an
  // InkDropContainer.
  ink_drop_mask_ = CreateInkDropMask();
  if (ink_drop_mask_)
    ink_drop_layer->SetMaskLayer(ink_drop_mask_->layer());
  ink_drop_container_->AddInkDropLayer(ink_drop_layer);
}

void ButtonFromView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  // TODO(bruthig): Rework InkDropHostView so that it can still manage the
  // creation/application of the mask while allowing subclasses to use an
  // InkDropContainer.
  // Layers safely handle destroying a mask layer before the masked layer.
  ink_drop_mask_.reset();
  ink_drop_container_->RemoveInkDropLayer(ink_drop_layer);
}

std::unique_ptr<views::InkDrop> ButtonFromView::CreateInkDrop() {
  return TrayPopupUtils::CreateInkDrop(TrayPopupInkDropStyle::INSET_BOUNDS,
                                       this);
}

std::unique_ptr<views::InkDropRipple> ButtonFromView::CreateInkDropRipple()
    const {
  return TrayPopupUtils::CreateInkDropRipple(
      ink_drop_style_, this, GetInkDropCenterBasedOnLastEvent());
}

std::unique_ptr<views::InkDropHighlight>
ButtonFromView::CreateInkDropHighlight() const {
  return TrayPopupUtils::CreateInkDropHighlight(ink_drop_style_, this);
}

std::unique_ptr<views::InkDropMask> ButtonFromView::CreateInkDropMask() const {
  return TrayPopupUtils::CreateInkDropMask(ink_drop_style_, this);
}

void ButtonFromView::ShowActive() {
  if (MaterialDesignController::IsSystemTrayMenuMaterial())
    return;
  bool border_visible =
      (button_hovered_ && highlight_on_hover_) || show_border_;
  SkColor border_color = border_visible ? kBorderColor : SK_ColorTRANSPARENT;
  SetBorder(views::CreateSolidBorder(1, border_color));
  if (highlight_on_hover_) {
    SkColor background_color =
        button_hovered_ ? kHoverBackgroundColor : kBackgroundColor;
    content_->set_background(
        views::Background::CreateSolidBackground(background_color));
    set_background(views::Background::CreateSolidBackground(background_color));
  }
  SchedulePaint();
}

}  // namespace tray
}  // namespace ash
