// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_H_
#define CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "ui/app_list/app_list_item.h"

class AppListControllerDelegate;
class Profile;

namespace extensions {
class AppSorting;
}  // namespace extensions

namespace gfx {
class ImageSkia;
}  // namespace gfx

// Base class of all chrome app list items.
class ChromeAppListItem : public app_list::AppListItem {
 public:
  // AppListControllerDelegate is not properly implemented in tests. Use mock
  // |controller| for unit_tests.
  static void OverrideAppListControllerDelegateForTesting(
      AppListControllerDelegate* controller);

  static gfx::ImageSkia CreateDisabledIcon(const gfx::ImageSkia& icon);

 protected:
  ChromeAppListItem(Profile* profile, const std::string& app_id);
  ~ChromeAppListItem() override;

  Profile* profile() const { return profile_; }

  extensions::AppSorting* GetAppSorting();

  AppListControllerDelegate* GetController();

  // Updates item position and name from |sync_item|. |sync_item| must be valid.
  void UpdateFromSync(
      const app_list::AppListSyncableService::SyncItem* sync_item);

  // Set the default position if it exists.
  void SetDefaultPositionIfApplicable();

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppListItem);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_H_
