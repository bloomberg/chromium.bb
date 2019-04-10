// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_client_impl.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "ash/public/cpp/menu_utils.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/app_sync_ui_state_watcher.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/search_controller_factory.h"
#include "chrome/browser/ui/app_list/search/search_resource_manager.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/common/extension.h"
#include "services/content/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/base/models/menu_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/rect.h"

namespace {

AppListClientImpl* g_app_list_client_instance = nullptr;

bool IsTabletMode() {
  return TabletModeClient::Get() &&
         TabletModeClient::Get()->tablet_mode_enabled();
}

}  // namespace

AppListClientImpl::MojoRecorderForTest::MojoRecorderForTest() = default;
AppListClientImpl::MojoRecorderForTest::~MojoRecorderForTest() = default;

int AppListClientImpl::MojoRecorderForTest::Query(int profile_id) const {
  auto iter = recorder_.find(profile_id);
  return iter == recorder_.end() ? 0 : iter->second;
}
///////////////////////////////////////////////////////////////////////////////

AppListClientImpl::AppListClientImpl()
    : template_url_service_observer_(this),
      binding_(this),
      weak_ptr_factory_(this) {
  // Bind this to the AppListController in Ash.
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &app_list_controller_);
  ash::mojom::AppListClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  app_list_controller_->SetClient(std::move(client));
  user_manager::UserManager::Get()->AddSessionStateObserver(this);

  DCHECK(!g_app_list_client_instance);
  g_app_list_client_instance = this;
}

AppListClientImpl::~AppListClientImpl() {
  app_list_controller_.reset();
  SetProfile(nullptr);

  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);

  DCHECK_EQ(this, g_app_list_client_instance);
  g_app_list_client_instance = nullptr;
}

// static
AppListClientImpl* AppListClientImpl::GetInstance() {
  return g_app_list_client_instance;
}

void AppListClientImpl::StartSearch(const base::string16& trimmed_query) {
  if (search_controller_) {
    search_controller_->Start(trimmed_query);
    OnSearchStarted();
  }
}

void AppListClientImpl::OpenSearchResult(
    const std::string& result_id,
    int event_flags,
    ash::mojom::AppListLaunchedFrom launched_from,
    ash::mojom::AppListLaunchType launch_type,
    int suggestion_index) {
  if (!search_controller_)
    return;

  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (!result)
    return;

  search_controller_->OpenResult(result, event_flags);

  // Send training signal to search controller.
  search_controller_->Train(result_id,
                            app_list::RankingItemTypeFromSearchResult(*result));

  if (launch_type == ash::mojom::AppListLaunchType::kAppSearchResult) {
    // Log the AppResult (either in the search result page, or in chip form in
    // AppsGridView) to the UKM system.
    app_launch_event_logger_.OnSuggestionChipOrSearchBoxClicked(
        result_id, suggestion_index, static_cast<int>(launched_from));
  }

  if (launched_from ==
      ash::mojom::AppListLaunchedFrom::kLaunchedFromSearchBox) {
    RecordSearchResultOpenTypeHistogram(result->GetSearchResultType());
  }
}

void AppListClientImpl::InvokeSearchResultAction(const std::string& result_id,
                                                 int action_index,
                                                 int event_flags) {
  if (!search_controller_)
    return;
  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (result)
    search_controller_->InvokeResultAction(result, action_index, event_flags);
}

void AppListClientImpl::GetSearchResultContextMenuModel(
    const std::string& result_id,
    GetContextMenuModelCallback callback) {
  if (!search_controller_) {
    std::move(callback).Run(std::vector<ash::mojom::MenuItemPtr>());
    return;
  }
  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (!result) {
    std::move(callback).Run(std::vector<ash::mojom::MenuItemPtr>());
    return;
  }
  result->GetContextMenuModel(base::BindOnce(
      [](GetContextMenuModelCallback callback,
         std::unique_ptr<ui::MenuModel> menu_model) {
        std::move(callback).Run(
            ash::menu_utils::GetMojoMenuItemsFromModel(menu_model.get()));
      },
      std::move(callback)));
}

void AppListClientImpl::SearchResultContextMenuItemSelected(
    const std::string& result_id,
    int command_id,
    int event_flags) {
  if (!search_controller_)
    return;
  ChromeSearchResult* result = search_controller_->FindSearchResult(result_id);
  if (!result)
    return;
  result->ContextMenuItemSelected(command_id, event_flags);
}

void AppListClientImpl::ViewClosing() {
  display_id_ = display::kInvalidDisplayId;
}

void AppListClientImpl::ViewShown(int64_t display_id) {
  if (current_model_updater_) {
    base::RecordAction(base::UserMetricsAction("Launcher_Show"));
    base::UmaHistogramSparse("Apps.AppListBadgedAppsCount",
                             current_model_updater_->BadgedItemCount());
  }
  display_id_ = display_id;
}

void AppListClientImpl::ActivateItem(int profile_id,
                                     const std::string& id,
                                     int event_flags) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];

  // Pointless to notify the AppListModelUpdater of the activated item if the
  // |requested_model_updater| is not the current one, which means that the
  // active profile is changed. The same rule applies to the GetContextMenuModel
  // and ContextMenuItemSelected.
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    return;
  }

  requested_model_updater->ActivateChromeItem(id, event_flags);

  // Send a training signal to the search controller.
  const auto* item = current_model_updater_->FindItem(id);
  if (item) {
    search_controller_->Train(
        id, app_list::RankingItemTypeFromChromeAppListItem(*item));
  }

  app_launch_event_logger_.OnGridClicked(id);
}

void AppListClientImpl::GetContextMenuModel(
    int profile_id,
    const std::string& id,
    GetContextMenuModelCallback callback) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    std::move(callback).Run(std::vector<ash::mojom::MenuItemPtr>());
    return;
  }
  requested_model_updater->GetContextMenuModel(
      id,
      base::BindOnce(
          [](GetContextMenuModelCallback callback,
             std::unique_ptr<ui::MenuModel> menu_model) {
            std::move(callback).Run(
                ash::menu_utils::GetMojoMenuItemsFromModel(menu_model.get()));
          },
          std::move(callback)));
}

void AppListClientImpl::ContextMenuItemSelected(int profile_id,
                                                const std::string& id,
                                                int command_id,
                                                int event_flags) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (requested_model_updater != current_model_updater_ ||
      !requested_model_updater) {
    return;
  }

  requested_model_updater->ContextMenuItemSelected(id, command_id, event_flags);
}

void AppListClientImpl::OnAppListTargetVisibilityChanged(bool visible) {
  app_list_target_visibility_ = visible;
}

void AppListClientImpl::OnAppListVisibilityChanged(bool visible) {
  app_list_visible_ = visible;
}

void AppListClientImpl::OnFolderCreated(
    int profile_id,
    ash::mojom::AppListItemMetadataPtr item) {
  if (mojo_recorder_for_test_.get())
    mojo_recorder_for_test_->Record(profile_id);

  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (!requested_model_updater)
    return;
  DCHECK(item->is_folder);
  requested_model_updater->OnFolderCreated(std::move(item));
}

void AppListClientImpl::OnFolderDeleted(
    int profile_id,
    ash::mojom::AppListItemMetadataPtr item) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (!requested_model_updater)
    return;
  DCHECK(item->is_folder);
  requested_model_updater->OnFolderDeleted(std::move(item));
}

void AppListClientImpl::OnItemUpdated(int profile_id,
                                      ash::mojom::AppListItemMetadataPtr item) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (!requested_model_updater)
    return;
  requested_model_updater->OnItemUpdated(std::move(item));
}

void AppListClientImpl::OnPageBreakItemAdded(
    int profile_id,
    const std::string& id,
    const syncer::StringOrdinal& position) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (!requested_model_updater)
    return;
  requested_model_updater->OnPageBreakItemAdded(id, position);
}

void AppListClientImpl::OnPageBreakItemDeleted(int profile_id,
                                               const std::string& id) {
  auto* requested_model_updater = profile_model_mappings_[profile_id];
  if (!requested_model_updater)
    return;
  requested_model_updater->OnPageBreakItemDeleted(id);
}

void AppListClientImpl::GetNavigableContentsFactory(
    content::mojom::NavigableContentsFactoryRequest request) {
  if (profile_) {
    content::BrowserContext::GetConnectorFor(profile_)->BindInterface(
        content::mojom::kServiceName, std::move(request));
  }
}

void AppListClientImpl::OnSearchResultVisibilityChanged(const std::string& id,
                                                        bool visibility) {
  if (!search_controller_)
    return;

  ChromeSearchResult* result = search_controller_->FindSearchResult(id);
  if (result == nullptr) {
    return;
  }
  result->OnVisibilityChanged(visibility);
}

void AppListClientImpl::ActiveUserChanged(
    const user_manager::User* active_user) {
  if (!active_user->is_profile_created())
    return;
  UpdateProfile();
}

void AppListClientImpl::UpdateProfile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile);
  DCHECK(syncable_service);
  SetProfile(profile);
}

void AppListClientImpl::SetProfile(Profile* new_profile) {
  if (profile_ == new_profile)
    return;

  if (profile_) {
    DCHECK(current_model_updater_);
    current_model_updater_->SetActive(false);

    search_resource_manager_.reset();
    search_controller_.reset();
    app_sync_ui_state_watcher_.reset();
    current_model_updater_ = nullptr;
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

  template_url_service_observer_.Add(
      TemplateURLServiceFactory::GetForProfile(profile_));

  app_list::AppListSyncableService* syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);

  current_model_updater_ = syncable_service->GetModelUpdater();
  current_model_updater_->SetActive(true);

  // On ChromeOS, there is no way to sign-off just one user. When signing off
  // all users, AppListClientImpl instance is destructed before profiles are
  // unloaded. So we don't need to remove elements from
  // |profile_model_mappings_| explicitly.
  profile_model_mappings_[current_model_updater_->model_id()] =
      current_model_updater_;

  app_sync_ui_state_watcher_ =
      std::make_unique<AppSyncUIStateWatcher>(profile_, current_model_updater_);

  SetUpSearchUI();
  OnTemplateURLServiceChanged();

  // Clear search query.
  current_model_updater_->UpdateSearchBox(base::string16(),
                                          false /* initiated_by_user */);
}

void AppListClientImpl::SetUpSearchUI() {
  search_resource_manager_ = std::make_unique<app_list::SearchResourceManager>(
      profile_, current_model_updater_);

  search_controller_ =
      app_list::CreateSearchController(profile_, current_model_updater_, this);
}

app_list::SearchController* AppListClientImpl::GetSearchControllerForTest() {
  return search_controller_.get();
}

AppListModelUpdater* AppListClientImpl::GetModelUpdaterForTest() {
  return current_model_updater_;
}

void AppListClientImpl::SetUpMojoRecorderForTest() {
  mojo_recorder_for_test_ = std::make_unique<MojoRecorderForTest>();
}

int AppListClientImpl::QueryMojoRecorderForTest(int profile_id) {
  DCHECK(mojo_recorder_for_test_.get());
  return mojo_recorder_for_test_->Query(profile_id);
}

void AppListClientImpl::OnTemplateURLServiceChanged() {
  DCHECK(current_model_updater_);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  const bool is_google =
      default_provider &&
      default_provider->GetEngineType(
          template_url_service->search_terms_data()) == SEARCH_ENGINE_GOOGLE;

  current_model_updater_->SetSearchEngineIsGoogle(is_google);
}

void AppListClientImpl::ShowAndSwitchToState(ash::AppListState state) {
  if (!app_list_controller_)
    return;
  app_list_controller_->ShowAppListAndSwitchToState(state);
}

void AppListClientImpl::ShowAppList() {
  // This may not work correctly if the profile passed in is different from the
  // one the ash Shell is currently using.
  if (!app_list_controller_)
    return;
  app_list_controller_->ShowAppList();
}

Profile* AppListClientImpl::GetCurrentAppListProfile() const {
  return ChromeLauncherController::instance()->profile();
}

ash::mojom::AppListController* AppListClientImpl::GetAppListController() const {
  return app_list_controller_.get();
}

void AppListClientImpl::DismissView() {
  if (!app_list_controller_)
    return;
  app_list_controller_->DismissAppList();
}

int64_t AppListClientImpl::GetAppListDisplayId() {
  return display_id_;
}

void AppListClientImpl::GetAppInfoDialogBounds(
    GetAppInfoDialogBoundsCallback callback) {
  if (!app_list_controller_) {
    LOG(ERROR) << "app_list_controller_ is null";
    std::move(callback).Run(gfx::Rect());
    return;
  }
  app_list_controller_->GetAppInfoDialogBounds(std::move(callback));
}

bool AppListClientImpl::IsAppPinned(const std::string& app_id) {
  return ChromeLauncherController::instance()->IsAppPinned(app_id);
}

bool AppListClientImpl::IsAppOpen(const std::string& app_id) const {
  return ChromeLauncherController::instance()->IsOpen(ash::ShelfID(app_id));
}

void AppListClientImpl::PinApp(const std::string& app_id) {
  ChromeLauncherController::instance()->PinAppWithID(app_id);
}

void AppListClientImpl::UnpinApp(const std::string& app_id) {
  ChromeLauncherController::instance()->UnpinAppWithID(app_id);
}

AppListControllerDelegate::Pinnable AppListClientImpl::GetPinnable(
    const std::string& app_id) {
  return GetPinnableForAppID(app_id,
                             ChromeLauncherController::instance()->profile());
}

void AppListClientImpl::CreateNewWindow(Profile* profile, bool incognito) {
  if (incognito)
    chrome::NewEmptyWindow(profile->GetOffTheRecordProfile());
  else
    chrome::NewEmptyWindow(profile);
}

void AppListClientImpl::OpenURL(Profile* profile,
                                const GURL& url,
                                ui::PageTransition transition,
                                WindowOpenDisposition disposition) {
  NavigateParams params(profile, url, transition);
  params.disposition = disposition;
  Navigate(&params);
}

void AppListClientImpl::ActivateApp(Profile* profile,
                                    const extensions::Extension* extension,
                                    AppListSource source,
                                    int event_flags) {
  // Platform apps treat activations as a launch. The app can decide whether to
  // show a new window or focus an existing window as it sees fit.
  if (extension->is_platform_app()) {
    LaunchApp(profile, extension, source, event_flags, GetAppListDisplayId());
    return;
  }

  ChromeLauncherController::instance()->ActivateApp(
      extension->id(), AppListSourceToLaunchSource(source), event_flags,
      GetAppListDisplayId());

  if (!IsTabletMode())
    DismissView();
}

void AppListClientImpl::LaunchApp(Profile* profile,
                                  const extensions::Extension* extension,
                                  AppListSource source,
                                  int event_flags,
                                  int64_t display_id) {
  ChromeLauncherController::instance()->LaunchApp(
      ash::ShelfID(extension->id()), AppListSourceToLaunchSource(source),
      event_flags, display_id);

  if (!IsTabletMode())
    DismissView();
}

ash::ShelfLaunchSource AppListClientImpl::AppListSourceToLaunchSource(
    AppListSource source) {
  switch (source) {
    case LAUNCH_FROM_APP_LIST:
      return ash::LAUNCH_FROM_APP_LIST;
    case LAUNCH_FROM_APP_LIST_SEARCH:
      return ash::LAUNCH_FROM_APP_LIST_SEARCH;
    default:
      return ash::LAUNCH_FROM_UNKNOWN;
  }
}

void AppListClientImpl::FlushMojoForTesting() {
  app_list_controller_.FlushForTesting();
  binding_.FlushForTesting();
}
