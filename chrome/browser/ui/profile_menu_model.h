// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILE_MENU_MODEL_H_
#define CHROME_BROWSER_UI_PROFILE_MENU_MODEL_H_
#pragma once

#include "ui/base/models/simple_menu_model.h"

// ProfileMenuModel
//
// Menu for the multi-profile button displayed on the browser frame when the
// user is in a multi-profile-enabled account. Stub for now. TODO(mirandac):
// enable and fill in as part of multi-profile work.
class ProfileMenuModel : public ui::SimpleMenuModel,
                         public ui::SimpleMenuModel::Delegate {
 public:
  explicit ProfileMenuModel();
  virtual ~ProfileMenuModel();

  // ui::SimpleMenuModel::Delegate implementation
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id, ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  enum {
    COMMAND_CREATE_NEW_PROFILE,
  };

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuModel);
};

#endif  // CHROME_BROWSER_UI_PROFILE_MENU_MODEL_H_


