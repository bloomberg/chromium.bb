// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/public/interfaces/app_list.mojom.h"
#include "base/callback_forward.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"

class ChromeAppListItem;

// An interface to wrap AppListModel access in browser.
class AppListModelUpdater {
 public:
  class TestApi {
   public:
    explicit TestApi(AppListModelUpdater* model_updater)
        : model_updater_(model_updater) {}
    ~TestApi() = default;

    void SetItemPosition(const std::string& id,
                         const syncer::StringOrdinal& new_position) {
      model_updater_->SetItemPosition(id, new_position);
    }

   private:
    AppListModelUpdater* const model_updater_;
  };

  // For AppListModel:
  virtual void AddItem(std::unique_ptr<ChromeAppListItem> item) {}
  virtual void AddItemToFolder(std::unique_ptr<ChromeAppListItem> item,
                               const std::string& folder_id) {}
  virtual void RemoveItem(const std::string& id) {}
  virtual void RemoveUninstalledItem(const std::string& id) {}
  virtual void MoveItemToFolder(const std::string& id,
                                const std::string& folder_id) {}
  virtual void SetStatus(ash::AppListModelStatus status) {}
  virtual void SetState(ash::AppListState state) {}
  virtual void HighlightItemInstalledFromUI(const std::string& id) {}
  // For SearchModel:
  virtual void SetSearchEngineIsGoogle(bool is_google) {}
  virtual void SetSearchTabletAndClamshellAccessibleName(
      const base::string16& tablet_accessible_name,
      const base::string16& clamshell_accessible_name) {}
  virtual void SetSearchHintText(const base::string16& hint_text) {}
  virtual void UpdateSearchBox(const base::string16& text,
                               bool initiated_by_user) {}
  virtual void PublishSearchResults(
      std::vector<std::unique_ptr<app_list::SearchResult>> results) {}

  // Item field setters only used by ChromeAppListItem and its derived classes.
  virtual void SetItemIcon(const std::string& id, const gfx::ImageSkia& icon) {}
  virtual void SetItemName(const std::string& id, const std::string& name) {}
  virtual void SetItemNameAndShortName(const std::string& id,
                                       const std::string& name,
                                       const std::string& short_name) {}
  virtual void SetItemPosition(const std::string& id,
                               const syncer::StringOrdinal& new_position) {}
  virtual void SetItemFolderId(const std::string& id,
                               const std::string& folder_id) {}
  virtual void SetItemIsInstalling(const std::string& id, bool is_installing) {}
  virtual void SetItemPercentDownloaded(const std::string& id,
                                        int32_t percent_downloaded) {}

  // For AppListModel:
  virtual ChromeAppListItem* FindItem(const std::string& id) = 0;
  virtual size_t ItemCount() = 0;
  virtual ChromeAppListItem* ItemAtForTest(size_t index) = 0;
  virtual ChromeAppListItem* FindFolderItem(const std::string& folder_id) = 0;
  virtual bool FindItemIndexForTest(const std::string& id, size_t* index) = 0;
  using GetIdToAppListIndexMapCallback =
      base::OnceCallback<void(const std::unordered_map<std::string, size_t>&)>;
  virtual void GetIdToAppListIndexMap(GetIdToAppListIndexMapCallback callback) {
  }
  virtual void ContextMenuItemSelected(const std::string& id,
                                       int command_id,
                                       int event_flags) {}

  // Methods for AppListSyncableService:
  virtual void AddItemToOemFolder(
      std::unique_ptr<ChromeAppListItem> item,
      app_list::AppListSyncableService::SyncItem* oem_sync_item,
      const std::string& oem_folder_id,
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preffered_oem_position) {}
  using ResolveOemFolderPositionCallback =
      base::OnceCallback<void(ChromeAppListItem*)>;
  virtual void ResolveOemFolderPosition(
      const std::string& oem_folder_id,
      const syncer::StringOrdinal& preffered_oem_position,
      ResolveOemFolderPositionCallback callback) {}
  virtual void UpdateAppItemFromSyncItem(
      app_list::AppListSyncableService::SyncItem* sync_item,
      bool update_name,
      bool update_folder) {}

  virtual size_t BadgedItemCount() = 0;
  // For SearchModel:
  virtual bool SearchEngineIsGoogle() = 0;

 protected:
  virtual ~AppListModelUpdater() {}
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_
