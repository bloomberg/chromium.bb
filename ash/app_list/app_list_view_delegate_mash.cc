// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_view_delegate_mash.h"

#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"

namespace ash {

AppListViewDelegateMash::AppListViewDelegateMash(
    ash::AppListControllerImpl* owner)
    : owner_(owner) {}

AppListViewDelegateMash::~AppListViewDelegateMash() = default;

app_list::AppListModel* AppListViewDelegateMash::GetModel() {
  return owner_->model();
}

app_list::SearchModel* AppListViewDelegateMash::GetSearchModel() {
  return owner_->search_model();
}

void AppListViewDelegateMash::StartSearch(const base::string16& raw_query) {
  owner_->StartSearch(raw_query);
}

void AppListViewDelegateMash::OpenSearchResult(const std::string& result_id,
                                               int event_flags) {
  owner_->OpenSearchResult(result_id, event_flags);
}

void AppListViewDelegateMash::InvokeSearchResultAction(
    const std::string& result_id,
    int action_index,
    int event_flags) {
  owner_->InvokeSearchResultAction(result_id, action_index, event_flags);
}

void AppListViewDelegateMash::ViewShown(int64_t display_id) {
  owner_->ViewShown(display_id);
}

void AppListViewDelegateMash::Dismiss() {
  owner_->presenter()->Dismiss(base::TimeTicks());
}

void AppListViewDelegateMash::ViewClosing() {
  owner_->ViewClosing();
}

void AppListViewDelegateMash::GetWallpaperProminentColors(
    GetWallpaperProminentColorsCallback callback) {
  Shell::Get()->wallpaper_controller()->GetWallpaperColors(std::move(callback));
}

void AppListViewDelegateMash::ActivateItem(const std::string& id,
                                           int event_flags) {
  owner_->ActivateItem(id, event_flags);
}

void AppListViewDelegateMash::GetContextMenuModel(
    const std::string& id,
    GetContextMenuModelCallback callback) {
  owner_->GetContextMenuModel(id, std::move(callback));
}

void AppListViewDelegateMash::ContextMenuItemSelected(const std::string& id,
                                                      int command_id,
                                                      int event_flags) {
  owner_->ContextMenuItemSelected(id, command_id, event_flags);
}

void AppListViewDelegateMash::AddObserver(
    app_list::AppListViewDelegateObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListViewDelegateMash::RemoveObserver(
    app_list::AppListViewDelegateObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
