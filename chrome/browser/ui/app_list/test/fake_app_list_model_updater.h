// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ash/app_list/model/search/search_result.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"

class ChromeAppListItem;

class FakeAppListModelUpdater : public AppListModelUpdater {
 public:
  FakeAppListModelUpdater();
  ~FakeAppListModelUpdater() override;

  // AppListModelUpdater:
  app_list::AppListModel* GetModel() override;
  app_list::SearchModel* GetSearchModel() override;
  // For AppListModel:
  void AddItem(std::unique_ptr<ChromeAppListItem> item) override;
  void AddItemToFolder(std::unique_ptr<ChromeAppListItem> item,
                       const std::string& folder_id) override;
  void AddItemToOemFolder(
      std::unique_ptr<ChromeAppListItem> item,
      app_list::AppListSyncableService::SyncItem* oem_sync_item,
      const std::string& oem_folder_id,
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preferred_oem_position) override;
  void RemoveItem(const std::string& id) override;
  void RemoveUninstalledItem(const std::string& id) override;
  void MoveItemToFolder(const std::string& id,
                        const std::string& folder_id) override;
  // For SearchModel:
  void SetSearchEngineIsGoogle(bool is_google) override;
  void PublishSearchResults(
      std::vector<std::unique_ptr<app_list::SearchResult>> results) override;

  void ActivateChromeItem(const std::string& id, int event_flags) override;

  // For AppListModel:
  ChromeAppListItem* FindItem(const std::string& id) override;
  size_t ItemCount() override;
  ChromeAppListItem* ItemAtForTest(size_t index) override;
  ChromeAppListItem* FindFolderItem(const std::string& folder_id) override;
  bool FindItemIndexForTest(const std::string& id, size_t* index) override;
  void GetIdToAppListIndexMap(GetIdToAppListIndexMapCallback callback) override;
  ui::MenuModel* GetContextMenuModel(const std::string& id) override;
  size_t BadgedItemCount() override;
  // For SearchModel:
  bool SearchEngineIsGoogle() override;
  app_list::SearchResult* FindSearchResult(
      const std::string& result_id) override;
  const std::vector<std::unique_ptr<app_list::SearchResult>>& search_results()
      const {
    return search_results_;
  }

  void SetDelegate(AppListModelUpdaterDelegate* delegate) override {}

 private:
  bool search_engine_is_google_ = false;
  std::vector<std::unique_ptr<ChromeAppListItem>> items_;
  std::vector<std::unique_ptr<app_list::SearchResult>> search_results_;

  ash::mojom::AppListItemMetadataPtr FindOrCreateOemFolder(
      const std::string& oem_folder_id,
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preferred_oem_position);
  syncer::StringOrdinal GetOemFolderPos();

  DISALLOW_COPY_AND_ASSIGN(FakeAppListModelUpdater);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_
