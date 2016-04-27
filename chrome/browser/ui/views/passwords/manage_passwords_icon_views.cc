// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_icon_views.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/vector_icons_public.h"

ManagePasswordsIconViews::ManagePasswordsIconViews(CommandUpdater* updater)
    : BubbleIconView(updater, IDC_MANAGE_PASSWORDS_FOR_PAGE),
      state_(password_manager::ui::INACTIVE_STATE) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  UpdateUiForState();
}

ManagePasswordsIconViews::~ManagePasswordsIconViews() {}

void ManagePasswordsIconViews::SetState(password_manager::ui::State state) {
  if (state_ == state)
    return;
  // If there is an opened bubble for the current icon it should go away.
  ManagePasswordsBubbleView::CloseCurrentBubble();
  state_ = state;
  UpdateUiForState();
}

void ManagePasswordsIconViews::UpdateUiForState() {
  if (state_ == password_manager::ui::INACTIVE_STATE) {
    SetVisible(false);
    return;
  }

  SetTooltipText(l10n_util::GetStringUTF16(
      state_ == password_manager::ui::PENDING_PASSWORD_STATE
          ? IDS_PASSWORD_MANAGER_TOOLTIP_SAVE
          : IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE));

  SetVisible(true);

  // We may be about to automatically pop up a ManagePasswordsBubbleView.
  // Force layout of the icon's parent now; the bubble will be incorrectly
  // positioned otherwise, as the icon won't have been drawn into position.
  parent()->Layout();
}

void ManagePasswordsIconViews::OnExecuting(
    BubbleIconView::ExecuteSource source) {}

bool ManagePasswordsIconViews::OnMousePressed(const ui::MouseEvent& event) {
  bool result = BubbleIconView::OnMousePressed(event);
  ManagePasswordsBubbleView::CloseCurrentBubble();
  return result;
}

bool ManagePasswordsIconViews::OnKeyPressed(const ui::KeyEvent& event) {
  // Space is always ignored because otherwise the bubble appears with the
  // default button down. Releasing the space is equivalent to clicking this
  // button.
  if (event.key_code() == ui::VKEY_SPACE)
    return true;
  if (event.key_code() == ui::VKEY_RETURN && IsBubbleShowing()) {
    // If the bubble's open, the icon should transfer its focus to the bubble.
    // If it still somehow got this key event, the bubble shouldn't be reopened.
    return true;
  }
  return BubbleIconView::OnKeyPressed(event);
}

views::BubbleDialogDelegateView* ManagePasswordsIconViews::GetBubble() const {
  return ManagePasswordsBubbleView::manage_password_bubble();
}

gfx::VectorIconId ManagePasswordsIconViews::GetVectorIcon() const {
  return gfx::VectorIconId::AUTOLOGIN;
}

void ManagePasswordsIconViews::AboutToRequestFocusFromTabTraversal(
    bool reverse) {
  if (IsBubbleShowing())
    ManagePasswordsBubbleView::ActivateBubble();
}
