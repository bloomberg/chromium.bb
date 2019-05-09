// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SUB_MENU_MODEL_H_

// This is the secondary menu model of send tab to self context menu item. This
// menu contains the imformation of valid devices, getting form
// SendTabToSelfModel. Every item of this menu is a valid device.

#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

class Profile;

namespace send_tab_to_self {

class SendTabToSelfSubMenuModel : public ui::SimpleMenuModel,
                                  public ui::SimpleMenuModel::Delegate {
 public:
  explicit SendTabToSelfSubMenuModel(Profile* profile);
  ~SendTabToSelfSubMenuModel() override;

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  void Build(Profile* profile);

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfSubMenuModel);
};

}  //  namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_SUB_MENU_MODEL_H_
