// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_menu_model.h"

#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

PermissionMenuModel::PermissionMenuModel(
    const GURL& url,
    const WebsiteSettingsUI::PermissionInfo& info,
    const ChangeCallback& callback)
    : ui::SimpleMenuModel(this), permission_(info), callback_(callback) {
  DCHECK(!callback_.is_null());
  base::string16 label;
  switch (permission_.default_setting) {
    case CONTENT_SETTING_ALLOW:
      label = l10n_util::GetStringUTF16(
          IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_ALLOW);
      break;
    case CONTENT_SETTING_BLOCK:
      label = l10n_util::GetStringUTF16(
          IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_BLOCK);
      break;
    case CONTENT_SETTING_ASK:
      label = l10n_util::GetStringUTF16(
          IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_ASK);
      break;
    case CONTENT_SETTING_NUM_SETTINGS:
      NOTREACHED();
    default:
      break;
  }
  AddCheckItem(CONTENT_SETTING_DEFAULT, label);

  // Media only support CONTENTE_SETTTING_ALLOW for https.
  if (permission_.type != CONTENT_SETTINGS_TYPE_MEDIASTREAM ||
      url.SchemeIsSecure()) {
    label = l10n_util::GetStringUTF16(
        IDS_WEBSITE_SETTINGS_MENU_ITEM_ALLOW);
    AddCheckItem(CONTENT_SETTING_ALLOW, label);
  }

  if (permission_.type != CONTENT_SETTINGS_TYPE_FULLSCREEN) {
    label = l10n_util::GetStringUTF16(
        IDS_WEBSITE_SETTINGS_MENU_ITEM_BLOCK);
    AddCheckItem(CONTENT_SETTING_BLOCK, label);
  }
}

PermissionMenuModel::PermissionMenuModel(const GURL& url,
                                         ContentSetting setting,
                                         const ChangeCallback& callback)
    : ui::SimpleMenuModel(this), callback_(callback) {
  DCHECK(setting == CONTENT_SETTING_ALLOW || setting == CONTENT_SETTING_BLOCK);
  permission_.type = CONTENT_SETTINGS_TYPE_DEFAULT;
  permission_.setting = setting;
  permission_.default_setting = CONTENT_SETTING_NUM_SETTINGS;
  AddCheckItem(CONTENT_SETTING_ALLOW,
               l10n_util::GetStringUTF16(IDS_PERMISSION_ALLOW));
  AddCheckItem(CONTENT_SETTING_BLOCK,
               l10n_util::GetStringUTF16(IDS_PERMISSION_DENY));
}

PermissionMenuModel::~PermissionMenuModel() {}

bool PermissionMenuModel::IsCommandIdChecked(int command_id) const {
  return permission_.setting == command_id;
}

bool PermissionMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

bool PermissionMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  // Accelerators are not supported.
  return false;
}

void PermissionMenuModel::ExecuteCommand(int command_id, int event_flags) {
  permission_.setting = static_cast<ContentSetting>(command_id);
  callback_.Run(permission_);
}
