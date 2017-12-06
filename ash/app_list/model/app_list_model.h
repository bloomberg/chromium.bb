// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_LIST_MODEL_H_
#define ASH_APP_LIST_MODEL_APP_LIST_MODEL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_item_list_observer.h"
#include "ash/app_list/model/app_list_model_export.h"
#include "ash/app_list/model/app_list_view_state.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace app_list {

class AppListFolderItem;
class AppListItem;
class AppListItemList;
class AppListModelObserver;

// Master model of app list that holds AppListItemList, which owns a list
// of AppListItems and is displayed in the grid view.
// NOTE: Currently this class observes |top_level_item_list_|. The View code may
// move entries in the item list directly (but can not add or remove them) and
// the model needs to notify its observers when this occurs.
class APP_LIST_MODEL_EXPORT AppListModel : public AppListItemListObserver {
 public:
  enum Status {
    STATUS_NORMAL,
    STATUS_SYNCING,  // Syncing apps or installing synced apps.
  };

  // Do not change the order of these as they are used for metrics.
  enum State {
    STATE_APPS = 0,
    STATE_SEARCH_RESULTS,
    STATE_START,
    STATE_CUSTOM_LAUNCHER_PAGE,
    // Add new values here.

    INVALID_STATE,
    STATE_LAST = INVALID_STATE,
  };

  AppListModel();
  ~AppListModel() override;

  void AddObserver(AppListModelObserver* observer);
  void RemoveObserver(AppListModelObserver* observer);

  void SetStatus(Status status);

  void SetState(State state);
  State state() const { return state_; }

  // The current state of the AppListView. Controlled by AppListView.
  void SetStateFullscreen(AppListViewState state);
  AppListViewState state_fullscreen() const { return state_fullscreen_; }

  // Finds the item matching |id|.
  AppListItem* FindItem(const std::string& id);

  // Find a folder item matching |id|.
  AppListFolderItem* FindFolderItem(const std::string& id);

  // Adds |item| to the model. The model takes ownership of |item|. Returns a
  // pointer to the item that is safe to use (e.g. after passing ownership).
  AppListItem* AddItem(std::unique_ptr<AppListItem> item);

  // Adds |item| to an existing folder or creates a new folder. If |folder_id|
  // is empty, adds the item to the top level model instead. The model takes
  // ownership of |item|. Returns a pointer to the item that is safe to use.
  AppListItem* AddItemToFolder(std::unique_ptr<AppListItem> item,
                               const std::string& folder_id);

  // Merges two items. If the target item is a folder, the source item is
  // added to the end of the target folder. Otherwise a new folder is created
  // in the same position as the target item with the target item as the first
  // item in the new folder and the source item as the second item. Returns
  // the id of the target folder, or an empty string if the merge failed. The
  // source item may already be in a folder. See also the comment for
  // RemoveItemFromFolder. NOTE: This should only be called by the View code
  // (not the sync code); it enforces folder restrictions (e.g. the target can
  // not be an OEM folder).
  const std::string MergeItems(const std::string& target_item_id,
                               const std::string& source_item_id);

  // Move |item| to the folder matching |folder_id| or to the top level if
  // |folder_id| is empty. |item|->position will determine where the item
  // is positioned. See also the comment for RemoveItemFromFolder.
  void MoveItemToFolder(AppListItem* item, const std::string& folder_id);

  // Move |item| to the folder matching |folder_id| or to the top level if
  // |folder_id| is empty. The item will be inserted before |position| or at
  // the end of the list if |position| is invalid. Note: |position| is copied
  // in case it refers to the containing folder which may get deleted. See
  // also the comment for RemoveItemFromFolder. Returns true if the item was
  // moved. NOTE: This should only be called by the View code (not the sync
  // code); it enforces folder restrictions (e.g. the source folder can not be
  // type OEM).
  bool MoveItemToFolderAt(AppListItem* item,
                          const std::string& folder_id,
                          syncer::StringOrdinal position);

  // Sets the position of |item| either in |top_level_item_list_| or the
  // folder specified by |item|->folder_id(). If |new_position| is invalid,
  // move the item to the end of the list.
  void SetItemPosition(AppListItem* item,
                       const syncer::StringOrdinal& new_position);

  // Sets the name of |item| and notifies observers.
  void SetItemName(AppListItem* item, const std::string& name);

  // Sets the name and short name of |item| and notifies observers.
  void SetItemNameAndShortName(AppListItem* item,
                               const std::string& name,
                               const std::string& short_name);

  // Deletes the item matching |id| from |top_level_item_list_| or from the
  // appropriate folder.
  void DeleteItem(const std::string& id);

  // Wrapper around DeleteItem() which will also clean up if its parent folder
  // has a single child left.
  void DeleteUninstalledItem(const std::string& id);

  // Sets whether or not the custom launcher page should be enabled.
  void SetCustomLauncherPageEnabled(bool enabled);
  bool custom_launcher_page_enabled() const {
    return custom_launcher_page_enabled_;
  }

  void set_custom_launcher_page_name(const std::string& name) {
    custom_launcher_page_name_ = name;
  }

  const std::string& custom_launcher_page_name() const {
    return custom_launcher_page_name_;
  }

  // Pushes a custom launcher page's subpage into the state stack in the
  // model.
  void PushCustomLauncherPageSubpage();

  // If the state stack is not empty, pops a subpage from the stack and
  // returns true. Returns false if the stack was empty.
  bool PopCustomLauncherPageSubpage();

  // Clears the custom launcher page's subpage state stack from the model.
  void ClearCustomLauncherPageSubpages();

  int custom_launcher_page_subpage_depth() {
    return custom_launcher_page_subpage_depth_;
  }

  AppListItemList* top_level_item_list() { return top_level_item_list_.get(); }

  Status status() const { return status_; }

 private:
  // AppListItemListObserver
  void OnListItemMoved(size_t from_index,
                       size_t to_index,
                       AppListItem* item) override;

  // Returns an existing folder matching |folder_id| or creates a new folder.
  AppListFolderItem* FindOrCreateFolderItem(const std::string& folder_id);

  // Adds |item_ptr| to |top_level_item_list_| and notifies observers.
  AppListItem* AddItemToItemListAndNotify(
      std::unique_ptr<AppListItem> item_ptr);

  // Adds |item_ptr| to |top_level_item_list_| and notifies observers that an
  // Update occured (e.g. item moved from a folder).
  AppListItem* AddItemToItemListAndNotifyUpdate(
      std::unique_ptr<AppListItem> item_ptr);

  // Adds |item_ptr| to |folder| and notifies observers.
  AppListItem* AddItemToFolderItemAndNotify(
      AppListFolderItem* folder,
      std::unique_ptr<AppListItem> item_ptr);

  // Removes |item| from |top_level_item_list_| or calls RemoveItemFromFolder
  // if |item|->folder_id is set.
  std::unique_ptr<AppListItem> RemoveItem(AppListItem* item);

  // Removes |item| from |folder|. If |folder| becomes empty, deletes |folder|
  // from |top_level_item_list_|. Does NOT trigger observers, calling function
  // must do so.
  std::unique_ptr<AppListItem> RemoveItemFromFolder(AppListFolderItem* folder,
                                                    AppListItem* item);

  std::unique_ptr<AppListItemList> top_level_item_list_;

  Status status_ = STATUS_NORMAL;
  State state_ = INVALID_STATE;
  // The AppListView state. Controlled by the AppListView.
  AppListViewState state_fullscreen_ = AppListViewState::CLOSED;
  base::ObserverList<AppListModelObserver, true> observers_;
  bool custom_launcher_page_enabled_ = true;
  std::string custom_launcher_page_name_;

  // The current number of subpages the custom launcher page has pushed.
  int custom_launcher_page_subpage_depth_;

  DISALLOW_COPY_AND_ASSIGN(AppListModel);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_MODEL_APP_LIST_MODEL_H_
