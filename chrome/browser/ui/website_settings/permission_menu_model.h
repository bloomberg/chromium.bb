// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_MENU_MODEL_H_
#define CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_MENU_MODEL_H_

#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "url/gurl.h"

class PermissionMenuModel : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  enum CommandID {
    COMMAND_SET_TO_DEFAULT,
    COMMAND_SET_TO_ALLOW,
    COMMAND_SET_TO_BLOCK,
  };

  class Delegate {
   public:
    // Executes the command with the given |command_id|.
    virtual void ExecuteCommand(int command_id) = 0;
    // Returns true if the command with the given |command_id| should be
    // checked.
    virtual bool IsCommandIdChecked(int command_id) = 0;
  };

  // Create a new menu model for permission settings. To restrict to
  // simple allow/block settings, set the type to CONTENT_SETTINGS_TYPE_DEFAULT
  // and the default setting to CONTENT_SETTING_NUM_SETTINGS.
  PermissionMenuModel(Delegate* delegate,
                      const GURL& url,
                      ContentSettingsType type,
                      ContentSetting default_setting,
                      ContentSetting current_setting);

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

 private:
  // The delegate of the |PermissionMenuModel|. |delegate_| can be NULL.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PermissionMenuModel);
};

#endif  // CHROME_BROWSER_UI_WEBSITE_SETTINGS_PERMISSION_MENU_MODEL_H_
