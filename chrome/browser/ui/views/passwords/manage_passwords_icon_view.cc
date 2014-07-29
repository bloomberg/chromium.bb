// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

ManagePasswordsIconView::ManagePasswordsIconView(CommandUpdater* updater)
    : BubbleIconView(updater, IDC_MANAGE_PASSWORDS_FOR_PAGE),
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
  if (state() == password_manager::ui::INACTIVE_STATE) {
    icon_id_ = 0;
    tooltip_text_id_ = 0;

    SetVisible(false);
    if (ManagePasswordsBubbleView::IsShowing())
      ManagePasswordsBubbleView::CloseBubble();
    return;
  }

  // Otherwise, start with the correct values for MANAGE_STATE, and adjust
  // things accordingly if we're either in BLACKLIST_STATE or PENDING_STATE.
  icon_id_ = active() ? IDR_SAVE_PASSWORD_ACTIVE : IDR_SAVE_PASSWORD_INACTIVE;
  tooltip_text_id_ = IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE;
  if (state() == password_manager::ui::BLACKLIST_STATE)
    icon_id_ = active() ? IDR_SAVE_PASSWORD_DISABLED_ACTIVE
                        : IDR_SAVE_PASSWORD_DISABLED_INACTIVE;
  else if (password_manager::ui::IsPendingState(state()))
    tooltip_text_id_ = IDS_PASSWORD_MANAGER_TOOLTIP_SAVE;

  SetVisible(true);
  SetImage(ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(icon_id_));
  SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id_));

  // We may be about to automatically pop up a ManagePasswordsBubbleView.
  // Force layout of the icon's parent now; the bubble will be incorrectly
  // positioned otherwise, as the icon won't have been drawn into position.
  parent()->Layout();
}

bool ManagePasswordsIconView::IsBubbleShowing() const {
  // If the bubble is being destroyed, it's considered showing though it may be
  // already invisible currently.
  return ManagePasswordsBubbleView::manage_password_bubble() != NULL;
}

void ManagePasswordsIconView::OnExecuting(
    BubbleIconView::ExecuteSource source) {
}

bool ManagePasswordsIconView::OnMousePressed(const ui::MouseEvent& event) {
  bool result = BubbleIconView::OnMousePressed(event);
  if (IsBubbleShowing())
    ManagePasswordsBubbleView::CloseBubble();
  return result;
}
