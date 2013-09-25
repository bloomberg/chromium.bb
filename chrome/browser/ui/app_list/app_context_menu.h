// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/models/simple_menu_model.h"

class AppListControllerDelegate;
class Profile;

namespace extensions {
class Extension;
class ContextMenuMatcher;
}

namespace app_list {

class AppContextMenuDelegate;

class AppContextMenu : public ui::SimpleMenuModel::Delegate {
 public:
  AppContextMenu(AppContextMenuDelegate* delegate,
                 Profile* profile,
                 const std::string& app_id,
                 AppListControllerDelegate* controller,
                 bool is_platform_app,
                 bool is_search_result_);
  virtual ~AppContextMenu();

  // Note this could return NULL if corresponding extension is gone.
  ui::MenuModel* GetMenuModel();

 private:
  const extensions::Extension* GetExtension() const;
  void ShowExtensionOptions();
  void ShowExtensionDetails();
  void StartExtensionUninstall();

  // ui::SimpleMenuModel::Delegate overrides:
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE;
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* acclelrator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

  AppContextMenuDelegate* delegate_;
  Profile* profile_;
  const std::string app_id_;
  AppListControllerDelegate* controller_;
  bool is_platform_app_;
  bool is_search_result_;

  scoped_ptr<ui::SimpleMenuModel> menu_model_;
  scoped_ptr<extensions::ContextMenuMatcher> extension_menu_items_;

  DISALLOW_COPY_AND_ASSIGN(AppContextMenu);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_CONTEXT_MENU_H_
