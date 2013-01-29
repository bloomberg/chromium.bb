// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_view_delegate.h"

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/apps_model_builder.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/search_builder.h"
#include "content/public/browser/user_metrics.h"

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/app_list/app_sync_ui_state_watcher.h"
#endif

AppListViewDelegate::AppListViewDelegate(AppListControllerDelegate* controller)
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
#if defined(USE_ASH)
    app_sync_ui_state_watcher_.reset(new AppSyncUIStateWatcher(profile, model));
#endif
  } else {
    apps_builder_.reset();
    search_builder_.reset();
#if defined(USE_ASH)
    app_sync_ui_state_watcher_.reset();
#endif
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

void AppListViewDelegate::Dismiss()  {
  controller_->DismissView();
}

void AppListViewDelegate::ViewClosing() {
  controller_->ViewClosing();
}

void AppListViewDelegate::ViewActivationChanged(bool active) {
  controller_->ViewActivationChanged(active);
}
