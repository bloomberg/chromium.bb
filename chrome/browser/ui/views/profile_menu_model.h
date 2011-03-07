// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILE_MENU_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILE_MENU_MODEL_H_
#pragma once

#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/point.h"

namespace ui {
class Accelerator;
}

namespace views {
class Menu2;

// ProfileMenuModel
//
// Menu for the multi-profile button displayed on the browser frame when the
// user is in a multi-profile-enabled account. Stub for now. TODO(mirandac):
// enable and fill in as part of multi-profile work.

class ProfileMenuModel : public ui::SimpleMenuModel,
                         public ui::SimpleMenuModel::Delegate {
 public:
  ProfileMenuModel();
  virtual ~ProfileMenuModel();

  void RunMenuAt(const gfx::Point& point);

  // ui::SimpleMenuModel::Delegate implementation
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          ui::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

 private:
  scoped_ptr<views::Menu2> menu_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuModel);
};

}  // namespace views

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILE_MENU_MODEL_H_


