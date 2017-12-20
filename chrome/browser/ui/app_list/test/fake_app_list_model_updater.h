// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/app_list/app_list_model_updater.h"

namespace app_list {

class FakeAppListModelUpdater : public AppListModelUpdater {
 public:
  FakeAppListModelUpdater();
  ~FakeAppListModelUpdater() override;

  // AppListModelUpdater:
  // For AppListModel:
  void AddItem(std::unique_ptr<ChromeAppListItem> item) override;
  void AddItemToFolder(std::unique_ptr<ChromeAppListItem> item,
                       const std::string& folder_id) override;
  void RemoveItem(const std::string& id) override;
  void RemoveUninstalledItem(const std::string& id) override;
  void MoveItemToFolder(const std::string& id,
                        const std::string& folder_id) override;
  void SetItemPosition(const std::string& id,
                       const syncer::StringOrdinal& new_position) override;
  // For SearchModel:
  void SetSearchEngineIsGoogle(bool is_google) override;

  // For AppListModel:
  ChromeAppListItem* FindItem(const std::string& id) override;
  size_t ItemCount() override;
  ChromeAppListItem* ItemAtForTest(size_t index) override;
  AppListFolderItem* FindFolderItem(const std::string& folder_id) override;
  bool FindItemIndexForTest(const std::string& id, size_t* index) override;
  app_list::AppListViewState StateFullscreen() override;
  std::map<std::string, size_t> GetIdToAppListIndexMap() override;
  // For SearchModel:
  bool TabletMode() override;
  bool SearchEngineIsGoogle() override;

 private:
  bool search_engine_is_google_ = false;
  std::vector<std::unique_ptr<ChromeAppListItem>> items_;

  DISALLOW_COPY_AND_ASSIGN(FakeAppListModelUpdater);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_TEST_FAKE_APP_LIST_MODEL_UPDATER_H_
