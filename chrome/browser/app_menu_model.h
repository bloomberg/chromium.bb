// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_MENU_MODEL_H_
#define CHROME_BROWSER_APP_MENU_MODEL_H_

#include <set>
#include <vector>

#include "app/menus/simple_menu_model.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/user_data_manager.h"

class Browser;

// A menu model that builds the contents of the app menu.
class AppMenuModel : public menus::SimpleMenuModel {
 public:
  explicit AppMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                        Browser* browser);
  virtual ~AppMenuModel();

  // Override this to handle the sync menu item (whose label is
  // updated dynamically).
  virtual bool IsLabelDynamicAt(int index) const;
  virtual string16 GetLabelAt(int index) const;

  // Build/update profile submenu. Return true if profiles submenu is built or
  // updated. False otherwise.
  bool BuildProfileSubMenu();

 private:
  void Build();

  bool ProfilesChanged(const std::vector<std::wstring>& profiles) const;

  string16 GetSyncMenuLabel() const;
  bool IsSyncItem(int index) const;

  // Contents of the profiles menu to populate with profile names.
  scoped_ptr<menus::SimpleMenuModel> profiles_menu_contents_;

  // Profile names that are in profiles_menu_contents_. This is used to
  // detect profile change.
  std::vector<std::wstring> known_profiles_;

  Browser* browser_;  // weak

  DISALLOW_COPY_AND_ASSIGN(AppMenuModel);
};

#endif  // CHROME_BROWSER_APP_MENU_MODEL_H_
