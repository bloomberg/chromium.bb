// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_options_menu_model.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_prefs_manager.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_settings_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// Menu commands
const int kTogglePermissionCommand = 0;
const int kToggleExtensionCommand = 1;
const int kOpenContentSettingsCommand = 2;
const int kCornerSelectionSubMenu = 3;

const int kCornerGroupId = 10;
const int kCornerUpperLeft = 11;
const int kCornerUpperRight = 12;
const int kCornerLowerLeft = 13;
const int kCornerLowerRight = 14;
const int kCornerDefault = 20;

CornerSelectionMenuModel::CornerSelectionMenuModel(Balloon* balloon)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      balloon_(balloon) {
  AddRadioItem(kCornerDefault,
               l10n_util::GetStringUTF16(IDS_NOTIFICATION_POSITION_DEFAULT),
               kCornerGroupId);
  AddSeparator();
  AddRadioItem(kCornerUpperLeft,
               l10n_util::GetStringUTF16(IDS_NOTIFICATION_POSITION_UPPER_LEFT),
               kCornerGroupId);
  AddRadioItem(kCornerUpperRight,
               l10n_util::GetStringUTF16(IDS_NOTIFICATION_POSITION_UPPER_RIGHT),
               kCornerGroupId);
  AddRadioItem(kCornerLowerLeft,
               l10n_util::GetStringUTF16(IDS_NOTIFICATION_POSITION_LOWER_LEFT),
               kCornerGroupId);
  AddRadioItem(kCornerLowerRight,
               l10n_util::GetStringUTF16(IDS_NOTIFICATION_POSITION_LOWER_RIGHT),
               kCornerGroupId);
}

CornerSelectionMenuModel::~CornerSelectionMenuModel() {
}

bool CornerSelectionMenuModel::IsCommandIdChecked(int command_id) const {
  NotificationPrefsManager* prefs =
      g_browser_process->notification_ui_manager()->prefs_manager();
  BalloonCollection::PositionPreference current =
      prefs->GetPositionPreference();

  if (command_id == kCornerUpperLeft)
    return (current == BalloonCollection::UPPER_LEFT);
  else if (command_id == kCornerUpperRight)
    return (current == BalloonCollection::UPPER_RIGHT);
  else if (command_id == kCornerLowerLeft)
    return (current == BalloonCollection::LOWER_LEFT);
  else if (command_id == kCornerLowerRight)
    return (current == BalloonCollection::LOWER_RIGHT);
  else if (command_id == kCornerDefault)
    return (current == BalloonCollection::DEFAULT_POSITION);

  NOTREACHED();
  return false;
}

bool CornerSelectionMenuModel::IsCommandIdEnabled(int command_id) const {
  // All the menu options are always enabled.
  return true;
}

bool CornerSelectionMenuModel::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  // Currently no accelerators.
  return false;
}

void CornerSelectionMenuModel::ExecuteCommand(int command_id) {
  NotificationPrefsManager* prefs =
      g_browser_process->notification_ui_manager()->prefs_manager();

  if (command_id == kCornerUpperLeft)
    prefs->SetPositionPreference(BalloonCollection::UPPER_LEFT);
  else if (command_id == kCornerUpperRight)
    prefs->SetPositionPreference(BalloonCollection::UPPER_RIGHT);
  else if (command_id == kCornerLowerLeft)
    prefs->SetPositionPreference(BalloonCollection::LOWER_LEFT);
  else if (command_id == kCornerLowerRight)
    prefs->SetPositionPreference(BalloonCollection::LOWER_RIGHT);
  else if (command_id == kCornerDefault)
    prefs->SetPositionPreference(BalloonCollection::DEFAULT_POSITION);
  else
    NOTREACHED();
}

NotificationOptionsMenuModel::NotificationOptionsMenuModel(Balloon* balloon)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      balloon_(balloon) {
  const Notification& notification = balloon->notification();
  const GURL& origin = notification.origin_url();

  if (origin.SchemeIs(chrome::kExtensionScheme)) {
    ExtensionService* extension_service =
        balloon_->profile()->GetExtensionService();
    const Extension* extension =
        extension_service->extensions()->GetExtensionOrAppByURL(
            ExtensionURLInfo(origin));
    // We get back no extension here when we show the notification after
    // the extension has crashed.
    if (extension) {
      const string16 disable_label = l10n_util::GetStringUTF16(
          IDS_EXTENSIONS_DISABLE);
      AddItem(kToggleExtensionCommand, disable_label);
    }
  } else if (!notification.display_source().empty()) {
    const string16 disable_label = l10n_util::GetStringFUTF16(
        IDS_NOTIFICATION_BALLOON_REVOKE_MESSAGE,
        notification.display_source());
    AddItem(kTogglePermissionCommand, disable_label);
  }

  if (!notification.display_source().empty()) {
    const string16 settings_label = l10n_util::GetStringUTF16(
        IDS_NOTIFICATIONS_SETTINGS_BUTTON);
    AddItem(kOpenContentSettingsCommand, settings_label);
  }

  corner_menu_model_.reset(new CornerSelectionMenuModel(balloon));
  AddSubMenu(kCornerSelectionSubMenu,
             l10n_util::GetStringUTF16(IDS_NOTIFICATION_CHOOSE_POSITION),
             corner_menu_model_.get());
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
  // TODO(tfarina,johnnyg): Remove this code if we decide to close notifications
  // after permissions are revoked.
  if (command_id == kTogglePermissionCommand ||
      command_id == kToggleExtensionCommand) {
    const Notification& notification = balloon_->notification();
    const GURL& origin = notification.origin_url();

    DesktopNotificationService* service =
        DesktopNotificationServiceFactory::GetForProfile(balloon_->profile());
    if (origin.SchemeIs(chrome::kExtensionScheme)) {
      ExtensionService* extension_service =
          balloon_->profile()->GetExtensionService();
      const Extension* extension =
          extension_service->extensions()->GetExtensionOrAppByURL(
              ExtensionURLInfo(origin));
      if (extension) {
        return l10n_util::GetStringUTF16(
            extension_service->IsExtensionEnabled(extension->id()) ?
                IDS_EXTENSIONS_DISABLE :
                IDS_EXTENSIONS_ENABLE);
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
    int /* command_id */, ui::Accelerator* /* accelerator */) {
  // Currently no accelerators.
  return false;
}

void NotificationOptionsMenuModel::ExecuteCommand(int command_id) {
  DesktopNotificationService* service =
      DesktopNotificationServiceFactory::GetForProfile(balloon_->profile());
  ExtensionService* extension_service =
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
      const Extension* extension =
          extension_service->extensions()->GetExtensionOrAppByURL(
              ExtensionURLInfo(origin));
      if (extension) {
        const std::string& id = extension->id();
        if (extension_service->IsExtensionEnabled(id))
          extension_service->DisableExtension(
              id, Extension::DISABLE_USER_ACTION);
        else
          extension_service->EnableExtension(id);
      }
      break;
    }
    case kOpenContentSettingsCommand: {
      Browser* browser =
          browser::FindLastActiveWithProfile(balloon_->profile());
      if (!browser) {
        // It is possible that there is no browser window (e.g. when there are
        // background pages, or for a chrome frame process on windows).
        browser = Browser::Create(balloon_->profile());
      }
      browser->ShowContentSettingsPage(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}
