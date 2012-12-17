// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_menu_model.h"

#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

PermissionMenuModel::PermissionMenuModel(
    Delegate* delegate,
    const GURL& url,
    ContentSettingsType type,
    ContentSetting default_setting,
    ContentSetting current_setting)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      delegate_(delegate),
      site_url_(url) {
  string16 label;
  switch (default_setting) {
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
    default:
      break;
  }
  AddCheckItem(COMMAND_SET_TO_DEFAULT, label);

  // Media only support COMMAND_SET_TO_ALLOW for https.
  if (type != CONTENT_SETTINGS_TYPE_MEDIASTREAM ||
      url.SchemeIsSecure()) {
    label = l10n_util::GetStringUTF16(
        IDS_WEBSITE_SETTINGS_MENU_ITEM_ALLOW);
    AddCheckItem(COMMAND_SET_TO_ALLOW, label);
  }
  if (type != CONTENT_SETTINGS_TYPE_FULLSCREEN) {
    label = l10n_util::GetStringUTF16(
        IDS_WEBSITE_SETTINGS_MENU_ITEM_BLOCK);
    AddCheckItem(COMMAND_SET_TO_BLOCK, label);
  }
}

bool PermissionMenuModel::IsCommandIdChecked(int command_id) const {
  if (delegate_)
    return delegate_->IsCommandIdChecked(command_id);
  return false;
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

void PermissionMenuModel::ExecuteCommand(int command_id) {
  if (delegate_)
    delegate_->ExecuteCommand(command_id);
}
