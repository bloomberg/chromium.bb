// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_options_menu_model.h"

#include "app/l10n_util.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/browser_dialogs.h"
#endif  // OS_WIN

// Menu commands
const int kTogglePermissionCommand = 0;
const int kToggleExtensionCommand = 1;
const int kOpenContentSettingsCommand = 2;

NotificationOptionsMenuModel::NotificationOptionsMenuModel(Balloon* balloon)
    : ALLOW_THIS_IN_INITIALIZER_LIST(menus::SimpleMenuModel(this)),
      balloon_(balloon) {

  const Notification& notification = balloon->notification();
  const GURL& origin = notification.origin_url();

  if (origin.SchemeIs(chrome::kExtensionScheme)) {
    const string16 disable_label = l10n_util::GetStringUTF16(
        IDS_EXTENSIONS_DISABLE);
    AddItem(kToggleExtensionCommand, disable_label);
  } else {
    const string16 disable_label = l10n_util::GetStringFUTF16(
        IDS_NOTIFICATION_BALLOON_REVOKE_MESSAGE,
        notification.display_source());
    AddItem(kTogglePermissionCommand, disable_label);
  }

  const string16 settings_label = l10n_util::GetStringUTF16(
      IDS_NOTIFICATIONS_SETTINGS_BUTTON);
  AddItem(kOpenContentSettingsCommand, settings_label);
}

NotificationOptionsMenuModel::~NotificationOptionsMenuModel() {
}

bool NotificationOptionsMenuModel::IsItemForCommandIdDynamic(int command_id)
    const {
  return command_id == kTogglePermissionCommand ||
         command_id == kToggleExtensionCommand;
}

string16 NotificationOptionsMenuModel::GetLabelForCommandId(int command_id)
    const {
  // TODO(tfarina,johnnyg): Removed this code if we decide to close
  // notifications after permissions are revoked.
  if (command_id == kTogglePermissionCommand ||
      command_id == kToggleExtensionCommand) {
    const Notification& notification = balloon_->notification();
    const GURL& origin = notification.origin_url();

    DesktopNotificationService* service =
        balloon_->profile()->GetDesktopNotificationService();
    if (origin.SchemeIs(chrome::kExtensionScheme)) {
      ExtensionService* ext_service =
          balloon_->profile()->GetExtensionService();
      const Extension* extension = ext_service->GetExtensionByURL(origin);
      if (extension) {
        ExtensionPrefs* extension_prefs = ext_service->extension_prefs();
        const std::string& id = extension->id();
        if (extension_prefs->GetExtensionState(id) == Extension::ENABLED)
          return l10n_util::GetStringUTF16(IDS_EXTENSIONS_DISABLE);
        else
          return l10n_util::GetStringUTF16(IDS_EXTENSIONS_ENABLE);
      }
    } else {
      if (service->GetContentSetting(origin) == CONTENT_SETTING_ALLOW) {
        return l10n_util::GetStringFUTF16(
            IDS_NOTIFICATION_BALLOON_REVOKE_MESSAGE,
            notification.display_source());
      } else {
        return l10n_util::GetStringFUTF16(
            IDS_NOTIFICATION_BALLOON_ENABLE_MESSAGE,
            notification.display_source());
      }
    }
  } else if (command_id == kOpenContentSettingsCommand) {
    return l10n_util::GetStringUTF16(IDS_NOTIFICATIONS_SETTINGS_BUTTON);
  }
  return string16();
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
  ExtensionService* ext_service =
      balloon_->profile()->GetExtensionService();
  const GURL& origin = balloon_->notification().origin_url();
  switch (command_id) {
    case kTogglePermissionCommand:
      if (service->GetContentSetting(origin) == CONTENT_SETTING_ALLOW)
        service->DenyPermission(origin);
      else
        service->GrantPermission(origin);
      break;
    case kToggleExtensionCommand: {
      const Extension* extension = ext_service->GetExtensionByURL(origin);
      if (extension) {
        ExtensionPrefs* extension_prefs = ext_service->extension_prefs();
        const std::string& id = extension->id();
        if (extension_prefs->GetExtensionState(id) == Extension::ENABLED)
          ext_service->DisableExtension(id);
        else
          ext_service->EnableExtension(id);
      }
      break;
    }
    case kOpenContentSettingsCommand: {
      Browser* browser = BrowserList::GetLastActive();
      if (browser) {
        static_cast<TabContentsDelegate*>(browser)->ShowContentSettingsWindow(
            CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
      } else {
#if defined(OS_WIN)
        if (CommandLine::ForCurrentProcess()->HasSwitch(
                switches::kChromeFrame)) {
          // We may not have a browser if this is a chrome frame process.
          browser::ShowContentSettingsWindow(NULL,
                                             CONTENT_SETTINGS_TYPE_DEFAULT,
                                             balloon_->profile());
        }
#endif  // OS_WIN
      }
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}
