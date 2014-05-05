// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

ManagePasswordsIconView::ManagePasswordsIconView(
    LocationBarView::Delegate* location_bar_delegate,
    CommandUpdater* command_updater)
    : BubbleIconView(command_updater, IDC_MANAGE_PASSWORDS_FOR_PAGE),
      location_bar_delegate_(location_bar_delegate),
      command_updater_(command_updater),
      icon_id_(0),
      tooltip_text_id_(0) {
  set_id(VIEW_ID_MANAGE_PASSWORDS_ICON_BUTTON);
  SetAccessibilityFocusable(true);
  UpdateVisibleUI();
}

ManagePasswordsIconView::~ManagePasswordsIconView() {}

void ManagePasswordsIconView::UpdateVisibleUI() {
  // If the icon is inactive: clear out it's image and tooltip, hide the icon,
  // close any active bubble, and exit early.
  if (state() == ManagePasswordsIcon::INACTIVE_STATE) {
    icon_id_ = 0;
    tooltip_text_id_ = 0;

    SetVisible(false);
    if (ManagePasswordsBubbleView::IsShowing())
      ManagePasswordsBubbleView::CloseBubble();
    return;
  }

  // Otherwise, start with the correct values for MANAGE_STATE, and adjust
  // things accordingly if we're either in BLACKLISTED_STATE or PENDING_STATE.
  icon_id_ = IDR_SAVE_PASSWORD;
  tooltip_text_id_ = IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE;
  if (state() == ManagePasswordsIcon::BLACKLISTED_STATE)
    icon_id_ = IDR_SAVE_PASSWORD_BLACKLISTED;
  else if (state() == ManagePasswordsIcon::PENDING_STATE)
    tooltip_text_id_ = IDS_PASSWORD_MANAGER_TOOLTIP_SAVE;

  SetVisible(true);
  SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id_));
  SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id_));
}

void ManagePasswordsIconView::ShowBubbleWithoutUserInteraction() {
  // Suppress the bubble if the user is working in the omnibox.
  if (location_bar_delegate_->GetToolbarModel()->input_in_progress())
    return;

  command_updater_->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE);
}

bool ManagePasswordsIconView::IsBubbleShowing() const {
  return ManagePasswordsBubbleView::IsShowing();
}

void ManagePasswordsIconView::OnExecuting(
    BubbleIconView::ExecuteSource source) {
}
