// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_H_
#define CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_H_

#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/model/app_list_item.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"

class AppListControllerDelegate;
class Profile;

namespace extensions {
class AppSorting;
}  // namespace extensions

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ui {
class MenuModel;
}  // namespace ui

// Base class of all chrome app list items.
class ChromeAppListItem : public app_list::AppListItem {
 public:
  class TestApi {
   public:
    explicit TestApi(ChromeAppListItem* item) : item_(item) {}
    ~TestApi() = default;

    void set_folder_id(const std::string& folder_id) {
      item_->set_folder_id(folder_id);
    }

    void set_position(const syncer::StringOrdinal& position) {
      item_->set_position(position);
    }

   private:
    ChromeAppListItem* item_;
  };

  ChromeAppListItem(Profile* profile, const std::string& app_id);
  ~ChromeAppListItem() override;

  // AppListControllerDelegate is not properly implemented in tests. Use mock
  // |controller| for unit_tests.
  static void OverrideAppListControllerDelegateForTesting(
      AppListControllerDelegate* controller);

  static gfx::ImageSkia CreateDisabledIcon(const gfx::ImageSkia& icon);

  // Activates (opens) the item. Does nothing by default.
  virtual void Activate(int event_flags);

  // Returns a static const char* identifier for the subclass (defaults to "").
  // Pointers can be compared for quick type checking.
  const char* GetItemType() const override;

  // Returns the context menu model for this item, or NULL if there is currently
  // no menu for the item (e.g. during install).
  // Note the returned menu model is owned by this item.
  virtual ui::MenuModel* GetContextMenuModel();

  bool CompareForTest(const app_list::AppListItem* other) const override;

  std::string ToDebugString() const override;

 protected:
  // TODO(hejq): break the inheritance and remove this.
  friend class ChromeAppListModelUpdater;

  Profile* profile() const { return profile_; }

  extensions::AppSorting* GetAppSorting();

  AppListControllerDelegate* GetController();

  AppListModelUpdater* model_updater() { return model_updater_; }
  void set_model_updater(AppListModelUpdater* model_updater) {
    model_updater_ = model_updater;
  }

  // Updates item position and name from |sync_item|. |sync_item| must be valid.
  void UpdateFromSync(
      const app_list::AppListSyncableService::SyncItem* sync_item);

  // Set the default position if it exists.
  void SetDefaultPositionIfApplicable();

 private:
  Profile* profile_;
  AppListModelUpdater* model_updater_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppListItem);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_ITEM_H_
