// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_menu_model.h"

#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/plugins_field_trial.h"
#include "content/public/common/origin_util.h"
#include "ui/base/l10n/l10n_util.h"

PermissionMenuModel::PermissionMenuModel(
    const GURL& url,
    const WebsiteSettingsUI::PermissionInfo& info,
    const ChangeCallback& callback)
    : ui::SimpleMenuModel(this), permission_(info), callback_(callback) {
  DCHECK(!callback_.is_null());
  base::string16 label;

  ContentSetting effective_default_setting = permission_.default_setting;

#if defined(ENABLE_PLUGINS)
  effective_default_setting =
      content_settings::PluginsFieldTrial::EffectiveContentSetting(
          permission_.type, permission_.default_setting);
#endif  // defined(ENABLE_PLUGINS)

  switch (effective_default_setting) {
    case CONTENT_SETTING_ALLOW:
      label = l10n_util::GetStringUTF16(
          IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_ALLOW);
      break;
    case CONTENT_SETTING_BLOCK:
      label = l10n_util::GetStringUTF16(
          IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_BLOCK);
      break;
    case CONTENT_SETTING_ASK:
      label =
          l10n_util::GetStringUTF16(IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_ASK);
      break;
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
      label = l10n_util::GetStringUTF16(
          IDS_WEBSITE_SETTINGS_MENU_ITEM_DEFAULT_DETECT_IMPORTANT_CONTENT);
      break;
    case CONTENT_SETTING_NUM_SETTINGS:
      NOTREACHED();
    default:
      break;
  }
  AddCheckItem(CONTENT_SETTING_DEFAULT, label);

  // CONTENT_SETTING_ALLOW and CONTENT_SETTING_BLOCK are not allowed for
  // fullscreen or mouse lock on file:// URLs, because there wouldn't be
  // a reasonable origin with which to associate the preference.
  // TODO(estark): Revisit this when crbug.com/455882 is fixed.
  bool is_exclusive_access_on_file =
      (permission_.type == CONTENT_SETTINGS_TYPE_FULLSCREEN ||
       permission_.type == CONTENT_SETTINGS_TYPE_MOUSELOCK) &&
      url.SchemeIsFile();

  // The deprecated MEDIASTREAM setting is no longer used to represent media.
  DCHECK_NE(CONTENT_SETTINGS_TYPE_MEDIASTREAM, permission_.type);

  // Media only supports CONTENT_SETTTING_ALLOW for secure origins.
  bool is_media_permission =
      permission_.type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC ||
      permission_.type == CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA;
  if ((!is_media_permission || content::IsOriginSecure(url)) &&
      !is_exclusive_access_on_file) {
    label = l10n_util::GetStringUTF16(
        IDS_WEBSITE_SETTINGS_MENU_ITEM_ALLOW);
    AddCheckItem(CONTENT_SETTING_ALLOW, label);
  }

  if (permission_.type == CONTENT_SETTINGS_TYPE_PLUGINS) {
    label = l10n_util::GetStringUTF16(
        IDS_WEBSITE_SETTINGS_MENU_ITEM_DETECT_IMPORTANT_CONTENT);
    AddCheckItem(CONTENT_SETTING_DETECT_IMPORTANT_CONTENT, label);
  }

  if (permission_.type != CONTENT_SETTINGS_TYPE_FULLSCREEN &&
      !is_exclusive_access_on_file) {
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
  ContentSetting setting = permission_.setting;

#if defined(ENABLE_PLUGINS)
  setting = content_settings::PluginsFieldTrial::EffectiveContentSetting(
      permission_.type, permission_.setting);
#endif  // defined(ENABLE_PLUGINS)

  return setting == command_id;
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
