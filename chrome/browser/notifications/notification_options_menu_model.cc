// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_options_menu_model.h"

#include "app/l10n_util.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

// Menu commands
const int kRevokePermissionCommand = 0;
const int kDisableExtensionCommand = 1;
const int kOpenContentSettingsCommand = 2;

NotificationOptionsMenuModel::NotificationOptionsMenuModel(Balloon* balloon)
    : ALLOW_THIS_IN_INITIALIZER_LIST(menus::SimpleMenuModel(this)),
      balloon_(balloon) {

  const Notification& notification = balloon->notification();
  const GURL& origin = notification.origin_url();

  if (origin.SchemeIs(chrome::kExtensionScheme)) {
    const string16 disable_label = l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_DISABLE);
    AddItem(kDisableExtensionCommand, disable_label);
  } else {
    const string16 disable_label = l10n_util::GetStringFUTF16(
        IDS_NOTIFICATION_BALLOON_REVOKE_MESSAGE,
        WideToUTF16(notification.display_source()));
    AddItem(kRevokePermissionCommand, disable_label);
  }

  const string16 settings_label = l10n_util::GetStringUTF16(
      IDS_NOTIFICATIONS_SETTINGS_BUTTON);
  AddItem(kOpenContentSettingsCommand, settings_label);
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
  // Currently no accelerators.
  return false;
}

void NotificationOptionsMenuModel::ExecuteCommand(int command_id) {
  DesktopNotificationService* service =
      balloon_->profile()->GetDesktopNotificationService();
  ExtensionsService* ext_service =
      balloon_->profile()->GetExtensionsService();
  const GURL& origin = balloon_->notification().origin_url();
  switch (command_id) {
    case kRevokePermissionCommand:
      service->DenyPermission(origin);
      break;
    case kDisableExtensionCommand: {
      Extension* extension = ext_service->GetExtensionByURL(origin);
      if (extension)
        ext_service->DisableExtension(extension->id());
      break;
    }
    case kOpenContentSettingsCommand: {
      Browser* browser = BrowserList::GetLastActive();
      if (browser)
        static_cast<TabContentsDelegate*>(browser)->ShowContentSettingsWindow(
            CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}
