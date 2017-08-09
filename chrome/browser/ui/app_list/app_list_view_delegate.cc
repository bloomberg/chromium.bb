// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_view_delegate.h"

#include <stddef.h>

#include <vector>

#include "ash/public/interfaces/constants.mojom.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/profiler/scoped_tracker.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/custom_launcher_page_contents.h"
#include "chrome/browser/ui/app_list/launcher_page_event_dispatcher.h"
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
#include "extensions/common/manifest_handlers/launcher_page_info.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_controller.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/webview/webview.h"

namespace {

const int kAutoLaunchDefaultTimeoutMilliSec = 50;

// Gets a list of URLs of the custom launcher pages to show in the launcher.
// Returns a URL for each installed launcher page. If --custom-launcher-page is
// specified and valid, also includes that URL.
void GetCustomLauncherPageUrls(content::BrowserContext* browser_context,
                               std::vector<GURL>* urls) {
  // First, check the command line.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(app_list::switches::kCustomLauncherPage)) {
    GURL custom_launcher_page_url(command_line->GetSwitchValueASCII(
        app_list::switches::kCustomLauncherPage));

    if (custom_launcher_page_url.SchemeIs(extensions::kExtensionScheme)) {
      urls->push_back(custom_launcher_page_url);
    } else {
      LOG(ERROR) << "Invalid custom launcher page URL: "
                 << custom_launcher_page_url.possibly_invalid_spec();
    }
  }

  // Prevent launcher pages from loading unless the pref is enabled.
  // (Command-line specified pages are exempt from this rule).
  PrefService* profile_prefs = user_prefs::UserPrefs::Get(browser_context);
  if (profile_prefs &&
      profile_prefs->HasPrefPath(prefs::kGoogleNowLauncherEnabled) &&
      !profile_prefs->GetBoolean(prefs::kGoogleNowLauncherEnabled)) {
    return;
  }

  // Search the list of installed extensions for ones with 'launcher_page'.
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(browser_context);
  const extensions::ExtensionSet& enabled_extensions =
      extension_registry->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator it = enabled_extensions.begin();
       it != enabled_extensions.end();
       ++it) {
    const extensions::Extension* extension = it->get();
    extensions::LauncherPageInfo* info =
        extensions::LauncherPageHandler::GetInfo(extension);
    if (!info)
      continue;

    urls->push_back(extension->GetResourceURL(info->page));
  }
}

}  // namespace

AppListViewDelegate::AppListViewDelegate(AppListControllerDelegate* controller)
    : controller_(controller),
      profile_(nullptr),
      model_(nullptr),
      template_url_service_observer_(this),
      observer_binding_(this),
      weak_ptr_factory_(this) {
  CHECK(controller_);
  speech_ui_.reset(new app_list::SpeechUIModel);

#if defined(GOOGLE_CHROME_BUILD)
  gfx::ImageSkia* image;
  {
    // TODO(tapted): Remove ScopedTracker below once crbug.com/431326 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION("431326 GetImageSkiaNamed()"));
    image = ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_APP_LIST_GOOGLE_LOGO_VOICE_SEARCH);
  }

  speech_ui_->set_logo(*image);
#endif

  registrar_.Add(this,
                 chrome::NOTIFICATION_APP_TERMINATING,
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
    // Note: |search_resource_manager_| has a reference to |speech_ui_| so must
    // be destroyed first.
    search_resource_manager_.reset();
    search_controller_.reset();
    launcher_page_event_dispatcher_.reset();
    custom_page_contents_.clear();
    app_list::StartPageService* start_page_service =
        app_list::StartPageService::Get(profile_);
    if (start_page_service)
      start_page_service->RemoveObserver(this);
    app_sync_ui_state_watcher_.reset();
    model_ = NULL;
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

  {
    // TODO(tapted): Remove ScopedTracker below once crbug.com/431326 is fixed.
    tracked_objects::ScopedTracker tracking_profile(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "431326 AppListViewDelegate TemplateURL etc."));
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile_);
    template_url_service_observer_.Add(template_url_service);

    model_ = app_list::AppListSyncableServiceFactory::GetForProfile(profile_)
                 ->GetModel();

    // After |model_| is initialized, make a GetWallpaperColors mojo call to set
    // wallpaper colors for |model_|.
    wallpaper_controller_ptr_->GetWallpaperColors(
        base::Bind(&AppListViewDelegate::OnGetWallpaperColorsCallback,
                   weak_ptr_factory_.GetWeakPtr()));

    app_sync_ui_state_watcher_.reset(
        new AppSyncUIStateWatcher(profile_, model_));

    SetUpSearchUI();
    SetUpCustomLauncherPages();
    OnTemplateURLServiceChanged();
  }

  // Clear search query.
  model_->search_box()->Update(base::string16(), false);
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

  speech_ui_->SetSpeechRecognitionState(start_page_service
                                            ? start_page_service->state()
                                            : app_list::SPEECH_RECOGNITION_OFF,
                                        false);

  search_resource_manager_.reset(new app_list::SearchResourceManager(
      profile_,
      model_->search_box(),
      speech_ui_.get()));

  search_controller_ = CreateSearchController(profile_, model_, controller_);
}

void AppListViewDelegate::SetUpCustomLauncherPages() {
  std::vector<GURL> custom_launcher_page_urls;
  GetCustomLauncherPageUrls(profile_, &custom_launcher_page_urls);
  if (custom_launcher_page_urls.empty())
    return;

  for (auto it = custom_launcher_page_urls.begin();
       it != custom_launcher_page_urls.end(); ++it) {
    std::string extension_id = it->host();
    auto page_contents = base::MakeUnique<app_list::CustomLauncherPageContents>(
        base::MakeUnique<ChromeAppDelegate>(false), extension_id);
    page_contents->Initialize(profile_, *it);
    custom_page_contents_.push_back(std::move(page_contents));
  }

  std::string first_launcher_page_app_id = custom_launcher_page_urls[0].host();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)
          ->GetExtensionById(first_launcher_page_app_id,
                             extensions::ExtensionRegistry::EVERYTHING);
  model_->set_custom_launcher_page_name(extension->name());
  // Only the first custom launcher page gets events dispatched to it.
  launcher_page_event_dispatcher_.reset(
      new app_list::LauncherPageEventDispatcher(profile_,
                                                first_launcher_page_app_id));
}

void AppListViewDelegate::OnWallpaperColorsChanged(
    const std::vector<SkColor>& prominent_colors) {
  if (wallpaper_prominent_colors_ == prominent_colors)
    return;

  wallpaper_prominent_colors_ = prominent_colors;
  for (auto& observer : observers_)
    observer.OnWallpaperColorsChanged();
}

void AppListViewDelegate::OnHotwordStateChanged(bool started) {
  if (started) {
    if (speech_ui_->state() == app_list::SPEECH_RECOGNITION_READY) {
      OnSpeechRecognitionStateChanged(
          app_list::SPEECH_RECOGNITION_HOTWORD_LISTENING);
    }
  } else {
    if (speech_ui_->state() == app_list::SPEECH_RECOGNITION_HOTWORD_LISTENING)
      OnSpeechRecognitionStateChanged(app_list::SPEECH_RECOGNITION_READY);
  }
}

void AppListViewDelegate::OnHotwordRecognized(
    const scoped_refptr<content::SpeechRecognitionSessionPreamble>& preamble) {
  DCHECK_EQ(app_list::SPEECH_RECOGNITION_HOTWORD_LISTENING,
            speech_ui_->state());
  StartSpeechRecognitionForHotword(preamble);
}

app_list::AppListModel* AppListViewDelegate::GetModel() {
  return model_;
}

app_list::SpeechUIModel* AppListViewDelegate::GetSpeechUI() {
  return speech_ui_.get();
}

void AppListViewDelegate::StartSearch() {
  if (search_controller_) {
    search_controller_->Start();
    controller_->OnSearchStarted();
  }
}

void AppListViewDelegate::OpenSearchResult(
    app_list::SearchResult* result,
    bool auto_launch,
    int event_flags) {
  if (auto_launch)
    base::RecordAction(base::UserMetricsAction("AppList_AutoLaunched"));
  search_controller_->OpenResult(result, event_flags);
}

void AppListViewDelegate::InvokeSearchResultAction(
    app_list::SearchResult* result,
    int action_index,
    int event_flags) {
  search_controller_->InvokeResultAction(result, action_index, event_flags);
}

base::TimeDelta AppListViewDelegate::GetAutoLaunchTimeout() {
  return auto_launch_timeout_;
}

void AppListViewDelegate::AutoLaunchCanceled() {
  if (model_ && model_->search_box()->is_voice_query()) {
    base::RecordAction(base::UserMetricsAction("AppList_AutoLaunchCanceled"));
  }
  auto_launch_timeout_ = base::TimeDelta();
}

void AppListViewDelegate::ViewInitialized() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service) {
    service->AppListShown();
    if (service->HotwordEnabled()) {
      HotwordService* hotword_service =
          HotwordServiceFactory::GetForProfile(profile_);
      if (hotword_service)
        hotword_service->RequestHotwordSession(this);
    }
    OnHotwordStateChanged(service->HotwordEnabled());
  }
}

void AppListViewDelegate::Dismiss()  {
  controller_->DismissView();
}

void AppListViewDelegate::ViewClosing() {
  controller_->ViewClosing();

  if (!profile_)
    return;

  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service) {
    service->AppListHidden();
    if (service->HotwordEnabled()) {
      HotwordService* hotword_service =
          HotwordServiceFactory::GetForProfile(profile_);
      if (hotword_service) {
        hotword_service->StopHotwordSession(this);

        // If we're in always-on mode, we always want to restart hotwording
        // after closing the launcher window. So, in always-on mode, hotwording
        // is stopped, and then started again right away. Note that hotwording
        // may already be stopped. The call to StopHotwordSession() above both
        // explicitly stops hotwording, if it's running, and clears the
        // association between the hotword service and |this|.  When starting up
        // hotwording, pass nullptr as the client so that hotword triggers cause
        // the launcher to open.
        // TODO(amistry): This only works on ChromeOS since Chrome hides the
        // launcher instead of destroying it. Make this work on Chrome.
        if (hotword_service->IsAlwaysOnEnabled())
          hotword_service->RequestHotwordSession(nullptr);
      }
    }
  }
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
    if (service->state() == app_list::SPEECH_RECOGNITION_NETWORK_ERROR) {
      speech_ui_->SetSpeechRecognitionState(
          app_list::SPEECH_RECOGNITION_NETWORK_ERROR, true);
      return;
    }
    service->StartSpeechRecognition(preamble);
  }

  // With the new hotword extension, stop the hotword session. With the launcher
  // and NTP, this is unnecessary since the hotwording is implicitly stopped.
  // However, for always on, hotword triggering launches the launcher which
  // starts a session and hence starts the hotword detector. This results in the
  // hotword detector and the speech-to-text engine running in parallel, which
  // will conflict with each other (i.e. saying 'Ok Google' twice in a row
  // should cause a search to happen for 'Ok Google', not two hotword triggers).
  // To get around this, always stop the session when switching to speech
  // recognition.
  if (service && service->HotwordEnabled()) {
    HotwordService* hotword_service =
        HotwordServiceFactory::GetForProfile(profile_);
    if (hotword_service)
      hotword_service->StopHotwordSession(this);
  }
}

void AppListViewDelegate::OnSpeechResult(const base::string16& result,
                                         bool is_final) {
  speech_ui_->SetSpeechResult(result, is_final);
  if (is_final) {
    auto_launch_timeout_ = base::TimeDelta::FromMilliseconds(
        kAutoLaunchDefaultTimeoutMilliSec);
    model_->search_box()->Update(result, true);
  }
}

void AppListViewDelegate::OnSpeechSoundLevelChanged(int16_t level) {
  speech_ui_->UpdateSoundLevel(level);
}

void AppListViewDelegate::OnSpeechRecognitionStateChanged(
    app_list::SpeechRecognitionState new_state) {
  speech_ui_->SetSpeechRecognitionState(new_state, false);

  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  // With the new hotword extension, we need to re-request hotwording after
  // speech recognition has stopped. Do not request hotwording after the app
  // list has already closed.
  if (new_state == app_list::SPEECH_RECOGNITION_READY &&
      service && service->HotwordEnabled() &&
      controller_->GetAppListWindow()) {
    HotwordService* hotword_service =
        HotwordServiceFactory::GetForProfile(profile_);
    if (hotword_service) {
      hotword_service->RequestHotwordSession(this);
    }
  }
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
  views::WebView* web_view = new views::WebView(
      web_contents->GetBrowserContext());
  web_view->SetPreferredSize(size);
  web_view->SetResizeBackgroundColor(SK_ColorTRANSPARENT);
  web_view->SetWebContents(web_contents);
  return web_view;
}

std::vector<views::View*> AppListViewDelegate::CreateCustomPageWebViews(
    const gfx::Size& size) {
  std::vector<views::View*> web_views;

  for (const auto& contents : custom_page_contents_) {
    content::WebContents* web_contents = contents->web_contents();

    // The web contents should belong to the current profile.
    DCHECK_EQ(profile_, web_contents->GetBrowserContext());

    // Make the webview transparent.
    content::RenderWidgetHostView* render_view_host_view =
        web_contents->GetRenderViewHost()->GetWidget()->GetView();
    // The RenderWidgetHostView may be null if the renderer has crashed.
    if (render_view_host_view)
      render_view_host_view->SetBackgroundColor(SK_ColorTRANSPARENT);

    views::WebView* web_view =
        new views::WebView(web_contents->GetBrowserContext());
    web_view->SetPreferredSize(size);
    web_view->SetResizeBackgroundColor(SK_ColorTRANSPARENT);
    web_view->SetWebContents(web_contents);
    web_views.push_back(web_view);
  }

  return web_views;
}

void AppListViewDelegate::CustomLauncherPageAnimationChanged(double progress) {
  if (launcher_page_event_dispatcher_)
    launcher_page_event_dispatcher_->ProgressChanged(progress);
}

void AppListViewDelegate::CustomLauncherPagePopSubpage() {
  if (launcher_page_event_dispatcher_)
    launcher_page_event_dispatcher_->PopSubpage();
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

  model_->SetSearchEngineIsGoogle(is_google);

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
