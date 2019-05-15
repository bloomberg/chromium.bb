// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_info/permission_menu_model.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/chrome_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/origin_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"

PermissionMenuModel::PermissionMenuModel(Profile* profile,
                                         const GURL& url,
                                         const PageInfoUI::PermissionInfo& info,
                                         const ChangeCallback& callback)
    : ui::SimpleMenuModel(this),
      host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(profile)),
      permission_(info),
      callback_(callback) {
  DCHECK(!callback_.is_null());
  base::string16 label;

  DCHECK_NE(permission_.default_setting, CONTENT_SETTING_NUM_SETTINGS);

  // The Material UI for site settings uses comboboxes instead of menubuttons,
  // which means the elements of the menu themselves have to be shorter, instead
  // of simply setting a shorter label on the menubutton.
  label = PageInfoUI::PermissionActionToUIString(
      profile, permission_.type, CONTENT_SETTING_DEFAULT,
      permission_.default_setting, permission_.source);

  AddCheckItem(CONTENT_SETTING_DEFAULT, label);

  // Retrieve the string to show for allowing the permission.
  if (ShouldShowAllow(url)) {
    label = PageInfoUI::PermissionActionToUIString(
        profile, permission_.type, CONTENT_SETTING_ALLOW,
        permission_.default_setting, permission_.source);
    AddCheckItem(CONTENT_SETTING_ALLOW, label);
  }

  // Retrieve the string to show for blocking the permission.
  label = PageInfoUI::PermissionActionToUIString(
      profile, info.type, CONTENT_SETTING_BLOCK, permission_.default_setting,
      info.source);
  AddCheckItem(CONTENT_SETTING_BLOCK, label);

  // Retrieve the string to show for allowing the user to be asked about the
  // permission.
  if (ShouldShowAsk(url)) {
    label = PageInfoUI::PermissionActionToUIString(
        profile, info.type, CONTENT_SETTING_ASK, permission_.default_setting,
        info.source);
    AddCheckItem(CONTENT_SETTING_ASK, label);
  }
}

PermissionMenuModel::~PermissionMenuModel() {}

bool PermissionMenuModel::IsCommandIdChecked(int command_id) const {
  return permission_.setting == command_id;
}

bool PermissionMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

void PermissionMenuModel::ExecuteCommand(int command_id, int event_flags) {
  permission_.setting = static_cast<ContentSetting>(command_id);
  callback_.Run(permission_);
}

bool PermissionMenuModel::ShouldShowAllow(const GURL& url) {
  // Notifications does not support CONTENT_SETTING_ALLOW in incognito.
  if (permission_.is_incognito &&
      permission_.type == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) {
    return false;
  }

  // Media only supports CONTENT_SETTING_ALLOW for secure origins.
  if ((permission_.type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
       permission_.type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA) &&
      !content::IsOriginSecure(url)) {
    return false;
  }

  // Chooser permissions do not support CONTENT_SETTING_ALLOW.
  if (permission_.type == CONTENT_SETTINGS_TYPE_SERIAL_GUARD ||
      permission_.type == CONTENT_SETTINGS_TYPE_USB_GUARD) {
    return false;
  }

  // Bluetooth scanning permission does not support CONTENT_SETTING_ALLOW.
  if (permission_.type == CONTENT_SETTINGS_TYPE_BLUETOOTH_SCANNING)
    return false;

  return true;
}

bool PermissionMenuModel::ShouldShowAsk(const GURL& url) {
  return permission_.type == CONTENT_SETTINGS_TYPE_USB_GUARD ||
         permission_.type == CONTENT_SETTINGS_TYPE_SERIAL_GUARD ||
         permission_.type == CONTENT_SETTINGS_TYPE_BLUETOOTH_SCANNING;
}
