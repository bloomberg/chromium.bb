// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_CONTEXTUAL_MENU_H_
#define CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_CONTEXTUAL_MENU_H_

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

namespace extensions {
class ComponentMigrationHelper;
}  // namespace extensions

// The class for the contextual menu for the Media Router action.
class MediaRouterContextualMenu : public ui::SimpleMenuModel::Delegate {
 public:
  explicit MediaRouterContextualMenu(Browser* browser);
  ~MediaRouterContextualMenu() override;

  ui::MenuModel* menu_model() { return &menu_model_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaRouterContextualMenuUnitTest,
                           ToggleCloudServicesItem);
  FRIEND_TEST_ALL_PREFIXES(MediaRouterContextualMenuUnitTest,
                           ToggleAlwaysShowIconItem);

  // Gets or sets the "Always show icon" option.
  bool GetAlwaysShowActionPref() const;
  void SetAlwaysShowActionPref(bool always_show);

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  void ReportIssue();

  Browser* const browser_;
  ui::SimpleMenuModel menu_model_;
  extensions::ComponentMigrationHelper* const component_migration_helper_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterContextualMenu);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_MEDIA_ROUTER_CONTEXTUAL_MENU_H_
