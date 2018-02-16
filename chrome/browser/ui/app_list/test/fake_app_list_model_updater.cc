// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"

#include <utility>

#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "extensions/common/constants.h"

FakeAppListModelUpdater::FakeAppListModelUpdater() {}

FakeAppListModelUpdater::~FakeAppListModelUpdater() {}

app_list::AppListModel* FakeAppListModelUpdater::GetModel() {
  return nullptr;
}

app_list::SearchModel* FakeAppListModelUpdater::GetSearchModel() {
  return nullptr;
}

void FakeAppListModelUpdater::AddItem(std::unique_ptr<ChromeAppListItem> item) {
  items_.push_back(std::move(item));
}

void FakeAppListModelUpdater::AddItemToFolder(
    std::unique_ptr<ChromeAppListItem> item,
    const std::string& folder_id) {
  ChromeAppListItem::TestApi test_api(item.get());
  test_api.SetFolderId(folder_id);
  items_.push_back(std::move(item));
}

void FakeAppListModelUpdater::AddItemToOemFolder(
    std::unique_ptr<ChromeAppListItem> item,
    app_list::AppListSyncableService::SyncItem* oem_sync_item,
    const std::string& oem_folder_id,
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preferred_oem_position) {
  syncer::StringOrdinal position_to_try = preferred_oem_position;
  // If we find a valid postion in the sync item, then we'll try it.
  if (oem_sync_item && oem_sync_item->item_ordinal.IsValid())
    position_to_try = oem_sync_item->item_ordinal;
  // In ash:
  FindOrCreateOemFolder(oem_folder_id, oem_folder_name, position_to_try);
  // In chrome, after oem folder is created:
  AddItemToFolder(std::move(item), oem_folder_id);
}

void FakeAppListModelUpdater::RemoveItem(const std::string& id) {
  size_t index;
  if (FindItemIndexForTest(id, &index))
    items_.erase(items_.begin() + index);
}

void FakeAppListModelUpdater::RemoveUninstalledItem(const std::string& id) {
  RemoveItem(id);
}

void FakeAppListModelUpdater::MoveItemToFolder(const std::string& id,
                                               const std::string& folder_id) {
  size_t index;
  if (FindItemIndexForTest(id, &index)) {
    ChromeAppListItem::TestApi test_api(items_[index].get());
    test_api.SetFolderId(folder_id);
  }
}

void FakeAppListModelUpdater::SetSearchEngineIsGoogle(bool is_google) {
  search_engine_is_google_ = is_google;
}

ChromeAppListItem* FakeAppListModelUpdater::FindItem(const std::string& id) {
  size_t index;
  if (FindItemIndexForTest(id, &index))
    return items_[index].get();
  return nullptr;
}

size_t FakeAppListModelUpdater::ItemCount() {
  return items_.size();
}

ChromeAppListItem* FakeAppListModelUpdater::ItemAtForTest(size_t index) {
  return index < items_.size() ? items_[index].get() : nullptr;
}

bool FakeAppListModelUpdater::FindItemIndexForTest(const std::string& id,
                                                   size_t* index) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i]->id() == id) {
      *index = i;
      return true;
    }
  }
  return false;
}

ChromeAppListItem* FakeAppListModelUpdater::FindFolderItem(
    const std::string& folder_id) {
  ChromeAppListItem* item = FindItem(folder_id);
  return (item && item->is_folder()) ? item : nullptr;
}

void FakeAppListModelUpdater::GetIdToAppListIndexMap(
    GetIdToAppListIndexMapCallback callback) {
  std::unordered_map<std::string, size_t> id_to_app_list_index;
  for (size_t i = 0; i < items_.size(); ++i)
    id_to_app_list_index[items_[i]->id()] = i;
  std::move(callback).Run(id_to_app_list_index);
}

ui::MenuModel* FakeAppListModelUpdater::GetContextMenuModel(
    const std::string& id) {
  return nullptr;
}

void FakeAppListModelUpdater::ActivateChromeItem(const std::string& id,
                                                 int event_flags) {}

size_t FakeAppListModelUpdater::BadgedItemCount() {
  return 0u;
}

bool FakeAppListModelUpdater::SearchEngineIsGoogle() {
  return search_engine_is_google_;
}

app_list::SearchResult* FakeAppListModelUpdater::FindSearchResult(
    const std::string& result_id) {
  for (auto& result : search_results_) {
    if (result->id() == result_id)
      return result.get();
  }
  return nullptr;
}

void FakeAppListModelUpdater::PublishSearchResults(
    std::vector<std::unique_ptr<app_list::SearchResult>> results) {
  search_results_ = std::move(results);
}

ash::mojom::AppListItemMetadataPtr
FakeAppListModelUpdater::FindOrCreateOemFolder(
    const std::string& oem_folder_id,
    const std::string& oem_folder_name,
    const syncer::StringOrdinal& preferred_oem_position) {
  ChromeAppListItem* oem_folder = FindFolderItem(oem_folder_id);
  if (oem_folder) {
    ash::mojom::AppListItemMetadataPtr folder_data =
        oem_folder->CloneMetadata();
    folder_data->name = oem_folder_name;
    oem_folder->SetMetadata(std::move(folder_data));
  } else {
    std::unique_ptr<ChromeAppListItem> new_folder =
        std::make_unique<ChromeAppListItem>(nullptr, oem_folder_id);
    oem_folder = new_folder.get();
    AddItem(std::move(new_folder));
    ash::mojom::AppListItemMetadataPtr folder_data =
        oem_folder->CloneMetadata();
    folder_data->position = preferred_oem_position.IsValid()
                                ? preferred_oem_position
                                : GetOemFolderPos();
    folder_data->name = oem_folder_name;
    oem_folder->SetMetadata(std::move(folder_data));
  }
  return oem_folder->CloneMetadata();
}

syncer::StringOrdinal FakeAppListModelUpdater::GetOemFolderPos() {
  // The oem folder's correct position is based on the item order.
  // We don't have the information in Chrome, so the returned position
  // here is not guaranteed correct.
  size_t web_store_app_index;
  if (!FindItemIndexForTest(extensions::kWebStoreAppId, &web_store_app_index))
    return items_.back()->position().CreateAfter();
  const ChromeAppListItem* web_store_app_item =
      ItemAtForTest(web_store_app_index);
  return web_store_app_item->position().CreateAfter();
}
