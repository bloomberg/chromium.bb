// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/models/simple_menu_model.h"

class AppListControllerDelegate;
class Profile;

namespace extensions {
class ContextMenuMatcher;
}

namespace app_list {

class AppContextMenuDelegate;

class AppContextMenu : public ui::SimpleMenuModel::Delegate {
 public:
  enum CommandId {
    LAUNCH_NEW = 100,
    TOGGLE_PIN,
    CREATE_SHORTCUTS,
    SHOW_APP_INFO,
    OPTIONS,
    UNINSTALL,
    REMOVE_FROM_FOLDER,
    MENU_NEW_WINDOW,
    MENU_NEW_INCOGNITO_WINDOW,
    // Order matters in USE_LAUNCH_TYPE_* and must match the LaunchType enum.
    USE_LAUNCH_TYPE_COMMAND_START = 200,
    USE_LAUNCH_TYPE_PINNED = USE_LAUNCH_TYPE_COMMAND_START,
    USE_LAUNCH_TYPE_REGULAR,
    USE_LAUNCH_TYPE_FULLSCREEN,
    USE_LAUNCH_TYPE_WINDOW,
    USE_LAUNCH_TYPE_COMMAND_END,
  };

  AppContextMenu(AppContextMenuDelegate* delegate,
                 Profile* profile,
                 const std::string& app_id,
                 AppListControllerDelegate* controller);
  ~AppContextMenu() override;

  static void DisableInstalledExtensionCheckForTesting(bool disable);

  // Note this could return NULL if corresponding extension is gone.
  ui::MenuModel* GetMenuModel();

  void set_is_platform_app(bool is_platform_app) {
    is_platform_app_ = is_platform_app;
  }
  void set_is_search_result(bool is_search_result) {
    is_search_result_ = is_search_result;
  }
  void set_is_in_folder(bool is_in_folder) {
    is_in_folder_ = is_in_folder;
  }

 private:
  // ui::SimpleMenuModel::Delegate overrides:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override;
  void ExecuteCommand(int command_id, int event_flags) override;

  AppContextMenuDelegate* delegate_;
  Profile* profile_;
  const std::string app_id_;
  AppListControllerDelegate* controller_;
  bool is_platform_app_;
  bool is_search_result_;
  bool is_in_folder_;

  scoped_ptr<ui::SimpleMenuModel> menu_model_;
  scoped_ptr<extensions::ContextMenuMatcher> extension_menu_items_;

  DISALLOW_COPY_AND_ASSIGN(AppContextMenu);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_
