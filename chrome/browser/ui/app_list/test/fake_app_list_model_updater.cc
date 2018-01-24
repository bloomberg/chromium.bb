// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"

#include <utility>

#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

FakeAppListModelUpdater::FakeAppListModelUpdater() {}

FakeAppListModelUpdater::~FakeAppListModelUpdater() {}

void FakeAppListModelUpdater::AddItem(std::unique_ptr<ChromeAppListItem> item) {
  items_.push_back(std::move(item));
}

void FakeAppListModelUpdater::AddItemToFolder(
    std::unique_ptr<ChromeAppListItem> item,
    const std::string& folder_id) {
  ChromeAppListItem::TestApi test_api(item.get());
  test_api.set_folder_id(folder_id);
  items_.push_back(std::move(item));
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
    test_api.set_folder_id(folder_id);
  }
}

void FakeAppListModelUpdater::SetItemPosition(
    const std::string& id,
    const syncer::StringOrdinal& new_position) {
  size_t index;
  if (FindItemIndexForTest(id, &index)) {
    ChromeAppListItem::TestApi test_api(items_[index].get());
    test_api.set_position(new_position);
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

app_list::AppListFolderItem* FakeAppListModelUpdater::FindFolderItem(
    const std::string& folder_id) {
  return nullptr;
}

std::map<std::string, size_t>
FakeAppListModelUpdater::GetIdToAppListIndexMap() {
  std::map<std::string, size_t> id_to_app_list_index;
  for (size_t i = 0; i < items_.size(); ++i)
    id_to_app_list_index[items_[i]->id()] = i;
  return id_to_app_list_index;
}

size_t FakeAppListModelUpdater::BadgedItemCount() {
  return 0u;
}

bool FakeAppListModelUpdater::SearchEngineIsGoogle() {
  return search_engine_is_google_;
}

void FakeAppListModelUpdater::PublishSearchResults(
    std::vector<std::unique_ptr<app_list::SearchResult>> results) {
  search_results_ = std::move(results);
}
