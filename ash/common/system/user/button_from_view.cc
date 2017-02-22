// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/common/system/user/button_from_view.h"

#include "ash/common/ash_constants.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace tray {

ButtonFromView::ButtonFromView(views::View* content,
                               views::ButtonListener* listener,
                               TrayPopupInkDropStyle ink_drop_style)
    : CustomButton(listener),
      content_(content),
      ink_drop_style_(ink_drop_style),
      button_hovered_(false),
      ink_drop_container_(nullptr) {
  set_has_ink_drop_action_on_click(true);
  set_notify_enter_exit_on_child(true);
  ink_drop_container_ = new views::InkDropContainerView();
  AddChildView(ink_drop_container_);
  SetLayoutManager(new views::FillLayout());
  SetInkDropMode(InkDropHostView::InkDropMode::ON);
  AddChildView(content_);
  // Only make it focusable when we are active/interested in clicks.
  if (listener)
    SetFocusForPlatform();
}

ButtonFromView::~ButtonFromView() {}

void ButtonFromView::OnMouseEntered(const ui::MouseEvent& event) {
  button_hovered_ = true;
}

void ButtonFromView::OnMouseExited(const ui::MouseEvent& event) {
  button_hovered_ = false;
}

void ButtonFromView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);
  if (HasFocus()) {
    gfx::RectF rect(GetLocalBounds());
    canvas->DrawSolidFocusRect(rect, kFocusBorderColor, kFocusBorderThickness);
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
  views::CustomButton::GetAccessibleNodeData(node_data);
  // If no label has been explicitly set via CustomButton::SetAccessibleName(),
  // use the content's label.
  if (node_data->GetStringAttribute(ui::AX_ATTR_NAME).empty()) {
    ui::AXNodeData content_data;
    content_->GetAccessibleNodeData(&content_data);
    node_data->SetName(content_data.GetStringAttribute(ui::AX_ATTR_NAME));
  }
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

}  // namespace tray
}  // namespace ash
