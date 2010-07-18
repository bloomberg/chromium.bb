// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_options_menu_model.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

static const int kRevokePermissionCommand = 0;

NotificationOptionsMenuModel::NotificationOptionsMenuModel(Balloon* balloon)
    : menus::SimpleMenuModel(this),
      balloon_(balloon) {
  const string16 label_text = WideToUTF16Hack(l10n_util::GetStringF(
      IDS_NOTIFICATION_BALLOON_REVOKE_MESSAGE,
      balloon->notification().display_source()));
  AddItem(kRevokePermissionCommand, label_text);
}

NotificationOptionsMenuModel::~NotificationOptionsMenuModel() {
}

bool NotificationOptionsMenuModel::IsCommandIdChecked(int /* command_id */)
    const {
  // Nothing in the menu is checked.
  return false;
}

bool NotificationOptionsMenuModel::IsCommandIdEnabled(int /* command_id */)
    const {
  // All the menu options are always enabled.
  return true;
}

bool NotificationOptionsMenuModel::GetAcceleratorForCommandId(
    int /* command_id */, menus::Accelerator* /* accelerator */) {
  // Current no accelerators.
  return false;
}

void NotificationOptionsMenuModel::ExecuteCommand(int command_id) {
  DesktopNotificationService* service =
    balloon_->profile()->GetDesktopNotificationService();
  switch (command_id) {
    case kRevokePermissionCommand:
      service->DenyPermission(balloon_->notification().origin_url());
      break;
    default:
      NOTREACHED();
      break;
  }
}
