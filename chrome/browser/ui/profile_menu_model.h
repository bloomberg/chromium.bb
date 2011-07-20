// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILE_MENU_MODEL_H_
#define CHROME_BROWSER_UI_PROFILE_MENU_MODEL_H_
#pragma once

#include "ui/base/models/simple_menu_model.h"

class Browser;

// Menu for the multi-profile button displayed on the browser frame.
class ProfileMenuModel : public ui::SimpleMenuModel,
                         public ui::SimpleMenuModel::Delegate {
 public:
  enum {
    COMMAND_PROFILE_NAME,
    COMMAND_CHOOSE_AVATAR_ICON,
    COMMAND_CUSTOMIZE_PROFILE,
    COMMAND_CREATE_NEW_PROFILE,
    COMMAND_SWITCH_PROFILE_MENU,
    // The profiles submenu contains a menu item for each profile. For the i'th
    // profile the command ID is COMMAND_SWITCH_TO_PROFILE + i. Since there can
    // be any number of profiles this must be the last command id.
    COMMAND_SWITCH_TO_PROFILE,
  };

  explicit ProfileMenuModel(Browser* browser);
  virtual ~ProfileMenuModel();

  // ui::SimpleMenuModel::Delegate implementation
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id, ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  Browser* browser_;
  scoped_ptr<ui::SimpleMenuModel> switch_profiles_sub_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuModel);
};

#endif  // CHROME_BROWSER_UI_PROFILE_MENU_MODEL_H_
