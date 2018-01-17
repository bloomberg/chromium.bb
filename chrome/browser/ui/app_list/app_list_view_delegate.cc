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
#include "ash/app_list/model/speech/speech_ui_model.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "base/command_line.h"
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
#include "chrome/browser/ui/app_list/start_page_service.h"
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

namespace {

// The UMA histogram that logs which state search results are opened from.
const char kAppListSearchResultOpenSourceHistogram[] =
    "Apps.AppListSearchResultOpenedSource";

// The different sources from which a search result is displayed. These values
// are written to logs.  New enum values can be added, but existing enums must
// never be renumbered or deleted and reused.
enum ApplistSearchResultOpenedSource {
  kHalfClamshell = 0,
  kFullscreenClamshell = 1,
  kFullscreenTablet = 2,
  kMaxApplistSearchResultOpenedSource = 3,
};

void RecordHistogram(bool is_tablet_mode, app_list::AppListViewState state) {
  ApplistSearchResultOpenedSource source;

  if (is_tablet_mode) {
    source = kFullscreenTablet;
  } else {
    source = state == app_list::AppListViewState::HALF ? kHalfClamshell
                                                       : kFullscreenClamshell;
  }
  UMA_HISTOGRAM_ENUMERATION(kAppListSearchResultOpenSourceHistogram, source,
                            kMaxApplistSearchResultOpenedSource);
}

}  // namespace

namespace app_list {

SpeechRecognitionState ToSpeechRecognitionState(SpeechRecognizerState state) {
  switch (state) {
    case SPEECH_RECOGNIZER_OFF:
      return SPEECH_RECOGNITION_OFF;
    case SPEECH_RECOGNIZER_READY:
      return SPEECH_RECOGNITION_READY;
    case SPEECH_RECOGNIZER_RECOGNIZING:
      return SPEECH_RECOGNITION_RECOGNIZING;
    case SPEECH_RECOGNIZER_IN_SPEECH:
      return SPEECH_RECOGNITION_IN_SPEECH;
    case SPEECH_RECOGNIZER_STOPPING:
      return SPEECH_RECOGNITION_STOPPING;
    case SPEECH_RECOGNIZER_NETWORK_ERROR:
      return SPEECH_RECOGNITION_NETWORK_ERROR;
  }
}

}  // namespace app_list

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
  speech_ui_.reset(new app_list::SpeechUIModel);

#if defined(GOOGLE_CHROME_BUILD)
  gfx::ImageSkia* image;
  image = ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_APP_LIST_GOOGLE_LOGO_VOICE_SEARCH);
  speech_ui_->set_logo(*image);
#endif

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

    // Note: |search_resource_manager_| has a reference to |speech_ui_| so must
    // be destroyed first.
    search_resource_manager_.reset();
    search_controller_.reset();
    app_list::StartPageService* start_page_service =
        app_list::StartPageService::Get(profile_);
    if (start_page_service)
      start_page_service->RemoveObserver(this);
    app_sync_ui_state_watcher_.reset();
    model_ = nullptr;
    search_model_ = nullptr;
    model_updater_ = nullptr;
  }

  template_url_service_observer_.RemoveAll();

  profile_ = new_profile;
  if (!profile_) {
    speech_ui_->SetSpeechRecognitionState(app_list::SPEECH_RECOGNITION_OFF,
                                          false);
    return;
  }

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
  app_list::StartPageService* start_page_service =
      app_list::StartPageService::Get(profile_);
  if (start_page_service)
    start_page_service->AddObserver(this);

  speech_ui_->SetSpeechRecognitionState(
      start_page_service
          ? app_list::ToSpeechRecognitionState(start_page_service->state())
          : app_list::SPEECH_RECOGNITION_OFF,
      false);

  search_resource_manager_.reset(new app_list::SearchResourceManager(
      profile_, model_updater_, speech_ui_.get()));

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

app_list::SpeechUIModel* AppListViewDelegate::GetSpeechUI() {
  return speech_ui_.get();
}

void AppListViewDelegate::StartSearch(const base::string16& raw_query) {
  if (search_controller_) {
    search_controller_->Start(raw_query);
    controller_->OnSearchStarted();
  }
}

void AppListViewDelegate::OpenSearchResult(app_list::SearchResult* result,
                                           int event_flags) {
  // Record the search metric if the SearchResult is not a suggested app.
  if (result->display_type() != app_list::SearchResult::DISPLAY_RECOMMENDATION)
    RecordHistogram(model_updater_->TabletMode(),
                    model_updater_->StateFullscreen());

  search_controller_->OpenResult(result, event_flags);
}

void AppListViewDelegate::InvokeSearchResultAction(
    app_list::SearchResult* result,
    int action_index,
    int event_flags) {
  search_controller_->InvokeResultAction(result, action_index, event_flags);
}

void AppListViewDelegate::ViewInitialized() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service) {
    service->AppListShown();
  }
}

void AppListViewDelegate::Dismiss() {
  controller_->DismissView();
}

void AppListViewDelegate::ViewClosing() {
  controller_->ViewClosing();

  if (!profile_)
    return;

  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service)
    service->AppListHidden();
}

void AppListViewDelegate::StartSpeechRecognition() {
  StartSpeechRecognitionForHotword(nullptr);
}

void AppListViewDelegate::StopSpeechRecognition() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service)
    service->StopSpeechRecognition();
}

void AppListViewDelegate::StartSpeechRecognitionForHotword(
    const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble) {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);

  // Don't start the recognizer or stop the hotword session if there is a
  // network error. Show the network error message instead.
  if (service) {
    if (service->state() == SPEECH_RECOGNIZER_NETWORK_ERROR) {
      speech_ui_->SetSpeechRecognitionState(
          app_list::SPEECH_RECOGNITION_NETWORK_ERROR, true);
      return;
    }
    service->StartSpeechRecognition(preamble);
  }
}

void AppListViewDelegate::OnSpeechSoundLevelChanged(int16_t level) {
  speech_ui_->UpdateSoundLevel(level);
}

void AppListViewDelegate::OnSpeechRecognitionStateChanged(
    SpeechRecognizerState new_state) {
  speech_ui_->SetSpeechRecognitionState(
      app_list::ToSpeechRecognitionState(new_state), false);
}

views::View* AppListViewDelegate::CreateStartPageWebView(
    const gfx::Size& size) {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (!service)
    return NULL;

  service->LoadContentsIfNeeded();

  content::WebContents* web_contents = service->GetStartPageContents();
  if (!web_contents)
    return NULL;

  DCHECK_EQ(profile_, web_contents->GetBrowserContext());
  views::WebView* web_view =
      new views::WebView(web_contents->GetBrowserContext());
  web_view->SetPreferredSize(size);
  web_view->SetResizeBackgroundColor(SK_ColorTRANSPARENT);
  web_view->SetWebContents(web_contents);
  return web_view;
}

bool AppListViewDelegate::IsSpeechRecognitionEnabled() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  return service && service->GetSpeechRecognitionContents();
}

void AppListViewDelegate::GetWallpaperProminentColors(
    std::vector<SkColor>* colors) {
  *colors = wallpaper_prominent_colors_;
}

void AppListViewDelegate::ActivateItem(const std::string& id, int event_flags) {
  model_updater_->ActivateChromeItem(id, event_flags);
}

ui::MenuModel* AppListViewDelegate::GetContextMenuModel(const std::string& id) {
  return model_updater_->GetContextMenuModel(id);
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

  app_list::StartPageService* start_page_service =
      app_list::StartPageService::Get(profile_);
  if (start_page_service)
    start_page_service->set_search_engine_is_google(is_google);
}

void AppListViewDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);

  SetProfile(nullptr);  // Ensures launcher page web contents are torn down.
}
