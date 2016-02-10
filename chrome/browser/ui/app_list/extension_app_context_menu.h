// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_CONTEXT_MENU_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_context_menu.h"

class AppListControllerDelegate;
class Profile;

namespace extensions {
class ContextMenuMatcher;
}

namespace app_list {

class AppContextMenuDelegate;

class ExtensionAppContextMenu : public AppContextMenu {
 public:
  ExtensionAppContextMenu(AppContextMenuDelegate* delegate,
                          Profile* profile,
                          const std::string& app_id,
                          AppListControllerDelegate* controller);
  ~ExtensionAppContextMenu() override;

  static void DisableInstalledExtensionCheckForTesting(bool disable);

  // AppListContextMenu overrides:
  ui::MenuModel* GetMenuModel() override;
  void BuildMenu(ui::SimpleMenuModel* menu_model) override;

  // ui::SimpleMenuModel::Delegate overrides:
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  void set_is_platform_app(bool is_platform_app) {
    is_platform_app_ = is_platform_app;
  }

 private:
  bool is_platform_app_ = false;

  scoped_ptr<extensions::ContextMenuMatcher> extension_menu_items_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppContextMenu);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_EXTENSION_APP_CONTEXT_MENU_H_
