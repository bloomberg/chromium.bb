// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_

#include <map>
#include <memory>
#include <string>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

namespace app_list {

// An interface to wrap AppListModel access in browser.
class AppListModelUpdater {
 public:
  // For AppListModel:
  virtual void AddItem(std::unique_ptr<ChromeAppListItem> item) {}
  virtual void AddItemToFolder(std::unique_ptr<ChromeAppListItem> item,
                               const std::string& folder_id) {}
  virtual void RemoveItem(const std::string& id) {}
  virtual void RemoveUninstalledItem(const std::string& id) {}
  virtual void MoveItemToFolder(const std::string& id,
                                const std::string& folder_id) {}
  virtual void SetItemPosition(const std::string& id,
                               const syncer::StringOrdinal& new_position) {}
  virtual void SetStatus(AppListModel::Status status) {}
  virtual void SetState(AppListModel::State state) {}
  virtual void SetItemName(const std::string& id, const std::string& name) {}
  virtual void HighlightItemInstalledFromUI(const std::string& id) {}
  // For SearchModel:
  virtual void SetSearchEngineIsGoogle(bool is_google) {}

  // For AppListModel:
  virtual ChromeAppListItem* FindItem(const std::string& id) = 0;
  virtual size_t ItemCount() = 0;
  virtual ChromeAppListItem* ItemAtForTest(size_t index) = 0;
  // TODO(hejq): |FindFolderItem| will return |ChromeAppListItem|.
  virtual AppListFolderItem* FindFolderItem(const std::string& folder_id) = 0;
  virtual bool FindItemIndexForTest(const std::string& id, size_t* index) = 0;
  virtual app_list::AppListViewState StateFullscreen() = 0;
  virtual std::map<std::string, size_t> GetIdToAppListIndexMap() = 0;
  // For SearchModel:
  virtual bool TabletMode() = 0;
  virtual bool SearchEngineIsGoogle() = 0;

 protected:
  virtual ~AppListModelUpdater() {}
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_
