// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/app_list/app_list_view_delegate.h"

#include "ash/shell.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/ash/app_list/apps_model_builder.h"
#include "chrome/browser/ui/views/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/views/ash/app_list/search_builder.h"

AppListViewDelegate::AppListViewDelegate() {
}

AppListViewDelegate::~AppListViewDelegate() {
}

void AppListViewDelegate::SetModel(app_list::AppListModel* model) {
  if (model) {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    apps_builder_.reset(new AppsModelBuilder(profile, model->apps()));
    apps_builder_->Build();

    search_builder_.reset(new SearchBuilder(profile,
                                            model->search_box(),
                                            model->results()));
  } else {
    apps_builder_.reset();
    search_builder_.reset();
  }
}

void AppListViewDelegate::ActivateAppListItem(
    app_list::AppListItemModel* item,
    int event_flags) {
  static_cast<ChromeAppListItem*>(item)->Activate(event_flags);
}

void AppListViewDelegate::StartSearch() {
  if (search_builder_.get())
    search_builder_->StartSearch();
}

void AppListViewDelegate::StopSearch() {
  if (search_builder_.get())
    search_builder_->StopSearch();
}

void AppListViewDelegate::OpenSearchResult(
    const app_list::SearchResult& result,
    int event_flags) {
  if (search_builder_.get())
    search_builder_->OpenResult(result, event_flags);
}

void AppListViewDelegate::Close()  {
  DCHECK(ash::Shell::HasInstance());
  if (ash::Shell::GetInstance()->GetAppListTargetVisibility())
    ash::Shell::GetInstance()->ToggleAppList();
}
