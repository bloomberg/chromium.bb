// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_view.h"

#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/gfx/size.h"

ZoomView::ZoomView(LocationBarView::Delegate* location_bar_delegate)
    : location_bar_delegate_(location_bar_delegate) {
  SetAccessibilityFocusable(true);
  Update(NULL);
}

ZoomView::~ZoomView() {
}

void ZoomView::Update(ZoomController* zoom_controller) {
  if (!zoom_controller || zoom_controller->IsAtDefaultZoom() ||
      location_bar_delegate_->GetToolbarModel()->input_in_progress()) {
    SetVisible(false);
    ZoomBubbleView::CloseBubble();
    return;
  }

  SetTooltipText(l10n_util::GetStringFUTF16Int(
      IDS_TOOLTIP_ZOOM, zoom_controller->GetZoomPercent()));
  SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      zoom_controller->GetResourceForZoomLevel()));
  SetVisible(true);
}

void ZoomView::GetAccessibleState(ui::AXViewState* state) {
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_ZOOM);
  state->role = ui::AX_ROLE_BUTTON;
}

bool ZoomView::GetTooltipText(const gfx::Point& p,
                              base::string16* tooltip) const {
  // Don't show tooltip if the zoom bubble is displayed.
  return !ZoomBubbleView::IsShowing() && ImageView::GetTooltipText(p, tooltip);
}

bool ZoomView::OnMousePressed(const ui::MouseEvent& event) {
  // Do nothing until mouse is released.
  return true;
}

void ZoomView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location()))
    ActivateBubble();
}

bool ZoomView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_SPACE &&
      event.key_code() != ui::VKEY_RETURN) {
    return false;
  }

  ActivateBubble();
  return true;
}

void ZoomView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    ActivateBubble();
    event->SetHandled();
  }
}

void ZoomView::ActivateBubble() {
  ZoomBubbleView::ShowBubble(location_bar_delegate_->GetWebContents(), false);
}
