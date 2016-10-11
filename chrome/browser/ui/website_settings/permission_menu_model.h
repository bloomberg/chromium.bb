// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_MENU_MODEL_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_MENU_MODEL_H_

#include "base/macros.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

class HostContentSettingsMap;
class Profile;

class PermissionMenuModel : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  typedef base::Callback<void(const WebsiteSettingsUI::PermissionInfo&)>
      ChangeCallback;

  // Create a new menu model for permission settings.
  PermissionMenuModel(Profile* profile,
                      const GURL& url,
                      const WebsiteSettingsUI::PermissionInfo& info,
                      const ChangeCallback& callback);
  // Creates a special-case menu model that only has the allow and block
  // options.  It does not track a permission type.  |setting| is the
  // initial selected option.  It must be either CONTENT_SETTING_ALLOW or
  // CONTENT_SETTING_BLOCK.
  PermissionMenuModel(Profile* profile,
                      const GURL& url,
                      ContentSetting setting,
                      const ChangeCallback& callback);
  ~PermissionMenuModel() override;

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  HostContentSettingsMap* host_content_settings_map_;

  // The permission info represented by the menu model.
  WebsiteSettingsUI::PermissionInfo permission_;

  // Callback to be called when the permission's setting is changed.
  ChangeCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(PermissionMenuModel);
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_MENU_MODEL_H_
