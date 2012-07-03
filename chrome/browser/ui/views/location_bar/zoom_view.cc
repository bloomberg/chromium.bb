// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_view.h"

#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/view_ids.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"

ZoomView::ZoomView(ToolbarModel* toolbar_model)
    : toolbar_model_(toolbar_model),
      zoom_icon_state_(ZoomController::NONE),
      zoom_percent_(100) {
  set_accessibility_focusable(true);
}

ZoomView::~ZoomView() {
}

void ZoomView::SetZoomIconState(ZoomController::ZoomIconState zoom_icon_state) {
  if (zoom_icon_state == zoom_icon_state_)
    return;

  zoom_icon_state_ = zoom_icon_state;
  Update();
}

void ZoomView::SetZoomIconTooltipPercent(int zoom_percent) {
  if (zoom_percent == zoom_percent_)
    return;

  zoom_percent_ = zoom_percent;
  Update();
}

void ZoomView::Update() {
  if (toolbar_model_->input_in_progress() ||
      zoom_icon_state_ == ZoomController::NONE) {
    SetVisible(false);
    ZoomBubbleView::CloseBubble();
  } else {
    SetVisible(true);
    SetTooltipText(
        l10n_util::GetStringFUTF16Int(IDS_TOOLTIP_ZOOM, zoom_percent_));
    SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        zoom_icon_state_ == ZoomController::ZOOM_PLUS_ICON ?
        IDR_ZOOM_PLUS : IDR_ZOOM_MINUS));
  }
}

void ZoomView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_ZOOM);
  state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
}

bool ZoomView::GetTooltipText(const gfx::Point& p, string16* tooltip) const {
  // Don't show tooltip if the zoom bubble is displayed.
  return !ZoomBubbleView::IsShowing() && ImageView::GetTooltipText(p, tooltip);
}

bool ZoomView::OnMousePressed(const views::MouseEvent& event) {
  // Do nothing until mouse is released.
  return true;
}

void ZoomView::OnMouseReleased(const views::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTest(event.location()))
    ZoomBubbleView::ShowBubble(this, zoom_percent_, false);
}

bool ZoomView::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_SPACE &&
      event.key_code() != ui::VKEY_RETURN)
    return false;

  ZoomBubbleView::ShowBubble(this, zoom_percent_, false);
  return true;
}
