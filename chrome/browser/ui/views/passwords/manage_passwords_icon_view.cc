// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"

#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

ManagePasswordsIconView::ManagePasswordsIconView(
    LocationBarView::Delegate* location_bar_delegate)
    : location_bar_delegate_(location_bar_delegate) {
  SetAccessibilityFocusable(true);
  Update(NULL);
  LocationBarView::InitTouchableLocationBarChildView(this);
}

ManagePasswordsIconView::~ManagePasswordsIconView() {}

void ManagePasswordsIconView::Update(
    ManagePasswordsBubbleUIController* manage_passwords_bubble_ui_controller) {
  SetVisible(manage_passwords_bubble_ui_controller &&
      manage_passwords_bubble_ui_controller->
          manage_passwords_icon_to_be_shown() &&
      !location_bar_delegate_->GetToolbarModel()->input_in_progress());
  if (!visible()) {
    ManagePasswordsBubbleView::CloseBubble(
        ManagePasswordsBubbleView::NOT_DISPLAYED);
    return;
  }
  SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_SAVE_PASSWORD));
  SetTooltip(manage_passwords_bubble_ui_controller->password_to_be_saved());
}

void ManagePasswordsIconView::ShowBubbleIfNeeded(
    ManagePasswordsBubbleUIController* manage_passwords_bubble_ui_controller) {
  if (manage_passwords_bubble_ui_controller->
          manage_passwords_bubble_needs_showing() &&
      visible() &&
      !ManagePasswordsBubbleView::IsShowing()) {
    ManagePasswordsBubbleView::ShowBubble(
        location_bar_delegate_->GetWebContents(),
        this,
        ManagePasswordsBubbleView::AUTOMATIC);
    manage_passwords_bubble_ui_controller->OnBubbleShown();
  }
}

void ManagePasswordsIconView::SetTooltip(bool password_to_be_saved) {
  SetTooltipText(l10n_util::GetStringUTF16(
      (password_to_be_saved ?
          IDS_PASSWORD_MANAGER_TOOLTIP_SAVE :
          IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE)));
}

bool ManagePasswordsIconView::GetTooltipText(const gfx::Point& p,
                                             base::string16* tooltip) const {
  // Don't show tooltip if the password bubble is displayed.
  return !ManagePasswordsBubbleView::IsShowing() &&
      ImageView::GetTooltipText(p, tooltip);
}

void ManagePasswordsIconView::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP) {
    ManagePasswordsBubbleView::ShowBubble(
        location_bar_delegate_->GetWebContents(),
        this,
        ManagePasswordsBubbleView::USER_ACTION);
    event->SetHandled();
  }
}

bool ManagePasswordsIconView::OnMousePressed(const ui::MouseEvent& event) {
  // Do nothing until the mouse button is released.
  return true;
}

void ManagePasswordsIconView::OnMouseReleased(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTestPoint(event.location())) {
    ManagePasswordsBubbleView::ShowBubble(
        location_bar_delegate_->GetWebContents(),
        this,
        ManagePasswordsBubbleView::USER_ACTION);
  }
}
