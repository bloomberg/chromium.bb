// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"

#include <utility>

namespace app_list {

FakeAppListModelUpdater::FakeAppListModelUpdater() {}

FakeAppListModelUpdater::~FakeAppListModelUpdater() {}

void FakeAppListModelUpdater::AddItem(std::unique_ptr<AppListItem> item) {
  items_.push_back(std::move(item));
}

void FakeAppListModelUpdater::AddItemToFolder(std::unique_ptr<AppListItem> item,
                                              const std::string& folder_id) {
  items_.push_back(std::move(item));
}

void FakeAppListModelUpdater::RemoveItem(const std::string& id) {
  size_t index;
  if (FindItemIndex(id, &index))
    items_.erase(items_.begin() + index);
}

void FakeAppListModelUpdater::RemoveUninstalledItem(const std::string& id) {
  RemoveItem(id);
}

void FakeAppListModelUpdater::SetSearchEngineIsGoogle(bool is_google) {
  search_engine_is_google_ = is_google;
}

AppListItem* FakeAppListModelUpdater::FindItem(const std::string& id) {
  size_t index;
  if (FindItemIndex(id, &index))
    return items_[index].get();
  return nullptr;
}

size_t FakeAppListModelUpdater::ItemCount() {
  return items_.size();
}

AppListItem* FakeAppListModelUpdater::ItemAt(size_t index) {
  return index < items_.size() ? items_[index].get() : nullptr;
}

bool FakeAppListModelUpdater::FindItemIndex(const std::string& id,
                                            size_t* index) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i]->id() == id) {
      *index = i;
      return true;
    }
  }
  return false;
}

AppListFolderItem* FakeAppListModelUpdater::FindFolderItem(
    const std::string& folder_id) {
  return nullptr;
}

bool FakeAppListModelUpdater::TabletMode() {
  return false;
}

app_list::AppListViewState FakeAppListModelUpdater::StateFullscreen() {
  return app_list::AppListViewState::CLOSED;
}

bool FakeAppListModelUpdater::SearchEngineIsGoogle() {
  return search_engine_is_google_;
}

}  // namespace app_list
