// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_TEST_LAUNCHER_APPLICATION_MENU_ITEM_MODEL_TEST_API_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_TEST_LAUNCHER_APPLICATION_MENU_ITEM_MODEL_TEST_API_H_

#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"
#include "base/macros.h"

// Test API to provide internal access to a LauncherApplicationMenuItemModel.
class LauncherApplicationMenuItemModelTestAPI {
 public:
  // Creates a test api to access the internals of the |menu_item_model|.
  explicit LauncherApplicationMenuItemModelTestAPI(
      LauncherApplicationMenuItemModel* menu_item_model);
  ~LauncherApplicationMenuItemModelTestAPI();

  // Wrapper function for
  // LauncherApplicationMenuItemModel::GetNumMenuItemsEnabled.
  int GetNumMenuItemsEnabled() const;

  // Wrapper function for
  // LauncherApplicationMenuItemModel::RecordMenuItemSelectedMetrics.
  void RecordMenuItemSelectedMetrics(int command_id,
                                     int num_menu_items_enabled);

 private:
  // The LauncherApplicationMenuItemModel to provide internal access to.
  // Not owned.
  LauncherApplicationMenuItemModel* menu_item_model_;

  DISALLOW_COPY_AND_ASSIGN(LauncherApplicationMenuItemModelTestAPI);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_TEST_LAUNCHER_APPLICATION_MENU_ITEM_MODEL_TEST_API_H_
