// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_MASH_H_
#define ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_MASH_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/app_list/app_list_view_delegate.h"

namespace app_list {
class AppListModel;
class SearchModel;
}  // namespace app_list

namespace ash {

class AppListControllerImpl;

// AppList views use this delegate to talk to AppListController.
// TODO(hejq): Replace AppListViewDelegate calls with AppListClient calls.
class ASH_EXPORT AppListViewDelegateMash
    : public app_list::AppListViewDelegate {
 public:
  explicit AppListViewDelegateMash(ash::AppListControllerImpl* owner);

  // app_list::AppListViewDelegate
  ~AppListViewDelegateMash() override;
  app_list::AppListModel* GetModel() override;
  app_list::SearchModel* GetSearchModel() override;
  void StartSearch(const base::string16& raw_query) override;
  void OpenSearchResult(const std::string& result_id, int event_flags) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index,
                                int event_flags) override;
  void ViewShown(int64_t display_id) override;
  void Dismiss() override;
  void ViewClosing() override;
  void GetWallpaperProminentColors(
      GetWallpaperProminentColorsCallback callback) override;
  void ActivateItem(const std::string& id, int event_flags) override;
  void GetContextMenuModel(const std::string& id,
                           GetContextMenuModelCallback callback) override;
  void ContextMenuItemSelected(const std::string& id,
                               int command_id,
                               int event_flags) override;

  void AddObserver(app_list::AppListViewDelegateObserver* observer) override;
  void RemoveObserver(app_list::AppListViewDelegateObserver* observer) override;

 private:
  ash::AppListControllerImpl* owner_;
  base::string16 last_raw_query_;
  base::ObserverList<app_list::AppListViewDelegateObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegateMash);
};

}  // namespace ash

#endif  // ASH_APP_LIST_APP_LIST_VIEW_DELEGATE_MASH_H_
