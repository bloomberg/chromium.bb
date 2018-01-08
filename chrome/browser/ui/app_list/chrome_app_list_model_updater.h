// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"

namespace ui {
class MenuModel;
}  // namespace ui

class ChromeAppListItem;
class SearchModel;

class ChromeAppListModelUpdater : public AppListModelUpdater {
 public:
  ChromeAppListModelUpdater();
  ~ChromeAppListModelUpdater() override;

  // AppListModelUpdater:
  void AddItem(std::unique_ptr<ChromeAppListItem> app_item) override;
  void AddItemToFolder(std::unique_ptr<ChromeAppListItem> app_item,
                       const std::string& folder_id) override;
  void RemoveItem(const std::string& id) override;
  void RemoveUninstalledItem(const std::string& id) override;
  void MoveItemToFolder(const std::string& id,
                        const std::string& folder_id) override;
  void SetStatus(app_list::AppListModel::Status status) override;
  void SetState(app_list::AppListModel::State state) override;
  void HighlightItemInstalledFromUI(const std::string& id) override;
  void SetSearchEngineIsGoogle(bool is_google) override;
  void SetSearchTabletAndClamshellAccessibleName(
      const base::string16& tablet_accessible_name,
      const base::string16& clamshell_accessible_name) override;
  void SetSearchHintText(const base::string16& hint_text) override;
  void SetSearchSpeechRecognitionButton(
      app_list::SpeechRecognitionState state) override;
  void UpdateSearchBox(const base::string16& text,
                       bool initiated_by_user) override;

  // Methods only for visiting Chrome items that never talk to ash.
  void ActivateChromeItem(const std::string& id, int event_flags);

  // Methods for item querying.
  ChromeAppListItem* FindItem(const std::string& id) override;
  size_t ItemCount() override;
  ChromeAppListItem* ItemAtForTest(size_t index) override;
  app_list::AppListFolderItem* FindFolderItem(
      const std::string& folder_id) override;
  bool FindItemIndexForTest(const std::string& id, size_t* index) override;
  bool TabletMode() override;
  app_list::AppListViewState StateFullscreen() override;
  bool SearchEngineIsGoogle() override;
  std::map<std::string, size_t> GetIdToAppListIndexMap() override;
  ui::MenuModel* GetContextMenuModel(const std::string& id);

  // Methods for AppListSyncableService:
  void AddItemToOemFolder(
      std::unique_ptr<ChromeAppListItem> item,
      app_list::AppListSyncableService::SyncItem* oem_sync_item,
      const std::string& oem_folder_id,
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preffered_oem_position);
  app_list::AppListFolderItem* ResolveOemFolderPosition(
      const std::string& oem_folder_id,
      const syncer::StringOrdinal& preffered_oem_position);
  void UpdateAppItemFromSyncItem(
      app_list::AppListSyncableService::SyncItem* sync_item,
      bool update_name,
      bool update_folder);

 protected:
  // AppListModelUpdater:
  // Methods only used by ChromeAppListItem that talk to ash directly.
  void SetItemIcon(const std::string& id, const gfx::ImageSkia& icon) override;
  void SetItemName(const std::string& id, const std::string& name) override;
  void SetItemNameAndShortName(const std::string& id,
                               const std::string& name,
                               const std::string& short_name) override;
  void SetItemPosition(const std::string& id,
                       const syncer::StringOrdinal& new_position) override;
  void SetItemFolderId(const std::string& id,
                       const std::string& folder_id) override;
  void SetItemIsInstalling(const std::string& id, bool is_installing) override;
  void SetItemPercentDownloaded(const std::string& id,
                                int32_t percent_downloaded) override;

 private:
  // TODO(hejq): Remove this friend. Currently |model_| and |search_model_| are
  // exposed to AppListViewDelegate via AppListSyncableService. We'll remove
  // this once we remove AppListViewDelegate.
  friend class app_list::AppListSyncableService;

  void FindOrCreateOemFolder(
      app_list::AppListSyncableService::SyncItem* oem_sync_item,
      const std::string& oem_folder_id,
      const std::string& oem_folder_name,
      const syncer::StringOrdinal& preffered_oem_position);
  syncer::StringOrdinal GetOemFolderPos();

  std::unique_ptr<app_list::AppListModel> model_;
  std::unique_ptr<app_list::SearchModel> search_model_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppListModelUpdater);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_CHROME_APP_LIST_MODEL_UPDATER_H_
