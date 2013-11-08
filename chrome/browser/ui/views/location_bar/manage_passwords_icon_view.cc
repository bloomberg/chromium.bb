// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/manage_passwords_icon_view.h"

#include "chrome/browser/ui/views/location_bar/manage_passwords_bubble_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

ManagePasswordsIconView::ManagePasswordsIconView(
    LocationBarView::Delegate* location_bar_delegate)
    : location_bar_delegate_(location_bar_delegate) {
  set_accessibility_focusable(true);
  Update(NULL);
  LocationBarView::InitTouchableLocationBarChildView(this);
}

ManagePasswordsIconView::~ManagePasswordsIconView() {}

void ManagePasswordsIconView::Update(
    ManagePasswordsIconController* manage_passwords_icon_controller) {
  SetVisible(manage_passwords_icon_controller &&
             manage_passwords_icon_controller->password_to_be_saved() &&
             !location_bar_delegate_->GetToolbarModel()->input_in_progress());
  if (!visible()) {
    ManagePasswordsBubbleView::CloseBubble();
    return;
  }
  SetTooltipText(l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_TOOLTIP_DEFAULT));
  SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_SAVE_PASSWORD));
}

void ManagePasswordsIconView::ShowBubbleIfNeeded(
    ManagePasswordsIconController* manage_passwords_icon_controller) {
  if (!manage_passwords_icon_controller->manage_passwords_bubble_shown() &&
      visible() && !ManagePasswordsBubbleView::IsShowing()) {
    ManagePasswordsBubbleView::ShowBubble(
        location_bar_delegate_->GetWebContents());
    manage_passwords_icon_controller->OnBubbleShown();
  }
}

bool ManagePasswordsIconView::GetTooltipText(const gfx::Point& p,
                                             string16* tooltip) const {
  // Don't show tooltip if the password bubble is displayed.
  return !ManagePasswordsBubbleView::IsShowing() &&
      ImageView::GetTooltipText(p, tooltip);
}

void ManagePasswordsIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    ManagePasswordsBubbleView::ShowBubble(
        location_bar_delegate_->GetWebContents());
    event->SetHandled();
  }
}

bool ManagePasswordsIconView::OnMousePressed(const ui::MouseEvent& event) {
  // Do nothing until the mouse button is released.
  return true;
}

void ManagePasswordsIconView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location()))
    ManagePasswordsBubbleView::ShowBubble(
        location_bar_delegate_->GetWebContents());
}
