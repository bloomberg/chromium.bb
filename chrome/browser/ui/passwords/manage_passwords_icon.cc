// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_icon.h"

#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"

ManagePasswordsIcon::ManagePasswordsIcon()
    : icon_id_(0),
      tooltip_text_id_(0),
      state_(password_manager::ui::INACTIVE_STATE),
      active_(false) {
}

ManagePasswordsIcon::~ManagePasswordsIcon() {
}

void ManagePasswordsIcon::SetActive(bool active) {
  if (active_ == active)
    return;
  active_ = active;
  UpdateIDs();
  UpdateVisibleUI();
}

void ManagePasswordsIcon::SetState(password_manager::ui::State state) {
  if (state_ == state)
    return;
  state_ = state;
  UpdateIDs();
  UpdateVisibleUI();
}

void ManagePasswordsIcon::UpdateIDs() {
  // If the icon is inactive: clear out its image and tooltip and exit early.
  if (state() == password_manager::ui::INACTIVE_STATE) {
    icon_id_ = 0;
    tooltip_text_id_ = 0;
    return;
  }

  // Otherwise, start with the correct values for MANAGE_STATE, and adjust
  // things accordingly if we're either in BLACKLIST_STATE or PENDING_STATE.
  // TODO(dconnelly): Figure out how to share the resources with Android.
#if !defined(OS_ANDROID)
  icon_id_ = active() ? IDR_SAVE_PASSWORD_ACTIVE : IDR_SAVE_PASSWORD_INACTIVE;
  tooltip_text_id_ = IDS_PASSWORD_MANAGER_TOOLTIP_MANAGE;
  if (state() == password_manager::ui::BLACKLIST_STATE)
    icon_id_ = active() ? IDR_SAVE_PASSWORD_DISABLED_ACTIVE
                        : IDR_SAVE_PASSWORD_DISABLED_INACTIVE;
  else if (password_manager::ui::IsPendingState(state()))
    tooltip_text_id_ = IDS_PASSWORD_MANAGER_TOOLTIP_SAVE;
#endif
}
