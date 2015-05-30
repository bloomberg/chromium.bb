// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/test/launcher_application_menu_item_model_test_api.h"

LauncherApplicationMenuItemModelTestAPI::
    LauncherApplicationMenuItemModelTestAPI(
        LauncherApplicationMenuItemModel* menu_item_model)
    : menu_item_model_(menu_item_model) {
}

LauncherApplicationMenuItemModelTestAPI::
    ~LauncherApplicationMenuItemModelTestAPI() {
}

int LauncherApplicationMenuItemModelTestAPI::GetNumMenuItemsEnabled() const {
  return menu_item_model_->GetNumMenuItemsEnabled();
}

void LauncherApplicationMenuItemModelTestAPI::RecordMenuItemSelectedMetrics(
    int command_id,
    int num_menu_items_enabled) {
  return menu_item_model_->RecordMenuItemSelectedMetrics(
      command_id, num_menu_items_enabled);
}
