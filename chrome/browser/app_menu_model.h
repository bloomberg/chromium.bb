// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_APP_MENU_MODEL_H_
#define CHROME_BROWSER_APP_MENU_MODEL_H_

#include "app/menus/simple_menu_model.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/user_data_manager.h"

class Browser;

// A menu model that builds the contents of the app menu. This menu has only
// one level (no submenus).
class AppMenuModel : public menus::SimpleMenuModel,
                     public GetProfilesHelper::Delegate {
 public:
  explicit AppMenuModel(menus::SimpleMenuModel::Delegate* delegate,
                        Browser* browser);
  virtual ~AppMenuModel();

  // Overridden from GetProfilesHelper::Delegate
  virtual void OnGetProfilesDone(
      const std::vector<std::wstring>& profiles);

 private:
  void Build();

  // Contents of the profiles menu to populate with profile names.
  scoped_ptr<menus::SimpleMenuModel> profiles_menu_contents_;

  // Helper class to enumerate profiles information on the file thread.
  scoped_refptr<GetProfilesHelper> profiles_helper_;

  Browser* browser_;  // weak

  DISALLOW_COPY_AND_ASSIGN(AppMenuModel);
};

#endif  // CHROME_BROWSER_APP_MENU_MODEL_H_
