// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"

#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

ManagePasswordsIconView::ManagePasswordsIconView(
    LocationBarView::Delegate* location_bar_delegate)
    : location_bar_delegate_(location_bar_delegate) {
  set_id(VIEW_ID_MANAGE_PASSWORDS_ICON_BUTTON);
  SetAccessibilityFocusable(true);
  SetState(ManagePasswordsIcon::INACTIVE_STATE);
}

ManagePasswordsIconView::~ManagePasswordsIconView() {}

void ManagePasswordsIconView::SetStateInternal(
    ManagePasswordsIcon::State state) {
  if (state == ManagePasswordsIcon::INACTIVE_STATE) {
    SetVisible(false);
    if (ManagePasswordsBubbleView::IsShowing())
      ManagePasswordsBubbleView::CloseBubble();
    return;
  }

  // Start with the correct values for ManagePasswordsIcon::MANAGE_STATE, and
  // adjust things accordingly if we're either in BLACKLISTED_STATE or
  // PENDING_STATE.
  int which_icon = IDR_SAVE_PASSWORD;
  int which_text = IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE;
  if (state == ManagePasswordsIcon::BLACKLISTED_STATE)
    which_icon = IDR_SAVE_PASSWORD_BLACKLISTED;
  else if (state == ManagePasswordsIcon::PENDING_STATE)
    which_text = IDS_PASSWORD_MANAGER_TOOLTIP_SAVE;

  SetVisible(true);
  SetImage(
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(which_icon));
  SetTooltipText(l10n_util::GetStringUTF16(which_text));
}

void ManagePasswordsIconView::ShowBubbleWithoutUserInteraction() {
  // Suppress the bubble if the user is working in the omnibox.
  if (location_bar_delegate_->GetToolbarModel()->input_in_progress())
    return;

  ManagePasswordsBubbleView::ShowBubble(
      location_bar_delegate_->GetWebContents(),
      ManagePasswordsBubbleView::AUTOMATIC);
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
        ManagePasswordsBubbleView::USER_ACTION);
  }
}
