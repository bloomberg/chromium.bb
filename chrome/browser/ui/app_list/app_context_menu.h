// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "ui/base/models/simple_menu_model.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {

class AppContextMenuDelegate;

// Base class of all context menus in app list view.
class AppContextMenu : public ui::SimpleMenuModel::Delegate {
 public:
  // Defines command ids, used in context menu of all types.
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

  ~AppContextMenu() override;

  // Note this could return nullptr if corresponding app item is gone.
  virtual ui::MenuModel* GetMenuModel();

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 protected:
  AppContextMenu(AppContextMenuDelegate* delegate,
                 Profile* profile,
                 const std::string& app_id,
                 AppListControllerDelegate* controller);

  // Creates default items, derived class may override to add their specific
  // items.
  virtual void BuildMenu(ui::SimpleMenuModel* menu_model);

  // Helper that toggles pinning state of provided app.
  void TogglePin(const std::string& shelf_app_id);

  const std::string& app_id() const { return app_id_; }
  Profile* profile() const { return profile_; }
  AppContextMenuDelegate* delegate() const { return delegate_; }
  AppListControllerDelegate* controller() const { return controller_; }

 private:
  AppContextMenuDelegate* delegate_;
  Profile* profile_;
  const std::string app_id_;
  AppListControllerDelegate* controller_;

  std::unique_ptr<ui::SimpleMenuModel> menu_model_;

  DISALLOW_COPY_AND_ASSIGN(AppContextMenu);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_
