// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_view_delegate.h"

#include <vector>

#include "apps/custom_launcher_page_contents.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/start_page_service.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "grit/theme_resources.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate_observer.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/webview/webview.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/controls/webview/webview.h"
#endif

#if defined(USE_AURA)
#include "ui/keyboard/keyboard_util.h"
#endif

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "chrome/browser/ui/ash/app_list/app_sync_ui_state_watcher.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/web_applications/web_app_win.h"
#endif


namespace chrome {
const char kAppLauncherCategoryTag[] = "AppLauncher";
}  // namespace chrome

namespace {

const int kAutoLaunchDefaultTimeoutMilliSec = 50;

#if defined(OS_WIN)
void CreateShortcutInWebAppDir(
    const base::FilePath& app_data_dir,
    base::Callback<void(const base::FilePath&)> callback,
    const web_app::ShortcutInfo& info) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(web_app::CreateShortcutInWebAppDir, app_data_dir, info),
      callback);
}
#endif

void PopulateUsers(const ProfileInfoCache& profile_info,
                   const base::FilePath& active_profile_path,
                   app_list::AppListViewDelegate::Users* users) {
  users->clear();
  const size_t count = profile_info.GetNumberOfProfiles();
  for (size_t i = 0; i < count; ++i) {
    app_list::AppListViewDelegate::User user;
    user.name = profile_info.GetNameOfProfileAtIndex(i);
    user.email = profile_info.GetUserNameOfProfileAtIndex(i);
    user.profile_path = profile_info.GetPathOfProfileAtIndex(i);
    user.active = active_profile_path == user.profile_path;
    users->push_back(user);
  }
}

// Gets a list of URLs of the custom launcher pages to show in the launcher.
// Returns a URL for each installed launcher page. If --custom-launcher-page is
// specified and valid, also includes that URL.
void GetCustomLauncherPageUrls(content::BrowserContext* browser_context,
                               std::vector<GURL>* urls) {
  // First, check the command line.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (app_list::switches::IsExperimentalAppListEnabled() &&
      command_line->HasSwitch(switches::kCustomLauncherPage)) {
    GURL custom_launcher_page_url(
        command_line->GetSwitchValueASCII(switches::kCustomLauncherPage));

    if (custom_launcher_page_url.SchemeIs(extensions::kExtensionScheme)) {
      urls->push_back(custom_launcher_page_url);
    } else {
      LOG(ERROR) << "Invalid custom launcher page URL: "
                 << custom_launcher_page_url.possibly_invalid_spec();
    }
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
    const extensions::Manifest* manifest = extension->manifest();
    if (!manifest->HasKey(extensions::manifest_keys::kLauncherPage))
      continue;
    std::string launcher_page_page;
    if (!manifest->GetString(extensions::manifest_keys::kLauncherPagePage,
                             &launcher_page_page)) {
      // TODO(mgiuca): Add a proper manifest parser to catch this error properly
      // and display it on the extensions page.
      LOG(ERROR) << "Extension " << extension->id() << ": "
                 << extensions::manifest_keys::kLauncherPage
                 << " has no 'page' attribute; will be ignored.";
      continue;
    }
    urls->push_back(extension->GetResourceURL(launcher_page_page));
  }
}

}  // namespace

AppListViewDelegate::AppListViewDelegate(Profile* profile,
                                         AppListControllerDelegate* controller)
    : controller_(controller),
      profile_(profile),
      model_(NULL),
      scoped_observer_(this) {
  CHECK(controller_);
  // The SigninManagerFactor and the SigninManagers are observed to keep the
  // profile switcher menu up to date, with the correct list of profiles and the
  // correct email address (or none for signed out users) for each.
  SigninManagerFactory::GetInstance()->AddObserver(this);

  // Start observing all already-created SigninManagers.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  std::vector<Profile*> profiles = profile_manager->GetLoadedProfiles();
  for (std::vector<Profile*>::iterator i = profiles.begin();
       i != profiles.end();
       ++i) {
    SigninManagerBase* manager =
        SigninManagerFactory::GetForProfileIfExists(*i);
    if (manager) {
      DCHECK(!scoped_observer_.IsObserving(manager));
      scoped_observer_.Add(manager);
    }
  }

  profile_manager->GetProfileInfoCache().AddObserver(this);

  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  speech_ui_.reset(new app_list::SpeechUIModel(
      service ? service->state() : app_list::SPEECH_RECOGNITION_OFF));

#if defined(GOOGLE_CHROME_BUILD)
  speech_ui_->set_logo(
      *ui::ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_APP_LIST_GOOGLE_LOGO_VOICE_SEARCH));
#endif

  OnProfileChanged();  // sets model_
  if (service)
    service->AddObserver(this);

  // Set up the custom launcher pages.
  std::vector<GURL> custom_launcher_page_urls;
  GetCustomLauncherPageUrls(profile, &custom_launcher_page_urls);
  for (std::vector<GURL>::const_iterator it = custom_launcher_page_urls.begin();
       it != custom_launcher_page_urls.end();
       ++it) {
    std::string extension_id = it->host();
    apps::CustomLauncherPageContents* page_contents =
        new apps::CustomLauncherPageContents(
            scoped_ptr<extensions::AppDelegate>(new ChromeAppDelegate),
            extension_id);
    page_contents->Initialize(profile, *it);
    custom_page_contents_.push_back(page_contents);
  }
}

AppListViewDelegate::~AppListViewDelegate() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service)
    service->RemoveObserver(this);
  g_browser_process->
      profile_manager()->GetProfileInfoCache().RemoveObserver(this);

  SigninManagerFactory* factory = SigninManagerFactory::GetInstance();
  if (factory)
    factory->RemoveObserver(this);

  // Ensure search controller is released prior to speech_ui_.
  search_controller_.reset();
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

void AppListViewDelegate::OnHotwordRecognized() {
  DCHECK_EQ(app_list::SPEECH_RECOGNITION_HOTWORD_LISTENING,
            speech_ui_->state());
  ToggleSpeechRecognition();
}

void AppListViewDelegate::SigninManagerCreated(SigninManagerBase* manager) {
  scoped_observer_.Add(manager);
}

void AppListViewDelegate::SigninManagerShutdown(SigninManagerBase* manager) {
  if (scoped_observer_.IsObserving(manager))
    scoped_observer_.Remove(manager);
}

void AppListViewDelegate::GoogleSigninFailed(
    const GoogleServiceAuthError& error) {
  OnProfileChanged();
}

void AppListViewDelegate::GoogleSigninSucceeded(const std::string& username,
                                                const std::string& password) {
  OnProfileChanged();
}

void AppListViewDelegate::GoogleSignedOut(const std::string& username) {
  OnProfileChanged();
}

void AppListViewDelegate::OnProfileChanged() {
  model_ = app_list::AppListSyncableServiceFactory::GetForProfile(
      profile_)->model();

  search_controller_.reset(new app_list::SearchController(
      profile_, model_->search_box(), model_->results(),
      speech_ui_.get(), controller_));

#if defined(USE_ASH)
  app_sync_ui_state_watcher_.reset(new AppSyncUIStateWatcher(profile_, model_));
#endif

  // Don't populate the app list users if we are on the ash desktop.
  chrome::HostDesktopType desktop = chrome::GetHostDesktopTypeForNativeWindow(
      controller_->GetAppListWindow());
  if (desktop == chrome::HOST_DESKTOP_TYPE_ASH)
    return;

  // Populate the app list users.
  PopulateUsers(g_browser_process->profile_manager()->GetProfileInfoCache(),
                profile_->GetPath(), &users_);

  FOR_EACH_OBSERVER(app_list::AppListViewDelegateObserver,
                    observers_,
                    OnProfilesChanged());
}

bool AppListViewDelegate::ForceNativeDesktop() const {
  return controller_->ForceNativeDesktop();
}

void AppListViewDelegate::SetProfileByPath(const base::FilePath& profile_path) {
  DCHECK(model_);

  // The profile must be loaded before this is called.
  profile_ =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  DCHECK(profile_);

  OnProfileChanged();

  // Clear search query.
  model_->search_box()->SetText(base::string16());
}

app_list::AppListModel* AppListViewDelegate::GetModel() {
  return model_;
}

app_list::SpeechUIModel* AppListViewDelegate::GetSpeechUI() {
  return speech_ui_.get();
}

void AppListViewDelegate::GetShortcutPathForApp(
    const std::string& app_id,
    const base::Callback<void(const base::FilePath&)>& callback) {
#if defined(OS_WIN)
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (!extension) {
    callback.Run(base::FilePath());
    return;
  }

  base::FilePath app_data_dir(
      web_app::GetWebAppDataDirectory(profile_->GetPath(),
                                      extension->id(),
                                      GURL()));

  web_app::GetShortcutInfoForApp(
      extension,
      profile_,
      base::Bind(CreateShortcutInWebAppDir, app_data_dir, callback));
#else
  callback.Run(base::FilePath());
#endif
}

void AppListViewDelegate::StartSearch() {
  if (search_controller_)
    search_controller_->Start();
}

void AppListViewDelegate::StopSearch() {
  if (search_controller_)
    search_controller_->Stop();
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
  base::RecordAction(base::UserMetricsAction("AppList_AutoLaunchCanceled"));
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
  }
}

void AppListViewDelegate::Dismiss()  {
  controller_->DismissView();
}

void AppListViewDelegate::ViewClosing() {
  controller_->ViewClosing();

  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service) {
    service->AppListHidden();
    if (service->HotwordEnabled()) {
      HotwordService* hotword_service =
          HotwordServiceFactory::GetForProfile(profile_);
      if (hotword_service)
        hotword_service->StopHotwordSession(this);
    }
  }
}

gfx::ImageSkia AppListViewDelegate::GetWindowIcon() {
  return controller_->GetWindowIcon();
}

void AppListViewDelegate::OpenSettings() {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          extension_misc::kSettingsAppId,
          extensions::ExtensionRegistry::EVERYTHING);
  DCHECK(extension);
  controller_->ActivateApp(profile_,
                           extension,
                           AppListControllerDelegate::LAUNCH_FROM_UNKNOWN,
                           0);
}

void AppListViewDelegate::OpenHelp() {
  chrome::HostDesktopType desktop = chrome::GetHostDesktopTypeForNativeWindow(
      controller_->GetAppListWindow());
  chrome::ScopedTabbedBrowserDisplayer displayer(profile_, desktop);
  content::OpenURLParams params(GURL(chrome::kAppLauncherHelpURL),
                                content::Referrer(),
                                NEW_FOREGROUND_TAB,
                                content::PAGE_TRANSITION_LINK,
                                false);
  displayer.browser()->OpenURL(params);
}

void AppListViewDelegate::OpenFeedback() {
  chrome::HostDesktopType desktop = chrome::GetHostDesktopTypeForNativeWindow(
      controller_->GetAppListWindow());
  Browser* browser = chrome::FindTabbedBrowser(profile_, false, desktop);
  chrome::ShowFeedbackPage(browser, std::string(),
                           chrome::kAppLauncherCategoryTag);
}

void AppListViewDelegate::ToggleSpeechRecognition() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (service)
    service->ToggleSpeechRecognition();
}

void AppListViewDelegate::ShowForProfileByPath(
    const base::FilePath& profile_path) {
  controller_->ShowForProfileByPath(profile_path);
}

void AppListViewDelegate::OnSpeechResult(const base::string16& result,
                                         bool is_final) {
  speech_ui_->SetSpeechResult(result, is_final);
  if (is_final) {
    auto_launch_timeout_ = base::TimeDelta::FromMilliseconds(
        kAutoLaunchDefaultTimeoutMilliSec);
    model_->search_box()->SetText(result);
  }
}

void AppListViewDelegate::OnSpeechSoundLevelChanged(int16 level) {
  speech_ui_->UpdateSoundLevel(level);
}

void AppListViewDelegate::OnSpeechRecognitionStateChanged(
    app_list::SpeechRecognitionState new_state) {
  speech_ui_->SetSpeechRecognitionState(new_state);

  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  // With the new hotword extension, we need to re-request hotwording after
  // speech recognition has stopped.
  if (new_state == app_list::SPEECH_RECOGNITION_READY &&
      HotwordService::IsExperimentalHotwordingEnabled() &&
      service && service->HotwordEnabled()) {
    HotwordService* hotword_service =
        HotwordServiceFactory::GetForProfile(profile_);
    if (hotword_service) {
      hotword_service->RequestHotwordSession(this);
    }
  }
}

void AppListViewDelegate::OnProfileAdded(const base::FilePath& profile_path) {
  OnProfileChanged();
}

void AppListViewDelegate::OnProfileWasRemoved(
    const base::FilePath& profile_path, const base::string16& profile_name) {
  OnProfileChanged();
}

void AppListViewDelegate::OnProfileNameChanged(
    const base::FilePath& profile_path,
    const base::string16& old_profile_name) {
  OnProfileChanged();
}

#if defined(TOOLKIT_VIEWS)
views::View* AppListViewDelegate::CreateStartPageWebView(
    const gfx::Size& size) {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  if (!service)
    return NULL;

  content::WebContents* web_contents = service->GetStartPageContents();
  if (!web_contents)
    return NULL;

  DCHECK_EQ(profile_, web_contents->GetBrowserContext());
  views::WebView* web_view = new views::WebView(
      web_contents->GetBrowserContext());
  web_view->SetPreferredSize(size);
  web_view->SetWebContents(web_contents);
  return web_view;
}

std::vector<views::View*> AppListViewDelegate::CreateCustomPageWebViews(
    const gfx::Size& size) {
  std::vector<views::View*> web_views;

  for (ScopedVector<apps::CustomLauncherPageContents>::const_iterator it =
           custom_page_contents_.begin();
       it != custom_page_contents_.end();
       ++it) {
    content::WebContents* web_contents = (*it)->web_contents();
    // TODO(mgiuca): DCHECK_EQ(profile_, web_contents->GetBrowserContext())
    // after http://crbug.com/392763 resolved.
    views::WebView* web_view =
        new views::WebView(web_contents->GetBrowserContext());
    web_view->SetPreferredSize(size);
    web_view->SetWebContents(web_contents);
    web_views.push_back(web_view);
  }

  return web_views;
}
#endif

bool AppListViewDelegate::IsSpeechRecognitionEnabled() {
  app_list::StartPageService* service =
      app_list::StartPageService::Get(profile_);
  return service && service->GetSpeechRecognitionContents();
}

const app_list::AppListViewDelegate::Users&
AppListViewDelegate::GetUsers() const {
  return users_;
}

bool AppListViewDelegate::ShouldCenterWindow() const {
  if (app_list::switches::IsCenteredAppListEnabled())
    return true;

  // keyboard depends upon Aura.
#if defined(USE_AURA)
  // If the virtual keyboard is enabled, use the new app list position. The old
  // position is too tall, and doesn't fit in the left-over screen space.
  if (keyboard::IsKeyboardEnabled())
    return true;
#endif

#if defined(USE_ASH)
  // If it is at all possible to enter maximize mode in this configuration
  // (which has a virtual keyboard), we should use the experimental position.
  // This avoids having the app list change shape and position as the user
  // enters and exits maximize mode.
  if (ash::Shell::HasInstance() &&
      ash::Shell::GetInstance()
          ->maximize_mode_controller()
          ->CanEnterMaximizeMode()) {
    return true;
  }
#endif

  return false;
}

void AppListViewDelegate::AddObserver(
    app_list::AppListViewDelegateObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListViewDelegate::RemoveObserver(
    app_list::AppListViewDelegateObserver* observer) {
  observers_.RemoveObserver(observer);
}
