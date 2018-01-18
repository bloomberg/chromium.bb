// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/model/speech/speech_ui_model.h"
#include "base/strings/string16.h"

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
  virtual void SetStatus(app_list::AppListModel::Status status) {}
  virtual void SetState(app_list::AppListModel::State state) {}
  virtual void HighlightItemInstalledFromUI(const std::string& id) {}
  // For SearchModel:
  virtual void SetSearchEngineIsGoogle(bool is_google) {}
  virtual void SetSearchTabletAndClamshellAccessibleName(
      const base::string16& tablet_accessible_name,
      const base::string16& clamshell_accessible_name) {}
  virtual void SetSearchHintText(const base::string16& hint_text) {}
  virtual void SetSearchSpeechRecognitionButton(
      app_list::SpeechRecognitionState state) {}
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
  // TODO(hejq): |FindFolderItem| will return |ChromeAppListItem|.
  virtual app_list::AppListFolderItem* FindFolderItem(
      const std::string& folder_id) = 0;
  virtual bool FindItemIndexForTest(const std::string& id, size_t* index) = 0;
  virtual app_list::AppListViewState StateFullscreen() = 0;
  virtual std::map<std::string, size_t> GetIdToAppListIndexMap() = 0;
  // For SearchModel:
  virtual bool TabletMode() = 0;
  virtual bool SearchEngineIsGoogle() = 0;

 protected:
  virtual ~AppListModelUpdater() {}
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_MODEL_UPDATER_H_
