// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_view_delegate.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_view_state.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/public/cpp/menu_utils.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/search_controller_factory.h"
#include "chrome/browser/ui/app_list/search/search_resource_manager.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/ash/app_list/app_sync_ui_state_watcher.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/speech_recognition_session_preamble.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_constants.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/webview/webview.h"

AppListViewDelegate::AppListViewDelegate(AppListControllerDelegate* controller)
    : controller_(controller),
      profile_(nullptr),
      model_(nullptr),
      search_model_(nullptr),
      model_updater_(nullptr),
      template_url_service_observer_(this),
      observer_binding_(this),
      weak_ptr_factory_(this) {
  CHECK(controller_);

  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &wallpaper_controller_ptr_);
  ash::mojom::WallpaperObserverAssociatedPtrInfo ptr_info;
  observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
  wallpaper_controller_ptr_->AddObserver(std::move(ptr_info));
}

AppListViewDelegate::~AppListViewDelegate() {
  // The destructor might not be called since the delegate is owned by a leaky
  // singleton. This matches the shutdown work done in Observe() in response to
  // chrome::NOTIFICATION_APP_TERMINATING, which may happen before this.
  SetProfile(nullptr);
}

void AppListViewDelegate::SetProfile(Profile* new_profile) {
  if (profile_ == new_profile)
    return;

  if (profile_) {
    DCHECK(model_);
    DCHECK(search_model_);
    DCHECK(model_updater_);
    // |search_controller_| will be destroyed on profile switch. Before that,
    // delete |model_|'s search results to clear any dangling pointers.
    search_model_->results()->DeleteAll();

    search_resource_manager_.reset();
    search_controller_.reset();
    app_sync_ui_state_watcher_.reset();
    model_ = nullptr;
    search_model_ = nullptr;
    model_updater_ = nullptr;
  }

  template_url_service_observer_.RemoveAll();

  profile_ = new_profile;
  if (!profile_)
    return;

  // If we are in guest mode, the new profile should be an incognito profile.
  // Otherwise, this may later hit a check (same condition as this one) in
  // Browser::Browser when opening links in a browser window (see
  // http://crbug.com/460437).
  DCHECK(!profile_->IsGuestSession() || profile_->IsOffTheRecord())
      << "Guest mode must use incognito profile";

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  template_url_service_observer_.Add(template_url_service);

  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
  model_ = syncable_service->GetModel();
  search_model_ = syncable_service->GetSearchModel();
  model_updater_ = syncable_service->GetModelUpdater();

  // After |model_| is initialized, make a GetWallpaperColors mojo call to set
  // wallpaper colors for |model_|.
  wallpaper_controller_ptr_->GetWallpaperColors(
      base::Bind(&AppListViewDelegate::OnGetWallpaperColorsCallback,
                 weak_ptr_factory_.GetWeakPtr()));

  app_sync_ui_state_watcher_ =
      std::make_unique<AppSyncUIStateWatcher>(profile_, model_updater_);

  SetUpSearchUI();
  OnTemplateURLServiceChanged();

  // Clear search query.
  model_updater_->UpdateSearchBox(base::string16(),
                                  false /* initiated_by_user */);
}

void AppListViewDelegate::OnGetWallpaperColorsCallback(
    const std::vector<SkColor>& colors) {
  OnWallpaperColorsChanged(colors);
}

void AppListViewDelegate::SetUpSearchUI() {
  search_resource_manager_.reset(
      new app_list::SearchResourceManager(profile_, model_updater_));

  search_controller_ =
      app_list::CreateSearchController(profile_, model_updater_, controller_);
}

void AppListViewDelegate::OnWallpaperColorsChanged(
    const std::vector<SkColor>& prominent_colors) {
  if (wallpaper_prominent_colors_ == prominent_colors)
    return;

  wallpaper_prominent_colors_ = prominent_colors;
  for (auto& observer : observers_)
    observer.OnWallpaperColorsChanged();
}

AppListModelUpdater* AppListViewDelegate::GetModelUpdater() {
  return model_updater_;
}

app_list::AppListModel* AppListViewDelegate::GetModel() {
  return model_;
}

app_list::SearchModel* AppListViewDelegate::GetSearchModel() {
  return search_model_;
}

void AppListViewDelegate::StartSearch(const base::string16& raw_query) {
  if (search_controller_) {
    search_controller_->Start(raw_query);
    controller_->OnSearchStarted();
  }
}

void AppListViewDelegate::OpenSearchResult(app_list::SearchResult* result,
                                           int event_flags) {
  search_controller_->OpenResult(result, event_flags);
}

void AppListViewDelegate::InvokeSearchResultAction(
    app_list::SearchResult* result,
    int action_index,
    int event_flags) {
  search_controller_->InvokeResultAction(result, action_index, event_flags);
}

void AppListViewDelegate::ViewShown() {
  base::RecordAction(base::UserMetricsAction("Launcher_Show"));
  base::UmaHistogramSparse("Apps.AppListBadgedAppsCount",
                           model_updater_->BadgedItemCount());
}

void AppListViewDelegate::Dismiss() {
  controller_->DismissView();
}

void AppListViewDelegate::ViewClosing() {
  controller_->ViewClosing();
}

void AppListViewDelegate::GetWallpaperProminentColors(
    GetWallpaperProminentColorsCallback callback) {
  std::move(callback).Run(wallpaper_prominent_colors_);
}

void AppListViewDelegate::ActivateItem(const std::string& id, int event_flags) {
  model_updater_->ActivateChromeItem(id, event_flags);
}

void AppListViewDelegate::GetContextMenuModel(
    const std::string& id,
    GetContextMenuModelCallback callback) {
  ui::MenuModel* menu = model_updater_->GetContextMenuModel(id);
  std::move(callback).Run(ash::menu_utils::GetMojoMenuItemsFromModel(menu));
}

void AppListViewDelegate::ContextMenuItemSelected(const std::string& id,
                                                  int command_id,
                                                  int event_flags) {
  model_updater_->ContextMenuItemSelected(id, command_id, event_flags);
}

void AppListViewDelegate::AddObserver(
    app_list::AppListViewDelegateObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListViewDelegate::RemoveObserver(
    app_list::AppListViewDelegateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListViewDelegate::OnTemplateURLServiceChanged() {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  const bool is_google =
      default_provider->GetEngineType(
          template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;

  model_updater_->SetSearchEngineIsGoogle(is_google);
}

void AppListViewDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);

  SetProfile(nullptr);  // Ensures launcher page web contents are torn down.
}
