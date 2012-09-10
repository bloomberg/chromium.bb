// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_view_delegate.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/apps_model_builder.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/search_builder.h"
#include "content/public/browser/user_metrics.h"

AppListViewDelegate::AppListViewDelegate(AppListController* controller)
    : controller_(controller) {}

AppListViewDelegate::~AppListViewDelegate() {}

void AppListViewDelegate::SetModel(app_list::AppListModel* model) {
  if (model) {
    Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
    apps_builder_.reset(new AppsModelBuilder(profile,
                                             model->apps(),
                                             controller_.get()));
    apps_builder_->Build();

    search_builder_.reset(new SearchBuilder(profile,
                                            model->search_box(),
                                            model->results(),
                                            controller_.get()));
  } else {
    apps_builder_.reset();
    search_builder_.reset();
  }
}

void AppListViewDelegate::ActivateAppListItem(
    app_list::AppListItemModel* item,
    int event_flags) {
  content::RecordAction(content::UserMetricsAction("AppList_ClickOnApp"));
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

void AppListViewDelegate::InvokeSearchResultAction(
    const app_list::SearchResult& result,
    int action_index,
    int event_flags) {
  if (search_builder_.get())
    search_builder_->InvokeResultAction(result, action_index, event_flags);
}

void AppListViewDelegate::Close()  {
  controller_->CloseView();
}

gfx::ImageSkia AppListViewDelegate::GetWindowAppIcon() {
  return controller_->GetWindowAppIcon();
}
