// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/process/process_info.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/banners/app_banner_manager_desktop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_permission_request.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/memory/tab_manager_web_contents_data.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/pepper_broker_infobar_delegate.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/task_management/web_contents_tags.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/blocked_content/popup_blocker_tab_helper.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_bubble_delegate.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_desktop.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_toolbar_model_delegate.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_bubble_manager.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/mouse_lock_controller.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/fast_unload_controller.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/media_utils.h"
#include "chrome/browser/ui/search/search_delegate.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/settings_window_manager.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_utils.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/validation_message_bubble.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiling.h"
#include "chrome/common/search_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/bubble/bubble_controller.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/history/core/browser/top_sites.h"
#include "components/search/search.h"
#include "components/security_state/security_state_model.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/translate/core/browser/language_state.h"
#include "components/ui/zoom/zoom_controller.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "net/base/filename_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/public/web/WebWindowFeatures.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/view_ids.h"
#include "components/autofill/core/browser/autofill_ie_toolbar_import_win.h"
#include "ui/base/touch/touch_device.h"
#include "ui/base/win/shell.h"
#endif  // OS_WIN

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#endif

#if defined(USE_ASH)
#include "ash/ash_switches.h"
#include "ash/shell.h"
#endif

using base::TimeDelta;
using base::UserMetricsAction;
using content::NativeWebKeyboardEvent;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::PluginService;
using content::Referrer;
using content::RenderWidgetHostView;
using content::SiteInstance;
using content::WebContents;
using extensions::Extension;
using security_state::SecurityStateModel;
using ui::WebDialogDelegate;
using web_modal::WebContentsModalDialogManager;
using blink::WebWindowFeatures;

///////////////////////////////////////////////////////////////////////////////

namespace {

// How long we wait before updating the browser chrome while loading a page.
const int kUIUpdateCoalescingTimeMS = 200;

BrowserWindow* CreateBrowserWindow(Browser* browser) {
  return BrowserWindow::CreateBrowserWindow(browser);
}

// Is the fast tab unload experiment enabled?
bool IsFastTabUnloadEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableFastUnload);
}

// Note: This is a lossy operation. Not all of the policies that can be
// expressed by a SecurityLevel (a //chrome concept) can be expressed by
// a content::SecurityStyle.
content::SecurityStyle SecurityLevelToSecurityStyle(
    SecurityStateModel::SecurityLevel security_level) {
  switch (security_level) {
    case SecurityStateModel::NONE:
      return content::SECURITY_STYLE_UNAUTHENTICATED;
    case SecurityStateModel::SECURITY_WARNING:
    case SecurityStateModel::SECURITY_POLICY_WARNING:
      return content::SECURITY_STYLE_WARNING;
    case SecurityStateModel::EV_SECURE:
    case SecurityStateModel::SECURE:
      return content::SECURITY_STYLE_AUTHENTICATED;
    case SecurityStateModel::SECURITY_ERROR:
      return content::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  }

  NOTREACHED();
  return content::SECURITY_STYLE_UNKNOWN;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Browser, CreateParams:

Browser::CreateParams::CreateParams(Profile* profile,
                                    chrome::HostDesktopType host_desktop_type)
    : type(TYPE_TABBED),
      profile(profile),
      host_desktop_type(host_desktop_type),
      trusted_source(false),
      initial_show_state(ui::SHOW_STATE_DEFAULT),
      is_session_restore(false),
      window(NULL) {
}

Browser::CreateParams::CreateParams(Type type,
                                    Profile* profile,
                                    chrome::HostDesktopType host_desktop_type)
    : type(type),
      profile(profile),
      host_desktop_type(host_desktop_type),
      trusted_source(false),
      initial_show_state(ui::SHOW_STATE_DEFAULT),
      is_session_restore(false),
      window(NULL) {
}

// static
Browser::CreateParams Browser::CreateParams::CreateForApp(
    const std::string& app_name,
    bool trusted_source,
    const gfx::Rect& window_bounds,
    Profile* profile,
    chrome::HostDesktopType host_desktop_type) {
  DCHECK(!app_name.empty());

  CreateParams params(TYPE_POPUP, profile, host_desktop_type);
  params.app_name = app_name;
  params.trusted_source = trusted_source;
  params.initial_bounds = window_bounds;

  return params;
}

// static
Browser::CreateParams Browser::CreateParams::CreateForDevTools(
    Profile* profile,
    chrome::HostDesktopType host_desktop_type) {
  CreateParams params(TYPE_POPUP, profile, host_desktop_type);
  params.app_name = DevToolsWindow::kDevToolsApp;
  params.trusted_source = true;
  return params;
}

////////////////////////////////////////////////////////////////////////////////
// Browser, InterstitialObserver:

class Browser::InterstitialObserver : public content::WebContentsObserver {
 public:
  InterstitialObserver(Browser* browser, content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        browser_(browser) {
  }

  void DidAttachInterstitialPage() override {
    browser_->UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
  }

  void DidDetachInterstitialPage() override {
    browser_->UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
  }

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialObserver);
};

///////////////////////////////////////////////////////////////////////////////
// Browser, Constructors, Creation, Showing:

Browser::Browser(const CreateParams& params)
    : extension_registry_observer_(this),
      type_(params.type),
      profile_(params.profile),
      window_(NULL),
      tab_strip_model_delegate_(new chrome::BrowserTabStripModelDelegate(this)),
      tab_strip_model_(
          new TabStripModel(tab_strip_model_delegate_.get(), params.profile)),
      app_name_(params.app_name),
      is_trusted_source_(params.trusted_source),
      cancel_download_confirmation_state_(NOT_PROMPTED),
      override_bounds_(params.initial_bounds),
      initial_show_state_(params.initial_show_state),
      is_session_restore_(params.is_session_restore),
      host_desktop_type_(params.host_desktop_type),
      content_setting_bubble_model_delegate_(
          new BrowserContentSettingBubbleModelDelegate(this)),
      toolbar_model_delegate_(new BrowserToolbarModelDelegate(this)),
      live_tab_context_(new BrowserLiveTabContext(this)),
      synced_window_delegate_(new BrowserSyncedWindowDelegate(this)),
      bookmark_bar_state_(BookmarkBar::HIDDEN),
      command_controller_(new chrome::BrowserCommandController(this)),
      window_has_shown_(false),
      chrome_updater_factory_(this),
      weak_factory_(this) {
  // If this causes a crash then a window is being opened using a profile type
  // that is disallowed by policy. The crash prevents the disabled window type
  // from opening at all, but the path that triggered it should be fixed.
  CHECK(IncognitoModePrefs::CanOpenBrowser(profile_));
  CHECK(!profile_->IsGuestSession() || profile_->IsOffTheRecord())
      << "Only off the record browser may be opened in guest mode";
  DCHECK(!profile_->IsSystemProfile())
      << "The system profile should never have a real browser.";
  // TODO(mlerman): After this hits stable channel, see if there are counts
  // for this metric. If not, change the DCHECK above to a CHECK.
  if (profile_->IsSystemProfile())
    content::RecordAction(base::UserMetricsAction("BrowserForSystemProfile"));

  // TODO(jeremy): Move to initializer list once flag is removed.
  if (IsFastTabUnloadEnabled())
    fast_unload_controller_.reset(new chrome::FastUnloadController(this));
  else
    unload_controller_.reset(new chrome::UnloadController(this));

  tab_strip_model_->AddObserver(this);

  toolbar_model_.reset(new ToolbarModelImpl(toolbar_model_delegate_.get()));
  search_model_.reset(new SearchModel());
  search_delegate_.reset(new SearchDelegate(search_model_.get()));

  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(profile_));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());
#if defined(ENABLE_THEMES)
  registrar_.Add(
      this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile_)));
#endif
  registrar_.Add(this, chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED,
                 content::NotificationService::AllSources());

  profile_pref_registrar_.Init(profile_->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kDevToolsDisabled,
      base::Bind(&Browser::OnDevToolsDisabledChanged, base::Unretained(this)));
  profile_pref_registrar_.Add(
      bookmarks::prefs::kShowBookmarkBar,
      base::Bind(&Browser::UpdateBookmarkBarState, base::Unretained(this),
                 BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE));

  // NOTE: These prefs all need to be explicitly destroyed in the destructor
  // or you'll get a nasty surprise when you run the incognito tests.
  encoding_auto_detect_.Init(prefs::kWebKitUsesUniversalDetector,
                             profile_->GetPrefs());

  if (search::IsInstantExtendedAPIEnabled() && is_type_tabbed())
    instant_controller_.reset(new BrowserInstantController(this));

  if (extensions::HostedAppBrowserController::IsForHostedApp(this)) {
    hosted_app_controller_.reset(
        new extensions::HostedAppBrowserController(this));
  }

  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_INIT);

  ProfileMetrics::LogProfileLaunch(profile_);

  window_ = params.window ? params.window : CreateBrowserWindow(this);

  if (hosted_app_controller_)
    hosted_app_controller_->UpdateLocationBarVisibility(false);

  // Create the extension window controller before sending notifications.
  extension_window_controller_.reset(
      new BrowserExtensionWindowController(this));

  SessionService* session_service =
      SessionServiceFactory::GetForProfileForSessionRestore(profile_);
  if (session_service)
    session_service->WindowOpened(this);

  // TODO(beng): move to ChromeBrowserMain:
  if (first_run::ShouldDoPersonalDataManagerFirstRun()) {
#if defined(OS_WIN)
    // Notify PDM that this is a first run.
    ImportAutofillDataWin(
        autofill::PersonalDataManagerFactory::GetForProfile(profile_));
#endif  // defined(OS_WIN)
  }

  exclusive_access_manager_.reset(
      new ExclusiveAccessManager(window_->GetExclusiveAccessContext()));

  // TODO(beng): Move BrowserList::AddBrowser() to the end of this function and
  //             replace uses of this with BL's notifications.
  BrowserList::AddBrowser(this);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY, content::Source<Browser>(this),
      content::NotificationService::NoDetails());
}

Browser::~Browser() {
  // Stop observing notifications before continuing with destruction. Profile
  // destruction will unload extensions and reentrant calls to Browser:: should
  // be avoided while it is being torn down.
  registrar_.RemoveAll();
  extension_registry_observer_.RemoveAll();

  // The tab strip should not have any tabs at this point.
  DCHECK(tab_strip_model_->empty());
  tab_strip_model_->RemoveObserver(this);
  bubble_manager_.reset();

  // Destroy the BrowserCommandController before removing the browser, so that
  // it doesn't act on any notifications that are sent as a result of removing
  // the browser.
  command_controller_.reset();
  BrowserList::RemoveBrowser(this);

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (session_service)
    session_service->WindowClosed(session_id_);

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  if (tab_restore_service)
    tab_restore_service->BrowserClosed(live_tab_context());

#if !defined(OS_MACOSX)
  if (!chrome::GetTotalBrowserCountForProfile(profile_)) {
    // We're the last browser window with this profile. We need to nuke the
    // TabRestoreService, which will start the shutdown of the
    // NavigationControllers and allow for proper shutdown. If we don't do this
    // chrome won't shutdown cleanly, and may end up crashing when some
    // thread tries to use the IO thread (or another thread) that is no longer
    // valid.
    // This isn't a valid assumption for Mac OS, as it stays running after
    // the last browser has closed. The Mac equivalent is in its app
    // controller.
    TabRestoreServiceFactory::ResetForProfile(profile_);
  }
#endif

  profile_pref_registrar_.RemoveAll();

  encoding_auto_detect_.Destroy();

  // Destroy BrowserExtensionWindowController before the incognito profile
  // is destroyed to make sure the chrome.windows.onRemoved event is sent.
  extension_window_controller_.reset();

  // Destroy BrowserInstantController before the incongnito profile is destroyed
  // because the InstantController destructor depends on this profile.
  instant_controller_.reset();

  if (profile_->IsOffTheRecord() &&
      !BrowserList::IsOffTheRecordSessionActiveForProfile(profile_)) {
    if (profile_->IsGuestSession()) {
// ChromeOS handles guest data independently.
#if !defined(OS_CHROMEOS)
      // Clear all browsing data once a Guest Session completes. The Guest
      // profile has BrowserContextKeyedServices that the Incognito profile
      // doesn't, so the ProfileDestroyer can't delete it properly.
      // TODO(mlerman): Delete the guest using an improved ProfileDestroyer.
      profiles::RemoveBrowsingDataForProfile(profile_->GetPath());
#endif
    } else {
      // An incognito profile is no longer needed, this indirectly frees
      // its cache and cookies once it gets destroyed at the appropriate time.
      ProfileDestroyer::DestroyProfileWhenAppropriate(profile_);
    }
  }

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  int num_downloads;
  if (OkToCloseWithInProgressDownloads(&num_downloads) ==
          DOWNLOAD_CLOSE_BROWSER_SHUTDOWN &&
      !browser_defaults::kBrowserAliveWithNoWindows) {
    DownloadService::CancelAllDownloads();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Getters & Setters

ChromeBubbleManager* Browser::GetBubbleManager() {
  if (!bubble_manager_)
    bubble_manager_.reset(new ChromeBubbleManager(tab_strip_model_.get()));
  return bubble_manager_.get();
}

FindBarController* Browser::GetFindBarController() {
  if (!find_bar_controller_.get()) {
    FindBar* find_bar = window_->CreateFindBar();
    find_bar_controller_.reset(new FindBarController(find_bar));
    find_bar->SetFindBarController(find_bar_controller_.get());
    find_bar_controller_->ChangeWebContents(
        tab_strip_model_->GetActiveWebContents());
    find_bar_controller_->find_bar()->MoveWindowIfNecessary(gfx::Rect());
  }
  return find_bar_controller_.get();
}

bool Browser::HasFindBarController() const {
  return find_bar_controller_.get() != NULL;
}

bool Browser::is_app() const {
  return !app_name_.empty();
}

bool Browser::is_devtools() const {
  return app_name_ == DevToolsWindow::kDevToolsApp;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, State Storage and Retrieval for UI:

gfx::Image Browser::GetCurrentPageIcon() const {
  WebContents* web_contents = tab_strip_model_->GetActiveWebContents();
  // |web_contents| can be NULL since GetCurrentPageIcon() is called by the
  // window during the window's creation (before tabs have been added).
  favicon::FaviconDriver* favicon_driver =
      web_contents
          ? favicon::ContentFaviconDriver::FromWebContents(web_contents)
          : nullptr;
  return favicon_driver ? favicon_driver->GetFavicon() : gfx::Image();
}

base::string16 Browser::GetWindowTitleForCurrentTab() const {
  WebContents* contents = tab_strip_model_->GetActiveWebContents();
  base::string16 title;

  // |contents| can be NULL because GetWindowTitleForCurrentTab is called by the
  // window during the window's creation (before tabs have been added).
  if (contents) {
    // The web app frame uses the host instead of the title.
    if (ShouldUseWebAppFrame())
      return base::UTF8ToUTF16(contents->GetURL().host());

    title = contents->GetTitle();
    FormatTitleForDisplay(&title);
  }
  if (title.empty())
    title = CoreTabHelper::GetDefaultTitle();

#if defined(OS_MACOSX)
  // On Mac, we don't want to suffix the page title with
  // the application name.
  return title;
#elif defined(USE_ASH)
  // On Ash, we don't want to suffix the page title with the application name,
  // but on Windows, where USE_ASH can also be true, we still want the prefix
  // on desktop.
  if (host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH)
    return title;
#endif
  // Don't append the app name to window titles on app frames and app popups
  return is_app() ?
      title :
      l10n_util::GetStringFUTF16(IDS_BROWSER_WINDOW_TITLE_FORMAT, title);
}

// static
void Browser::FormatTitleForDisplay(base::string16* title) {
  size_t current_index = 0;
  size_t match_index;
  while ((match_index = title->find(L'\n', current_index)) !=
         base::string16::npos) {
    title->replace(match_index, 1, base::string16());
    current_index = match_index;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, OnBeforeUnload handling:

bool Browser::ShouldCloseWindow() {
  if (!CanCloseWithInProgressDownloads())
    return false;

  if (IsFastTabUnloadEnabled())
    return fast_unload_controller_->ShouldCloseWindow();
  return unload_controller_->ShouldCloseWindow();
}

bool Browser::CallBeforeUnloadHandlers(
    const base::Callback<void(bool)>& on_close_confirmed) {
  cancel_download_confirmation_state_ = RESPONSE_RECEIVED;
  if (IsFastTabUnloadEnabled()) {
    return fast_unload_controller_->CallBeforeUnloadHandlers(
        on_close_confirmed);
  }
  return unload_controller_->CallBeforeUnloadHandlers(on_close_confirmed);
}

void Browser::ResetBeforeUnloadHandlers() {
  cancel_download_confirmation_state_ = NOT_PROMPTED;
  if (IsFastTabUnloadEnabled())
    fast_unload_controller_->ResetBeforeUnloadHandlers();
  else
    unload_controller_->ResetBeforeUnloadHandlers();
}

bool Browser::HasCompletedUnloadProcessing() const {
  DCHECK(IsFastTabUnloadEnabled());
  return fast_unload_controller_->HasCompletedUnloadProcessing();
}

bool Browser::IsAttemptingToCloseBrowser() const {
  if (IsFastTabUnloadEnabled())
    return fast_unload_controller_->is_attempting_to_close_browser();
  return unload_controller_->is_attempting_to_close_browser();
}

void Browser::OnWindowClosing() {
  if (!ShouldCloseWindow())
    return;

  // Application should shutdown on last window close if the user is explicitly
  // trying to quit, or if there is nothing keeping the browser alive (such as
  // AppController on the Mac, or BackgroundContentsService for background
  // pages).
  bool should_quit_if_last_browser =
      browser_shutdown::IsTryingToQuit() || !chrome::WillKeepAlive();

  if (should_quit_if_last_browser && ShouldStartShutdown())
    browser_shutdown::OnShutdownStarting(browser_shutdown::WINDOW_CLOSE);

  // Don't use GetForProfileIfExisting here, we want to force creation of the
  // session service so that user can restore what was open.
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile());
  if (session_service)
    session_service->WindowClosing(session_id());

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());

#if defined(USE_AURA)
  if (tab_restore_service && is_app() && !is_devtools())
    tab_restore_service->BrowserClosing(live_tab_context());
#endif

  if (tab_restore_service && is_type_tabbed() && tab_strip_model_->count())
    tab_restore_service->BrowserClosing(live_tab_context());

  // TODO(sky): convert session/tab restore to use notification.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_CLOSING,
      content::Source<Browser>(this),
      content::NotificationService::NoDetails());

  if (!IsFastTabUnloadEnabled())
    tab_strip_model_->CloseAllTabs();
}

////////////////////////////////////////////////////////////////////////////////
// In-progress download termination handling:

void Browser::InProgressDownloadResponse(bool cancel_downloads) {
  if (cancel_downloads) {
    cancel_download_confirmation_state_ = RESPONSE_RECEIVED;
    chrome::CloseWindow(this);
    return;
  }

  // Sets the confirmation state to NOT_PROMPTED so that if the user tries to
  // close again we'll show the warning again.
  cancel_download_confirmation_state_ = NOT_PROMPTED;

  // Show the download page so the user can figure-out what downloads are still
  // in-progress.
  chrome::ShowDownloads(this);

  // Reset UnloadController::is_attempting_to_close_browser_ so that we don't
  // prompt every time any tab is closed. http://crbug.com/305516
  if (IsFastTabUnloadEnabled())
    fast_unload_controller_->CancelWindowClose();
  else
    unload_controller_->CancelWindowClose();
}

Browser::DownloadClosePreventionType Browser::OkToCloseWithInProgressDownloads(
    int* num_downloads_blocking) const {
  DCHECK(num_downloads_blocking);
  *num_downloads_blocking = 0;

  // If we're not running a full browser process with a profile manager
  // (testing), it's ok to close the browser.
  if (!g_browser_process->profile_manager())
    return DOWNLOAD_CLOSE_OK;

  int total_download_count =
      DownloadService::NonMaliciousDownloadCountAllProfiles();
  if (total_download_count == 0)
    return DOWNLOAD_CLOSE_OK;   // No downloads; can definitely close.

  // Figure out how many windows are open total, and associated with this
  // profile, that are relevant for the ok-to-close decision.
  int profile_window_count = 0;
  int total_window_count = 0;
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    // Don't count this browser window or any other in the process of closing.
    Browser* const browser = *it;
    // Window closing may be delayed, and windows that are in the process of
    // closing don't count against our totals.
    if (browser == this || browser->IsAttemptingToCloseBrowser())
      continue;

    if (it->profile() == profile())
      profile_window_count++;
    total_window_count++;
  }

  // If there aren't any other windows, we're at browser shutdown,
  // which would cancel all current downloads.
  if (total_window_count == 0) {
    *num_downloads_blocking = total_download_count;
    return DOWNLOAD_CLOSE_BROWSER_SHUTDOWN;
  }

  // If there aren't any other windows on our profile, and we're an incognito
  // profile, and there are downloads associated with that profile,
  // those downloads would be cancelled by our window (-> profile) close.
  DownloadService* download_service =
      DownloadServiceFactory::GetForBrowserContext(profile());
  if ((profile_window_count == 0) &&
      (download_service->NonMaliciousDownloadCount() > 0) &&
      profile()->IsOffTheRecord()) {
    *num_downloads_blocking = download_service->NonMaliciousDownloadCount();
    return DOWNLOAD_CLOSE_LAST_WINDOW_IN_INCOGNITO_PROFILE;
  }

  // Those are the only conditions under which we will block shutdown.
  return DOWNLOAD_CLOSE_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Browser, Tab adding/showing functions:

void Browser::WindowFullscreenStateChanged() {
  exclusive_access_manager_->fullscreen_controller()
      ->WindowFullscreenStateChanged();
  command_controller_->FullscreenStateChanged();
  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TOGGLE_FULLSCREEN);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted browser commands:

void Browser::ToggleFullscreenModeWithExtension(const GURL& extension_url) {
  exclusive_access_manager_->fullscreen_controller()
      ->ToggleBrowserFullscreenModeWithExtension(extension_url);
}

bool Browser::SupportsWindowFeature(WindowFeature feature) const {
  return SupportsWindowFeatureImpl(feature, true);
}

bool Browser::CanSupportWindowFeature(WindowFeature feature) const {
  return SupportsWindowFeatureImpl(feature, false);
}

void Browser::ToggleEncodingAutoDetect() {
  content::RecordAction(UserMetricsAction("AutoDetectChange"));
  encoding_auto_detect_.SetValue(!encoding_auto_detect_.GetValue());
  // If "auto detect" is turned on, then any current override encoding
  // is cleared. This also implicitly performs a reload.
  // OTOH, if "auto detect" is turned off, we don't change the currently
  // active encoding.
  if (encoding_auto_detect_.GetValue()) {
    WebContents* contents = tab_strip_model_->GetActiveWebContents();
    if (contents)
      contents->ResetOverrideEncoding();
  }
}

void Browser::OverrideEncoding(int encoding_id) {
  content::RecordAction(UserMetricsAction("OverrideEncoding"));
  const std::string selected_encoding =
      CharacterEncoding::GetCanonicalEncodingNameByCommandId(encoding_id);
  WebContents* contents = tab_strip_model_->GetActiveWebContents();
  if (!selected_encoding.empty() && contents)
     contents->SetOverrideEncoding(selected_encoding);
  // Update the list of recently selected encodings.
  std::string new_selected_encoding_list;
  if (CharacterEncoding::UpdateRecentlySelectedEncoding(
        profile_->GetPrefs()->GetString(prefs::kRecentlySelectedEncoding),
        encoding_id,
        &new_selected_encoding_list)) {
    profile_->GetPrefs()->SetString(prefs::kRecentlySelectedEncoding,
                                    new_selected_encoding_list);
  }
}

void Browser::OpenFile() {
  content::RecordAction(UserMetricsAction("OpenFile"));
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(
          tab_strip_model_->GetActiveWebContents()));

  const base::FilePath directory = profile_->last_selected_directory();

  // TODO(beng): figure out how to juggle this.
  gfx::NativeWindow parent_window = window_->GetNativeWindow();
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.support_drive = true;
  select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                  base::string16(),
                                  directory,
                                  &file_types,
                                  0,
                                  base::FilePath::StringType(),
                                  parent_window,
                                  NULL);
}

void Browser::UpdateDownloadShelfVisibility(bool visible) {
  if (GetStatusBubble())
    GetStatusBubble()->UpdateDownloadShelfVisibility(visible);
}

///////////////////////////////////////////////////////////////////////////////

void Browser::UpdateUIForNavigationInTab(WebContents* contents,
                                         ui::PageTransition transition,
                                         bool user_initiated) {
  tab_strip_model_->TabNavigating(contents, transition);

  bool contents_is_selected =
      contents == tab_strip_model_->GetActiveWebContents();
  if (user_initiated && contents_is_selected && window()->GetLocationBar()) {
    // Forcibly reset the location bar if the url is going to change in the
    // current tab, since otherwise it won't discard any ongoing user edits,
    // since it doesn't realize this is a user-initiated action.
    window()->GetLocationBar()->Revert();
  }

  if (GetStatusBubble())
    GetStatusBubble()->Hide();

  // Update the location bar. This is synchronous. We specifically don't
  // update the load state since the load hasn't started yet and updating it
  // will put it out of sync with the actual state like whether we're
  // displaying a favicon, which controls the throbber. If we updated it here,
  // the throbber will show the default favicon for a split second when
  // navigating away from the new tab page.
  ScheduleUIUpdate(contents, content::INVALIDATE_TYPE_URL);

  if (contents_is_selected)
    contents->SetInitialFocus();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, PageNavigator implementation:

WebContents* Browser::OpenURL(const OpenURLParams& params) {
  return OpenURLFromTab(NULL, params);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModelObserver implementation:

void Browser::TabInsertedAt(WebContents* contents,
                            int index,
                            bool foreground) {
  SetAsDelegate(contents, true);

  SessionTabHelper* session_tab_helper =
      SessionTabHelper::FromWebContents(contents);
  session_tab_helper->SetWindowID(session_id());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_PARENTED,
      content::Source<content::WebContents>(contents),
      content::NotificationService::NoDetails());

  SyncHistoryWithTabs(index);

  // Make sure the loading state is updated correctly, otherwise the throbber
  // won't start if the page is loading.
  LoadingStateChanged(contents, true);

  interstitial_observers_.push_back(new InterstitialObserver(this, contents));

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (session_service) {
    session_service->TabInserted(contents);
    int new_active_index = tab_strip_model_->active_index();
    if (index < new_active_index)
      session_service->SetSelectedTabInWindow(session_id(),
                                              new_active_index);
  }
}

void Browser::TabClosingAt(TabStripModel* tab_strip_model,
                           WebContents* contents,
                           int index) {
  exclusive_access_manager_->OnTabClosing(contents);
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (session_service)
    session_service->TabClosing(contents);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CLOSING,
      content::Source<NavigationController>(&contents->GetController()),
      content::NotificationService::NoDetails());

  // Sever the WebContents' connection back to us.
  SetAsDelegate(contents, false);
}

void Browser::TabDetachedAt(WebContents* contents, int index) {
  // TabDetachedAt is called before TabStripModel has updated the
  // active index.
  int old_active_index = tab_strip_model_->active_index();
  if (index < old_active_index && !tab_strip_model_->closing_all()) {
    SessionService* session_service =
        SessionServiceFactory::GetForProfileIfExisting(profile_);
    if (session_service)
      session_service->SetSelectedTabInWindow(session_id(),
                                              old_active_index - 1);
  }

  TabDetachedAtImpl(contents, index, DETACH_TYPE_DETACH);
}

void Browser::TabDeactivated(WebContents* contents) {
  exclusive_access_manager_->OnTabDeactivated(contents);
  search_delegate_->OnTabDeactivated(contents);
  SearchTabHelper::FromWebContents(contents)->OnTabDeactivated();

  // Save what the user's currently typing, so it can be restored when we
  // switch back to this tab.
  window_->GetLocationBar()->SaveStateToContents(contents);

  if (instant_controller_)
    instant_controller_->TabDeactivated(contents);
}

void Browser::ActiveTabChanged(WebContents* old_contents,
                               WebContents* new_contents,
                               int index,
                               int reason) {
  content::RecordAction(UserMetricsAction("ActiveTabChanged"));

  // Update the bookmark state, since the BrowserWindow may query it during
  // OnActiveTabChanged() below.
  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_SWITCH);

  // Let the BrowserWindow do its handling.  On e.g. views this changes the
  // focused object, which should happen before we update the toolbar below,
  // since the omnibox expects the correct element to already be focused when it
  // is updated.
  window_->OnActiveTabChanged(old_contents, new_contents, index, reason);

  exclusive_access_manager_->OnTabDetachedFromView(old_contents);

  // Discarded tabs always get reloaded.
  // TODO(georgesak): Validate the usefulness of this. And if needed then move
  // to TabManager.
  if (g_browser_process->GetTabManager() &&
      g_browser_process->GetTabManager()->IsTabDiscarded(new_contents))
    chrome::Reload(this, CURRENT_TAB);

  // If we have any update pending, do it now.
  if (chrome_updater_factory_.HasWeakPtrs() && old_contents)
    ProcessPendingUIUpdates();

  // Propagate the profile to the location bar.
  UpdateToolbar((reason & CHANGE_REASON_REPLACED) == 0);

  if (search::IsInstantExtendedAPIEnabled())
    search_delegate_->OnTabActivated(new_contents);

  // Update reload/stop state.
  command_controller_->LoadingStateChanged(new_contents->IsLoading(), true);

  // Update commands to reflect current state.
  command_controller_->TabStateChanged();

  // Reset the status bubble.
  StatusBubble* status_bubble = GetStatusBubble();
  if (status_bubble) {
    status_bubble->Hide();

    // Show the loading state (if any).
    status_bubble->SetStatus(CoreTabHelper::FromWebContents(
        tab_strip_model_->GetActiveWebContents())->GetStatusText());
  }

  if (HasFindBarController()) {
    find_bar_controller_->ChangeWebContents(new_contents);
    find_bar_controller_->find_bar()->MoveWindowIfNecessary(gfx::Rect());
  }

  // Update sessions (selected tab index and last active time). Don't force
  // creation of sessions. If sessions doesn't exist, the change will be picked
  // up by sessions when created.
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile_);
  if (session_service && !tab_strip_model_->closing_all()) {
    session_service->SetSelectedTabInWindow(session_id(),
                                            tab_strip_model_->active_index());
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(new_contents);
    session_service->SetLastActiveTime(session_id(),
                                       session_tab_helper->session_id(),
                                       base::TimeTicks::Now());
  }

  // This needs to be called after notifying SearchDelegate.
  if (instant_controller_)
    instant_controller_->ActiveTabChanged();

  autofill::ChromeAutofillClient::FromWebContents(new_contents)->TabActivated();
  SearchTabHelper::FromWebContents(new_contents)->OnTabActivated();
}

void Browser::TabMoved(WebContents* contents,
                       int from_index,
                       int to_index) {
  DCHECK(from_index >= 0 && to_index >= 0);
  // Notify the history service.
  SyncHistoryWithTabs(std::min(from_index, to_index));
}

void Browser::TabReplacedAt(TabStripModel* tab_strip_model,
                            WebContents* old_contents,
                            WebContents* new_contents,
                            int index) {
  TabDetachedAtImpl(old_contents, index, DETACH_TYPE_REPLACE);
  exclusive_access_manager_->OnTabClosing(old_contents);
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (session_service)
    session_service->TabClosing(old_contents);
  TabInsertedAt(new_contents,
                index,
                (index == tab_strip_model_->active_index()));

  if (!new_contents->GetController().IsInitialBlankNavigation()) {
    // Send out notification so that observers are updated appropriately.
    int entry_count = new_contents->GetController().GetEntryCount();
    new_contents->GetController().NotifyEntryChanged(
        new_contents->GetController().GetEntryAtIndex(entry_count - 1));
  }

  if (session_service) {
    // The new_contents may end up with a different navigation stack. Force
    // the session service to update itself.
    session_service->TabRestored(new_contents,
                                 tab_strip_model_->IsTabPinned(index));
  }
}

void Browser::TabPinnedStateChanged(WebContents* contents, int index) {
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile());
  if (session_service) {
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(contents);
    session_service->SetPinnedState(session_id(),
                                    session_tab_helper->session_id(),
                                    tab_strip_model_->IsTabPinned(index));
  }
}

void Browser::TabStripEmpty() {
  // Close the frame after we return to the message loop (not immediately,
  // otherwise it will destroy this object before the stack has a chance to
  // cleanly unwind.)
  // Note: This will be called several times if TabStripEmpty is called several
  //       times. This is because it does not close the window if tabs are
  //       still present.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&Browser::CloseFrame, weak_factory_.GetWeakPtr()));

  // Instant may have visible WebContents that need to be detached before the
  // window system closes.
  instant_controller_.reset();
}

bool Browser::CanOverscrollContent() const {
#if defined(OS_WIN)
  // Don't enable overscroll on Windows machines unless they have a touch
  // screen as these machines typically don't have a touchpad capable of
  // horizontal scrolling. We are purposefully biased towards "no" here,
  // so that we don't waste resources capturing screenshots for horizontal
  // overscroll navigation unnecessarily.
  bool allow_overscroll = ui::GetTouchScreensAvailability() ==
      ui::TouchScreensAvailability::ENABLED;
#elif defined(USE_AURA)
  bool allow_overscroll = true;
#else
  bool allow_overscroll = false;
#endif

  if (!allow_overscroll)
    return false;

  const std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kOverscrollHistoryNavigation);
  bool overscroll_enabled = value != "0";
  if (!overscroll_enabled)
    return false;
  if (is_app() || is_devtools() || !is_type_tabbed())
    return false;

  // The detached bookmark bar has appearance of floating above the
  // web-contents. This does not play nicely with overscroll navigation
  // gestures. So disable overscroll navigation when the bookmark bar is in the
  // detached state and the overscroll effect moves the layers.
  if (value == "1" && bookmark_bar_state_ == BookmarkBar::DETACHED)
    return false;
  return true;
}

bool Browser::ShouldPreserveAbortedURLs(WebContents* source) {
  // Allow failed URLs to stick around in the omnibox on the NTP, but not when
  // other pages have committed.
  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());
  if (!profile || !source->GetController().GetLastCommittedEntry())
    return false;
  GURL committed_url(source->GetController().GetLastCommittedEntry()->GetURL());
  return search::IsNTPURL(committed_url, profile);
}

void Browser::SetFocusToLocationBar(bool select_all) {
  // Two differences between this and FocusLocationBar():
  // (1) This doesn't get recorded in user metrics, since it's called
  //     internally.
  // (2) This checks whether the location bar can be focused, and if not, clears
  //     the focus.  FocusLocationBar() is only reached when the location bar is
  //     focusable, but this may be reached at other times, e.g. while in
  //     fullscreen mode, where we need to leave focus in a consistent state.
  window_->SetFocusToLocationBar(select_all);
}

bool Browser::PreHandleKeyboardEvent(content::WebContents* source,
                                     const NativeWebKeyboardEvent& event,
                                     bool* is_keyboard_shortcut) {
  // Forward keyboard events to the manager for fullscreen / mouse lock. This
  // may consume the event (e.g., Esc exits fullscreen mode).
  // TODO(koz): Write a test for this http://crbug.com/100441.
  if (exclusive_access_manager_->HandleUserKeyPress(event))
    return true;

  return window()->PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void Browser::HandleKeyboardEvent(content::WebContents* source,
                                  const NativeWebKeyboardEvent& event) {
  DevToolsWindow* devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(source);
  bool handled = false;
  if (devtools_window)
    handled = devtools_window->ForwardKeyboardEvent(event);

  if (!handled)
    window()->HandleKeyboardEvent(event);
}

bool Browser::TabsNeedBeforeUnloadFired() {
  if (IsFastTabUnloadEnabled())
    return fast_unload_controller_->TabsNeedBeforeUnloadFired();
  return unload_controller_->TabsNeedBeforeUnloadFired();
}

void Browser::ShowValidationMessage(content::WebContents* web_contents,
                                    const gfx::Rect& anchor_in_root_view,
                                    const base::string16& main_text,
                                    const base::string16& sub_text) {
  validation_message_bubble_ =
      TabDialogs::FromWebContents(web_contents)
          ->ShowValidationMessage(anchor_in_root_view, main_text, sub_text);
}

void Browser::HideValidationMessage(content::WebContents* web_contents) {
  validation_message_bubble_.reset();
}

void Browser::MoveValidationMessage(content::WebContents* web_contents,
                                    const gfx::Rect& anchor_in_root_view) {
  if (!validation_message_bubble_)
    return;
  RenderWidgetHostView* rwhv = web_contents->GetRenderWidgetHostView();
  if (rwhv) {
    validation_message_bubble_->SetPositionRelativeToAnchor(
        rwhv->GetRenderWidgetHost(), anchor_in_root_view);
  }
}

bool Browser::PreHandleGestureEvent(content::WebContents* source,
                                    const blink::WebGestureEvent& event) {
  // Disable pinch zooming in undocked dev tools window due to poor UX.
  if (app_name() == DevToolsWindow::kDevToolsApp)
    return event.type == blink::WebGestureEvent::GesturePinchBegin ||
           event.type == blink::WebGestureEvent::GesturePinchUpdate ||
           event.type == blink::WebGestureEvent::GesturePinchEnd;

  return false;
}

bool Browser::CanDragEnter(content::WebContents* source,
                           const content::DropData& data,
                           blink::WebDragOperationsMask operations_allowed) {
  // Disallow drag-and-drop navigation for Settings windows which do not support
  // external navigation.
  if ((operations_allowed & blink::WebDragOperationLink) &&
      chrome::SettingsWindowManager::GetInstance()->IsSettingsBrowser(this)) {
    return false;
  }
  return true;
}

content::SecurityStyle Browser::GetSecurityStyle(
    WebContents* web_contents,
    content::SecurityStyleExplanations* security_style_explanations) {
  ChromeSecurityStateModelClient* model_client =
      ChromeSecurityStateModelClient::FromWebContents(web_contents);
  DCHECK(model_client);
  const SecurityStateModel::SecurityInfo& security_info =
      model_client->GetSecurityInfo();

  const content::SecurityStyle security_style =
      SecurityLevelToSecurityStyle(security_info.security_level);

  security_style_explanations->ran_insecure_content_style =
      SecurityLevelToSecurityStyle(
          SecurityStateModel::kRanInsecureContentLevel);
  security_style_explanations->displayed_insecure_content_style =
      SecurityLevelToSecurityStyle(
          SecurityStateModel::kDisplayedInsecureContentLevel);

  // Check if the page is HTTP; if so, no explanations are needed. Note
  // that SECURITY_STYLE_UNAUTHENTICATED does not necessarily mean that
  // the page is loaded over HTTP, because the security style merely
  // represents how the embedder wishes to display the security state of
  // the page, and the embedder can choose to display HTTPS page as HTTP
  // if it wants to (for example, displaying deprecated crypto
  // algorithms with the same UI treatment as HTTP pages).
  security_style_explanations->scheme_is_cryptographic =
      security_info.scheme_is_cryptographic;
  if (!security_info.scheme_is_cryptographic) {
    return security_style;
  }

  if (security_info.sha1_deprecation_status ==
      SecurityStateModel::DEPRECATED_SHA1_MAJOR) {
    security_style_explanations->broken_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_MAJOR_SHA1),
            l10n_util::GetStringUTF8(IDS_MAJOR_SHA1_DESCRIPTION),
            security_info.cert_id));
  } else if (security_info.sha1_deprecation_status ==
             SecurityStateModel::DEPRECATED_SHA1_MINOR) {
    security_style_explanations->unauthenticated_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_MINOR_SHA1),
            l10n_util::GetStringUTF8(IDS_MINOR_SHA1_DESCRIPTION),
            security_info.cert_id));
  }

  security_style_explanations->ran_insecure_content =
      security_info.mixed_content_status ==
          SecurityStateModel::RAN_MIXED_CONTENT ||
      security_info.mixed_content_status ==
          SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT;
  security_style_explanations->displayed_insecure_content =
      security_info.mixed_content_status ==
          SecurityStateModel::DISPLAYED_MIXED_CONTENT ||
      security_info.mixed_content_status ==
          SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT;

  if (net::IsCertStatusError(security_info.cert_status)) {
    base::string16 error_string = base::UTF8ToUTF16(net::ErrorToString(
        net::MapCertStatusToNetError(security_info.cert_status)));

    content::SecurityStyleExplanation explanation(
        l10n_util::GetStringUTF8(IDS_CERTIFICATE_CHAIN_ERROR),
        l10n_util::GetStringFUTF8(
            IDS_CERTIFICATE_CHAIN_ERROR_DESCRIPTION_FORMAT, error_string),
        security_info.cert_id);

    if (net::IsCertStatusMinorError(security_info.cert_status))
      security_style_explanations->unauthenticated_explanations.push_back(
          explanation);
    else
      security_style_explanations->broken_explanations.push_back(explanation);
  } else {
    // If the certificate does not have errors and is not using
    // deprecated SHA1, then add an explanation that the certificate is
    // valid.
    if (security_info.sha1_deprecation_status ==
        SecurityStateModel::NO_DEPRECATED_SHA1) {
      security_style_explanations->secure_explanations.push_back(
          content::SecurityStyleExplanation(
              l10n_util::GetStringUTF8(IDS_VALID_SERVER_CERTIFICATE),
              l10n_util::GetStringUTF8(
                  IDS_VALID_SERVER_CERTIFICATE_DESCRIPTION),
              security_info.cert_id));
    }
  }

  if (security_info.is_secure_protocol_and_ciphersuite) {
    security_style_explanations->secure_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_SECURE_PROTOCOL_AND_CIPHERSUITE),
            l10n_util::GetStringUTF8(
                IDS_SECURE_PROTOCOL_AND_CIPHERSUITE_DESCRIPTION)));
  }

  return security_style;
}

void Browser::ShowCertificateViewerInDevTools(
    content::WebContents* web_contents, int cert_id) {
  DevToolsWindow* devtools_window =
      DevToolsWindow::GetInstanceForInspectedWebContents(web_contents);
  if (devtools_window)
    devtools_window->ShowCertificateViewer(cert_id);
}

scoped_ptr<content::BluetoothChooser> Browser::RunBluetoothChooser(
    content::WebContents* web_contents,
    const content::BluetoothChooser::EventHandler& event_handler,
    const GURL& origin) {
  scoped_ptr<BluetoothChooserDesktop> bluetooth_chooser_desktop(
      new BluetoothChooserDesktop(event_handler));
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  scoped_ptr<BluetoothChooserBubbleDelegate> bubble_delegate(
      new BluetoothChooserBubbleDelegate(browser));
  BluetoothChooserBubbleDelegate* bubble_delegate_ptr = bubble_delegate.get();

  // Wire the ChooserBubbleDelegate to the BluetoothChooser.
  bluetooth_chooser_desktop->set_bluetooth_chooser_bubble_delegate(
      bubble_delegate_ptr);
  bubble_delegate->set_bluetooth_chooser(bluetooth_chooser_desktop.get());

  BubbleReference bubble_controller =
      browser->GetBubbleManager()->ShowBubble(std::move(bubble_delegate));
  bubble_delegate_ptr->set_bubble_controller(bubble_controller);

  return std::move(bluetooth_chooser_desktop);
}

bool Browser::RequestAppBanner(content::WebContents* web_contents) {
  banners::AppBannerManagerDesktop* manager =
      banners::AppBannerManagerDesktop::FromWebContents(web_contents);
  if (manager) {
    manager->RequestAppBanner(web_contents->GetMainFrame(),
                              web_contents->GetLastCommittedURL(), true);
    return true;
  }
  return false;
}

bool Browser::IsMouseLocked() const {
  return exclusive_access_manager_->mouse_lock_controller()->IsMouseLocked();
}

void Browser::OnWindowDidShow() {
  if (window_has_shown_)
    return;
  window_has_shown_ = true;

  startup_metric_utils::RecordBrowserWindowDisplay(base::TimeTicks::Now());

  // Nothing to do for non-tabbed windows.
  if (!is_type_tabbed())
    return;

  // Show any pending global error bubble.
  GlobalErrorService* service =
      GlobalErrorServiceFactory::GetForProfile(profile());
  GlobalError* error = service->GetFirstGlobalErrorWithBubbleView();
  if (error)
    error->ShowBubbleView(this);
}

void Browser::ShowFirstRunBubble() {
  window()->GetLocationBar()->ShowFirstRunBubble();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, content::WebContentsDelegate implementation:

WebContents* Browser::OpenURLFromTab(WebContents* source,
                                     const OpenURLParams& params) {
  if (is_devtools()) {
    DevToolsWindow* window = DevToolsWindow::AsDevToolsWindow(source);
    DCHECK(window);
    return window->OpenURLFromTab(source, params);
  }

  chrome::NavigateParams nav_params(this, params.url, params.transition);
  FillNavigateParamsFromOpenURLParams(&nav_params, params);
  nav_params.source_contents = source;
  nav_params.tabstrip_add_types = TabStripModel::ADD_NONE;
  if (params.user_gesture)
    nav_params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  nav_params.user_gesture = params.user_gesture;

  PopupBlockerTabHelper* popup_blocker_helper = NULL;
  if (source)
    popup_blocker_helper = PopupBlockerTabHelper::FromWebContents(source);

  if (popup_blocker_helper) {
    if ((params.disposition == NEW_POPUP ||
         params.disposition == NEW_FOREGROUND_TAB ||
         params.disposition == NEW_BACKGROUND_TAB ||
         params.disposition == NEW_WINDOW) &&
        !params.user_gesture &&
        !base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisablePopupBlocking)) {
      if (popup_blocker_helper->MaybeBlockPopup(nav_params,
                                                WebWindowFeatures())) {
        return NULL;
      }
    }
  }

  chrome::Navigate(&nav_params);

  return nav_params.target_contents;
}

void Browser::NavigationStateChanged(WebContents* source,
                                     content::InvalidateTypes changed_flags) {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466285
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466285 Browser::NavigationStateChanged::ScheduleUIUpdate"));
  // Only update the UI when something visible has changed.
  if (changed_flags)
    ScheduleUIUpdate(source, changed_flags);

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466285
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466285 Browser::NavigationStateChanged::TabStateChanged"));
  // We can synchronously update commands since they will only change once per
  // navigation, so we don't have to worry about flickering. We do, however,
  // need to update the command state early on load to always present usable
  // actions in the face of slow-to-commit pages.
  if (changed_flags & (content::INVALIDATE_TYPE_URL |
                       content::INVALIDATE_TYPE_LOAD))
    command_controller_->TabStateChanged();

  if (hosted_app_controller_)
    hosted_app_controller_->UpdateLocationBarVisibility(true);
}

void Browser::VisibleSSLStateChanged(const WebContents* source) {
  // When the current tab's SSL state changes, we need to update the URL
  // bar to reflect the new state.
  DCHECK(source);
  if (tab_strip_model_->GetActiveWebContents() == source)
    UpdateToolbar(false);
}

void Browser::AddNewContents(WebContents* source,
                             WebContents* new_contents,
                             WindowOpenDisposition disposition,
                             const gfx::Rect& initial_rect,
                             bool user_gesture,
                             bool* was_blocked) {
  chrome::AddWebContents(this, source, new_contents, disposition, initial_rect,
                         user_gesture, was_blocked);
}

void Browser::ActivateContents(WebContents* contents) {
  tab_strip_model_->ActivateTabAt(
      tab_strip_model_->GetIndexOfWebContents(contents), false);
  window_->Activate();
}

void Browser::LoadingStateChanged(WebContents* source,
    bool to_different_document) {
  window_->UpdateLoadingAnimations(tab_strip_model_->TabsAreLoading());
  window_->UpdateTitleBar();

  WebContents* selected_contents = tab_strip_model_->GetActiveWebContents();
  if (source == selected_contents) {
    bool is_loading = source->IsLoading() && to_different_document;
    command_controller_->LoadingStateChanged(is_loading, false);
    if (GetStatusBubble()) {
      GetStatusBubble()->SetStatus(CoreTabHelper::FromWebContents(
          tab_strip_model_->GetActiveWebContents())->GetStatusText());
    }
  }
}

void Browser::CloseContents(WebContents* source) {
  bool can_close_contents;
  if (IsFastTabUnloadEnabled())
    can_close_contents = fast_unload_controller_->CanCloseContents(source);
  else
    can_close_contents = unload_controller_->CanCloseContents(source);

  if (can_close_contents)
    chrome::CloseWebContents(this, source, true);
}

void Browser::MoveContents(WebContents* source, const gfx::Rect& pos) {
  if (!IsPopupOrPanel(source)) {
    NOTREACHED() << "moving invalid browser type";
    return;
  }
  window_->SetBounds(pos);
}

bool Browser::IsPopupOrPanel(const WebContents* source) const {
  return is_type_popup();
}

void Browser::UpdateTargetURL(WebContents* source, const GURL& url) {
  if (!GetStatusBubble())
    return;

  if (source == tab_strip_model_->GetActiveWebContents()) {
    PrefService* prefs = profile_->GetPrefs();
    GetStatusBubble()->SetURL(url, prefs->GetString(prefs::kAcceptLanguages));
  }
}

void Browser::ContentsMouseEvent(WebContents* source,
                                 const gfx::Point& location,
                                 bool motion,
                                 bool exited) {
  // Mouse motion events update the status bubble, if it exists.
  if (!GetStatusBubble() || (!motion && !exited))
    return;

  if (source == tab_strip_model_->GetActiveWebContents()) {
    GetStatusBubble()->MouseMoved(location, exited);
    if (exited)
      GetStatusBubble()->SetURL(GURL(), std::string());
  }
}

void Browser::ContentsZoomChange(bool zoom_in) {
  chrome::ExecuteCommand(this, zoom_in ? IDC_ZOOM_PLUS : IDC_ZOOM_MINUS);
}

bool Browser::TakeFocus(content::WebContents* source,
                        bool reverse) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FOCUS_RETURNED_TO_BROWSER,
      content::Source<Browser>(this),
      content::NotificationService::NoDetails());
  return false;
}

gfx::Rect Browser::GetRootWindowResizerRect() const {
  return window_->GetRootWindowResizerRect();
}

void Browser::BeforeUnloadFired(WebContents* web_contents,
                                bool proceed,
                                bool* proceed_to_fire_unload) {
  if (is_devtools() && DevToolsWindow::HandleBeforeUnload(web_contents,
        proceed, proceed_to_fire_unload))
    return;

  if (IsFastTabUnloadEnabled()) {
    *proceed_to_fire_unload =
        fast_unload_controller_->BeforeUnloadFired(web_contents, proceed);
  } else {
    *proceed_to_fire_unload =
        unload_controller_->BeforeUnloadFired(web_contents, proceed);
  }
}

bool Browser::ShouldFocusLocationBarByDefault(WebContents* source) {
  const content::NavigationEntry* entry =
      source->GetController().GetActiveEntry();
  if (entry) {
    GURL url = entry->GetURL();
    GURL virtual_url = entry->GetVirtualURL();
    if ((url.SchemeIs(content::kChromeUIScheme) &&
        url.host() == chrome::kChromeUINewTabHost) ||
        (virtual_url.SchemeIs(content::kChromeUIScheme) &&
        virtual_url.host() == chrome::kChromeUINewTabHost)) {
      return true;
    }
  }

  return search::NavEntryIsInstantNTP(source, entry);
}

void Browser::ViewSourceForTab(WebContents* source, const GURL& page_url) {
  DCHECK(source);
  chrome::ViewSource(this, source);
}

void Browser::ViewSourceForFrame(WebContents* source,
                                 const GURL& frame_url,
                                 const content::PageState& frame_page_state) {
  DCHECK(source);
  chrome::ViewSource(this, source, frame_url, frame_page_state);
}

void Browser::ShowRepostFormWarningDialog(WebContents* source) {
  TabModalConfirmDialog::Create(new RepostFormWarningController(source),
                                source);
}

bool Browser::ShouldCreateWebContents(
    WebContents* web_contents,
    int32_t route_id,
    int32_t main_frame_route_id,
    int32_t main_frame_widget_route_id,
    WindowContainerType window_container_type,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  if (window_container_type == WINDOW_CONTAINER_TYPE_BACKGROUND) {
    // If a BackgroundContents is created, suppress the normal WebContents.
    return !MaybeCreateBackgroundContents(
        route_id, main_frame_route_id, main_frame_widget_route_id, web_contents,
        frame_name, target_url, partition_id, session_storage_namespace);
  }

  return true;
}

void Browser::WebContentsCreated(WebContents* source_contents,
                                 int opener_render_frame_id,
                                 const std::string& frame_name,
                                 const GURL& target_url,
                                 WebContents* new_contents) {
  // Adopt the WebContents now, so all observers are in place, as the network
  // requests for its initial navigation will start immediately. The WebContents
  // will later be inserted into this browser using Browser::Navigate via
  // AddNewContents.
  TabHelpers::AttachTabHelpers(new_contents);

  // Make the tab show up in the task manager.
  task_management::WebContentsTags::CreateForTabContents(new_contents);

  // Notify.
  RetargetingDetails details;
  details.source_web_contents = source_contents;
  details.source_render_frame_id = opener_render_frame_id;
  details.target_url = target_url;
  details.target_web_contents = new_contents;
  details.not_yet_in_tabstrip = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_RETARGETING,
      content::Source<Profile>(profile_),
      content::Details<RetargetingDetails>(&details));
}

void Browser::RendererUnresponsive(WebContents* source) {
  // Ignore hangs if a tab is blocked.
  int index = tab_strip_model_->GetIndexOfWebContents(source);
  DCHECK_NE(TabStripModel::kNoTab, index);
  if (tab_strip_model_->IsTabBlocked(index))
    return;

  TabDialogs::FromWebContents(source)->ShowHungRendererDialog();
}

void Browser::RendererResponsive(WebContents* source) {
  TabDialogs::FromWebContents(source)->HideHungRendererDialog();
}

void Browser::DidNavigateMainFramePostCommit(WebContents* web_contents) {
  if (web_contents == tab_strip_model_->GetActiveWebContents())
    UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
}

content::JavaScriptDialogManager* Browser::GetJavaScriptDialogManager(
    WebContents* source) {
  return app_modal::JavaScriptDialogManager::GetInstance();
}

content::ColorChooser* Browser::OpenColorChooser(
      WebContents* web_contents,
      SkColor initial_color,
      const std::vector<content::ColorSuggestion>& suggestions) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void Browser::RunFileChooser(WebContents* web_contents,
                             const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(web_contents, params);
}

void Browser::EnumerateDirectory(WebContents* web_contents,
                                 int request_id,
                                 const base::FilePath& path) {
  FileSelectHelper::EnumerateDirectory(web_contents, request_id, path);
}

bool Browser::EmbedsFullscreenWidget() const {
  return true;
}

void Browser::EnterFullscreenModeForTab(WebContents* web_contents,
                                        const GURL& origin) {
  exclusive_access_manager_->fullscreen_controller()->EnterFullscreenModeForTab(
      web_contents, origin);
}

void Browser::ExitFullscreenModeForTab(WebContents* web_contents) {
  exclusive_access_manager_->fullscreen_controller()->ExitFullscreenModeForTab(
      web_contents);
}

bool Browser::IsFullscreenForTabOrPending(
    const WebContents* web_contents) const {
  return exclusive_access_manager_->fullscreen_controller()
      ->IsFullscreenForTabOrPending(web_contents);
}

blink::WebDisplayMode Browser::GetDisplayMode(
    const WebContents* web_contents) const {
  if (window_->IsFullscreen())
    return blink::WebDisplayModeFullscreen;

  if (is_type_popup())
    return blink::WebDisplayModeStandalone;

  return blink::WebDisplayModeBrowser;
}

void Browser::RegisterProtocolHandler(WebContents* web_contents,
                                      const std::string& protocol,
                                      const GURL& url,
                                      bool user_gesture) {
  content::BrowserContext* context = web_contents->GetBrowserContext();
  if (context->IsOffTheRecord())
    return;

  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(protocol, url);

  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(context);
  if (registry->SilentlyHandleRegisterHandlerRequest(handler))
    return;

  TabSpecificContentSettings* tab_content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  if (!user_gesture && window_) {
    tab_content_settings->set_pending_protocol_handler(handler);
    tab_content_settings->set_previous_protocol_handler(
        registry->GetHandlerFor(handler.protocol()));
    window_->GetLocationBar()->UpdateContentSettingsIcons();
    return;
  }

  // Make sure content-setting icon is turned off in case the page does
  // ungestured and gestured RPH calls.
  if (window_) {
    tab_content_settings->ClearPendingProtocolHandler();
    window_->GetLocationBar()->UpdateContentSettingsIcons();
  }

  PermissionBubbleManager* bubble_manager =
      PermissionBubbleManager::FromWebContents(web_contents);
  if (bubble_manager) {
    bubble_manager->AddRequest(
        new RegisterProtocolHandlerPermissionRequest(registry, handler,
                                                     url, user_gesture));
  }
}

void Browser::UnregisterProtocolHandler(WebContents* web_contents,
                                        const std::string& protocol,
                                        const GURL& url,
                                        bool user_gesture) {
  // user_gesture will be used in case we decide to have confirmation bubble
  // for user while un-registering the handler.
  content::BrowserContext* context = web_contents->GetBrowserContext();
  if (context->IsOffTheRecord())
    return;

  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(protocol, url);

  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(context);
  registry->RemoveHandler(handler);
}

void Browser::UpdatePreferredSize(WebContents* source,
                                  const gfx::Size& pref_size) {
  window_->UpdatePreferredSize(source, pref_size);
}

void Browser::ResizeDueToAutoResize(WebContents* source,
                                    const gfx::Size& new_size) {
  window_->ResizeDueToAutoResize(source, new_size);
}

void Browser::FindReply(WebContents* web_contents,
                        int request_id,
                        int number_of_matches,
                        const gfx::Rect& selection_rect,
                        int active_match_ordinal,
                        bool final_update) {
  FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(web_contents);
  if (!find_tab_helper)
    return;

  find_tab_helper->HandleFindReply(request_id,
                                   number_of_matches,
                                   selection_rect,
                                   active_match_ordinal,
                                   final_update);
}

void Browser::RequestToLockMouse(WebContents* web_contents,
                                 bool user_gesture,
                                 bool last_unlocked_by_target) {
  exclusive_access_manager_->mouse_lock_controller()->RequestToLockMouse(
      web_contents, user_gesture, last_unlocked_by_target);
}

void Browser::LostMouseLock() {
  exclusive_access_manager_->mouse_lock_controller()->LostMouseLock();
}

void Browser::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  ::RequestMediaAccessPermission(web_contents, profile_, request, callback);
}

bool Browser::CheckMediaAccessPermission(content::WebContents* web_contents,
                                         const GURL& security_origin,
                                         content::MediaStreamType type) {
  return ::CheckMediaAccessPermission(web_contents, security_origin, type);
}

bool Browser::RequestPpapiBrokerPermission(
    WebContents* web_contents,
    const GURL& url,
    const base::FilePath& plugin_path,
    const base::Callback<void(bool)>& callback) {
  PepperBrokerInfoBarDelegate::Create(web_contents, url, plugin_path, callback);
  return true;
}

gfx::Size Browser::GetSizeForNewRenderView(WebContents* web_contents) const {
  // When navigating away from NTP with unpinned bookmark bar, the bookmark bar
  // would disappear on non-NTP pages, resulting in a bigger size for the new
  // render view.
  gfx::Size size = web_contents->GetContainerBounds().size();
  // Don't change render view size if bookmark bar is currently not detached,
  // or there's no pending entry, or navigating to a NTP page.
  if (size.IsEmpty() || bookmark_bar_state_ != BookmarkBar::DETACHED)
    return size;
  const NavigationEntry* pending_entry =
      web_contents->GetController().GetPendingEntry();
  if (pending_entry &&
      !search::IsNTPURL(pending_entry->GetVirtualURL(), profile_)) {
    size.Enlarge(
        0, window()->GetRenderViewHeightInsetWithDetachedBookmarkBar());
  }
  return size;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, CoreTabHelperDelegate implementation:

void Browser::SwapTabContents(content::WebContents* old_contents,
                              content::WebContents* new_contents,
                              bool did_start_load,
                              bool did_finish_load) {
  int index = tab_strip_model_->GetIndexOfWebContents(old_contents);
  DCHECK_NE(TabStripModel::kNoTab, index);
  tab_strip_model_->ReplaceWebContentsAt(index, new_contents);
}

bool Browser::CanReloadContents(content::WebContents* web_contents) const {
  return chrome::CanReload(this);
}

bool Browser::CanSaveContents(content::WebContents* web_contents) const {
  return chrome::CanSavePage(this);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, SearchEngineTabHelperDelegate implementation:

void Browser::ConfirmAddSearchProvider(TemplateURL* template_url,
                                       Profile* profile) {
  window()->ConfirmAddSearchProvider(template_url, profile);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, SearchTabHelperDelegate implementation:

void Browser::NavigateOnThumbnailClick(const GURL& url,
                                       WindowOpenDisposition disposition,
                                       content::WebContents* source_contents) {
  DCHECK(source_contents);
  // We're guaranteed that AUTO_BOOKMARK is the right transition since this only
  // gets called to handle clicks in the new tab page (to navigate to most
  // visited item URLs) and in the search results page (to navigate to
  // privileged destinations (e.g. chrome://URLs)).
  //
  // TODO(kmadhusu): Page transitions to privileged destinations should be
  // marked as "LINK" instead of "AUTO_BOOKMARK"?
  chrome::NavigateParams params(this, url,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.referrer = content::Referrer();
  params.source_contents = source_contents;
  params.disposition = disposition;
  params.is_renderer_initiated = false;
  params.initiating_profile = profile_;
  chrome::Navigate(&params);
}

void Browser::OnWebContentsInstantSupportDisabled(
    const content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (tab_strip_model_->GetActiveWebContents() == web_contents)
    UpdateToolbar(false);
}

OmniboxView* Browser::GetOmniboxView() {
  return window_->GetLocationBar()->GetOmniboxView();
}

std::set<std::string> Browser::GetOpenUrls() {
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (!top_sites)  // NULL for Incognito profiles.
    return std::set<std::string>();

  std::set<std::string> open_urls;
  chrome::GetOpenUrls(*tab_strip_model_, *top_sites, &open_urls);
  return open_urls;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, web_modal::WebContentsModalDialogManagerDelegate implementation:

void Browser::SetWebContentsBlocked(content::WebContents* web_contents,
                                    bool blocked) {
  int index = tab_strip_model_->GetIndexOfWebContents(web_contents);
  if (index == TabStripModel::kNoTab) {
    NOTREACHED();
    return;
  }
  tab_strip_model_->SetTabBlocked(index, blocked);
  if (!blocked && tab_strip_model_->GetActiveWebContents() == web_contents)
    web_contents->Focus();
}

web_modal::WebContentsModalDialogHost*
Browser::GetWebContentsModalDialogHost() {
  return window_->GetWebContentsModalDialogHost();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, BookmarkTabHelperDelegate implementation:

void Browser::URLStarredChanged(content::WebContents* web_contents,
                                bool starred) {
  if (web_contents == tab_strip_model_->GetActiveWebContents())
    window_->SetStarredState(starred);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ZoomObserver implementation:

void Browser::OnZoomChanged(
    const ui_zoom::ZoomController::ZoomChangedEventData& data) {
  if (data.web_contents == tab_strip_model_->GetActiveWebContents()) {
    window_->ZoomChangedForActiveTab(data.can_show_bubble);
    // Change the zoom commands state based on the zoom state
    command_controller_->ZoomStateChanged();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ui::SelectFileDialog::Listener implementation:

void Browser::FileSelected(const base::FilePath& path, int index,
                           void* params) {
  FileSelectedWithExtraInfo(ui::SelectedFileInfo(path, path), index, params);
}

void Browser::FileSelectedWithExtraInfo(const ui::SelectedFileInfo& file_info,
                                        int index,
                                        void* params) {
  profile_->set_last_selected_directory(file_info.file_path.DirName());

  GURL url = net::FilePathToFileURL(file_info.local_path);

#if defined(OS_CHROMEOS)
  const GURL external_url =
      chromeos::CreateExternalFileURLFromPath(profile_, file_info.file_path);
  if (!external_url.is_empty())
    url = external_url;
#endif

  if (url.is_empty())
    return;

  OpenURL(OpenURLParams(
      url, Referrer(), CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));
}

///////////////////////////////////////////////////////////////////////////////
// Browser, content::NotificationObserver implementation:

void Browser::Observe(int type,
                      const content::NotificationSource& source,
                      const content::NotificationDetails& details) {
  switch (type) {
    case extensions::NOTIFICATION_EXTENSION_PROCESS_TERMINATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile_->IsSameProfile(profile) && window()->GetLocationBar())
        window()->GetLocationBar()->UpdatePageActions();
      break;
    }

#if defined(ENABLE_THEMES)
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      window()->UserChangedTheme();
      break;
#endif

    case chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      if (web_contents == tab_strip_model_->GetActiveWebContents()) {
        LocationBar* location_bar = window()->GetLocationBar();
        if (location_bar)
          location_bar->UpdateContentSettingsIcons();
      }
      break;
    }

    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

#if defined(ENABLE_EXTENSIONS)
///////////////////////////////////////////////////////////////////////////////
// Browser, extensions::ExtensionRegistryObserver implementation:

void Browser::OnExtensionUninstalled(content::BrowserContext* browser_context,
                                     const extensions::Extension* extension,
                                     extensions::UninstallReason reason) {
  // During window creation on Windows we may end up calling into
  // SHAppBarMessage, which internally spawns a nested message loop.This
  // makes it possible for us to end up here before window creation has
  // completed, at which point window_ is NULL. See 94752 for details.

  if (window() && window()->GetLocationBar())
    window()->GetLocationBar()->UpdatePageActions();
}

void Browser::OnExtensionLoaded(content::BrowserContext* browser_context,
                                const extensions::Extension* extension) {
  command_controller_->ExtensionStateChanged();
}

void Browser::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  command_controller_->ExtensionStateChanged();
  if (window()->GetLocationBar())
    window()->GetLocationBar()->UpdatePageActions();

  // Close any tabs from the unloaded extension, unless it's terminated,
  // in which case let the sad tabs remain.
  // Also, if tab is muted and the cause is the unloaded extension, unmute it.
  if (reason != extensions::UnloadedExtensionInfo::REASON_TERMINATE) {
    // Iterate backwards as we may remove items while iterating.
    for (int i = tab_strip_model_->count() - 1; i >= 0; --i) {
      WebContents* web_contents = tab_strip_model_->GetWebContentsAt(i);
      // Two cases are handled here:

      // - The scheme check is for when an extension page is loaded in a
      // tab, e.g. chrome-extension://id/page.html.
      // - The extension_app check is for apps, which can have non-extension
      // schemes, e.g. https://mail.google.com if you have the Gmail app
      // installed.
      if ((web_contents->GetURL().SchemeIs(extensions::kExtensionScheme) &&
           web_contents->GetURL().host() == extension->id()) ||
          (extensions::TabHelper::FromWebContents(web_contents)
               ->extension_app() == extension)) {
        tab_strip_model_->CloseWebContentsAt(i, TabStripModel::CLOSE_NONE);
      } else {
        chrome::UnmuteIfMutedByExtension(web_contents, extension->id());
      }
    }
  }
}
#endif  // defined(ENABLE_EXTENSIONS)

///////////////////////////////////////////////////////////////////////////////
// Browser, translate::ContentTranslateDriver::Observer implementation:

void Browser::OnIsPageTranslatedChanged(content::WebContents* source) {
  DCHECK(source);
  if (tab_strip_model_->GetActiveWebContents() == source) {
    window_->SetTranslateIconToggled(
        ChromeTranslateClient::FromWebContents(
            source)->GetLanguageState().IsPageTranslated());
  }
}

void Browser::OnTranslateEnabledChanged(content::WebContents* source) {
  DCHECK(source);
  if (tab_strip_model_->GetActiveWebContents() == source)
    UpdateToolbar(false);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Command and state updating (private):

void Browser::OnDevToolsDisabledChanged() {
  if (profile_->GetPrefs()->GetBoolean(prefs::kDevToolsDisabled))
    content::DevToolsAgentHost::DetachAllClients();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, UI update coalescing and handling (private):

void Browser::UpdateToolbar(bool should_restore_state) {
  window_->UpdateToolbar(should_restore_state ?
      tab_strip_model_->GetActiveWebContents() : NULL);
}

void Browser::ScheduleUIUpdate(WebContents* source,
                               unsigned changed_flags) {
  DCHECK(source);
  int index = tab_strip_model_->GetIndexOfWebContents(source);
  DCHECK_NE(TabStripModel::kNoTab, index);

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466285
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466285 Browser::ScheduleUIUpdate::Toolbar"));
  // Do some synchronous updates.
  if (changed_flags & content::INVALIDATE_TYPE_URL) {
    if (source == tab_strip_model_->GetActiveWebContents()) {
      // Only update the URL for the current tab. Note that we do not update
      // the navigation commands since those would have already been updated
      // synchronously by NavigationStateChanged.
      UpdateToolbar(false);
    } else {
      // Clear the saved tab state for the tab that navigated, so that we don't
      // restore any user text after the old URL has been invalidated (e.g.,
      // after a new navigation commits in that tab while unfocused).
      window_->ResetToolbarTabState(source);
    }
    changed_flags &= ~content::INVALIDATE_TYPE_URL;
  }

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466285
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466285 Browser::ScheduleUIUpdate::TabStripModel"));
  if (changed_flags & content::INVALIDATE_TYPE_LOAD) {
    // Update the loading state synchronously. This is so the throbber will
    // immediately start/stop, which gives a more snappy feel. We want to do
    // this for any tab so they start & stop quickly.
    tab_strip_model_->UpdateWebContentsStateAt(
        tab_strip_model_->GetIndexOfWebContents(source),
        TabStripModelObserver::LOADING_ONLY);
    // The status bubble needs to be updated during INVALIDATE_TYPE_LOAD too,
    // but we do that asynchronously by not stripping INVALIDATE_TYPE_LOAD from
    // changed_flags.
  }

  if (changed_flags & content::INVALIDATE_TYPE_TITLE && !source->IsLoading()) {
    // To correctly calculate whether the title changed while not loading
    // we need to process the update synchronously. This state only matters for
    // the TabStripModel, so we notify the TabStripModel now and notify others
    // asynchronously.
    tab_strip_model_->UpdateWebContentsStateAt(
        tab_strip_model_->GetIndexOfWebContents(source),
        TabStripModelObserver::TITLE_NOT_LOADING);
  }

  // If the only updates were synchronously handled above, we're done.
  if (changed_flags == 0)
    return;

  // Save the dirty bits.
  scheduled_updates_[source] |= changed_flags;

  if (!chrome_updater_factory_.HasWeakPtrs()) {
    // No task currently scheduled, start another.
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&Browser::ProcessPendingUIUpdates,
                              chrome_updater_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kUIUpdateCoalescingTimeMS));
  }
}

void Browser::ProcessPendingUIUpdates() {
#ifndef NDEBUG
  // Validate that all tabs we have pending updates for exist. This is scary
  // because the pending list must be kept in sync with any detached or
  // deleted tabs.
  for (UpdateMap::const_iterator i = scheduled_updates_.begin();
       i != scheduled_updates_.end(); ++i) {
    bool found = false;
    for (int tab = 0; tab < tab_strip_model_->count(); tab++) {
      if (tab_strip_model_->GetWebContentsAt(tab) == i->first) {
        found = true;
        break;
      }
    }
    DCHECK(found);
  }
#endif

  chrome_updater_factory_.InvalidateWeakPtrs();

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "467185 Browser::ProcessPendingUIUpdates1"));

  for (UpdateMap::const_iterator i = scheduled_updates_.begin();
       i != scheduled_updates_.end(); ++i) {
    // Do not dereference |contents|, it may be out-of-date!
    const WebContents* contents = i->first;
    unsigned flags = i->second;

    if (contents == tab_strip_model_->GetActiveWebContents()) {
      // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185
      // is fixed.
      tracked_objects::ScopedTracker tracking_profile2(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "467185 Browser::ProcessPendingUIUpdates2"));

      // Updates that only matter when the tab is selected go here.

      // Updating the URL happens synchronously in ScheduleUIUpdate.
      if (flags & content::INVALIDATE_TYPE_LOAD && GetStatusBubble()) {
        GetStatusBubble()->SetStatus(CoreTabHelper::FromWebContents(
            tab_strip_model_->GetActiveWebContents())->GetStatusText());
      }

      if (flags & (content::INVALIDATE_TYPE_TAB |
                   content::INVALIDATE_TYPE_TITLE)) {
        window_->UpdateTitleBar();
      }
    }

    // Updates that don't depend upon the selected state go here.
    if (flags &
        (content::INVALIDATE_TYPE_TAB | content::INVALIDATE_TYPE_TITLE)) {
      // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185
      // is fixed.
      tracked_objects::ScopedTracker tracking_profile3(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "467185 Browser::ProcessPendingUIUpdates3"));
      tab_strip_model_->UpdateWebContentsStateAt(
          tab_strip_model_->GetIndexOfWebContents(contents),
          TabStripModelObserver::ALL);
    }

    // Update the bookmark bar. It may happen that the tab is crashed, and if
    // so, the bookmark bar should be hidden.
    if (flags & content::INVALIDATE_TYPE_TAB) {
      // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185
      // is fixed.
      tracked_objects::ScopedTracker tracking_profile4(
          FROM_HERE_WITH_EXPLICIT_FUNCTION(
              "467185 Browser::ProcessPendingUIUpdates4"));
      UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
    }

    // We don't need to process INVALIDATE_STATE, since that's not visible.
  }

  scheduled_updates_.clear();
}

void Browser::RemoveScheduledUpdatesFor(WebContents* contents) {
  if (!contents)
    return;

  UpdateMap::iterator i = scheduled_updates_.find(contents);
  if (i != scheduled_updates_.end())
    scheduled_updates_.erase(i);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Getters for UI (private):

StatusBubble* Browser::GetStatusBubble() {
  // In kiosk and exclusive app mode, we want to always hide the status bubble.
  if (chrome::IsRunningInAppMode())
    return NULL;

  return window_ ? window_->GetStatusBubble() : NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Session restore functions (private):

void Browser::SyncHistoryWithTabs(int index) {
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile());
  if (session_service) {
    for (int i = index; i < tab_strip_model_->count(); ++i) {
      WebContents* web_contents = tab_strip_model_->GetWebContentsAt(i);
      if (web_contents) {
        SessionTabHelper* session_tab_helper =
            SessionTabHelper::FromWebContents(web_contents);
        session_service->SetTabIndexInWindow(
            session_id(), session_tab_helper->session_id(), i);
        session_service->SetPinnedState(
            session_id(),
            session_tab_helper->session_id(),
            tab_strip_model_->IsTabPinned(i));
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, In-progress download termination handling (private):

bool Browser::CanCloseWithInProgressDownloads() {
  // If we've prompted, we need to hear from the user before we
  // can close.
  if (cancel_download_confirmation_state_ != NOT_PROMPTED)
    return cancel_download_confirmation_state_ != WAITING_FOR_RESPONSE;

  int num_downloads_blocking;
  Browser::DownloadClosePreventionType dialog_type =
      OkToCloseWithInProgressDownloads(&num_downloads_blocking);
  if (dialog_type == DOWNLOAD_CLOSE_OK)
    return true;

  // Closing this window will kill some downloads; prompt to make sure
  // that's ok.
  cancel_download_confirmation_state_ = WAITING_FOR_RESPONSE;
  window_->ConfirmBrowserCloseWithPendingDownloads(
      num_downloads_blocking,
      dialog_type,
      false,
      base::Bind(&Browser::InProgressDownloadResponse,
                 weak_factory_.GetWeakPtr()));

  // Return false so the browser does not close.  We'll close if the user
  // confirms in the dialog.
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted utility functions (private):

void Browser::SetAsDelegate(WebContents* web_contents, bool set_delegate) {
  Browser* delegate = set_delegate ? this : NULL;
  // WebContents...
  web_contents->SetDelegate(delegate);

  // ...and all the helpers.
  BookmarkTabHelper::FromWebContents(web_contents)->set_delegate(delegate);
  WebContentsModalDialogManager::FromWebContents(web_contents)->
      SetDelegate(delegate);
  CoreTabHelper::FromWebContents(web_contents)->set_delegate(delegate);
  SearchEngineTabHelper::FromWebContents(web_contents)->set_delegate(delegate);
  SearchTabHelper::FromWebContents(web_contents)->set_delegate(delegate);
  translate::ContentTranslateDriver& content_translate_driver =
      ChromeTranslateClient::FromWebContents(web_contents)->translate_driver();
  if (delegate) {
    ui_zoom::ZoomController::FromWebContents(web_contents)->AddObserver(this);
    content_translate_driver.AddObserver(this);
  } else {
    ui_zoom::ZoomController::FromWebContents(web_contents)->RemoveObserver(
        this);
    content_translate_driver.RemoveObserver(this);
  }
}

void Browser::CloseFrame() {
  window_->Close();
}

void Browser::TabDetachedAtImpl(content::WebContents* contents,
                                int index,
                                DetachType type) {
  if (type == DETACH_TYPE_DETACH) {
    // Save the current location bar state, but only if the tab being detached
    // is the selected tab.  Because saving state can conditionally revert the
    // location bar, saving the current tab's location bar state to a
    // non-selected tab can corrupt both tabs.
    if (contents == tab_strip_model_->GetActiveWebContents()) {
      LocationBar* location_bar = window()->GetLocationBar();
      if (location_bar)
        location_bar->SaveStateToContents(contents);
    }

    if (!tab_strip_model_->closing_all())
      SyncHistoryWithTabs(0);
  }

  SetAsDelegate(contents, false);
  RemoveScheduledUpdatesFor(contents);

  if (find_bar_controller_.get() && index == tab_strip_model_->active_index()) {
    find_bar_controller_->ChangeWebContents(NULL);
  }

  // Stop observing search model changes for this tab.
  search_delegate_->OnTabDetached(contents);

  for (size_t i = 0; i < interstitial_observers_.size(); i++) {
    if (interstitial_observers_[i]->web_contents() != contents)
      continue;

    delete interstitial_observers_[i];
    interstitial_observers_.erase(interstitial_observers_.begin() + i);
    return;
  }
}

bool Browser::SupportsLocationBar() const {
  // Tabbed browser always show a location bar.
  if (is_type_tabbed())
    return true;

  // Non-app windows that aren't tabbed or system windows should always show a
  // location bar, unless they are from a trusted source.
  if (!is_app())
    return !is_trusted_source();

  if (hosted_app_controller_)
    return hosted_app_controller_->SupportsLocationBar();

  return false;
}

bool Browser::ShouldUseWebAppFrame() const {
  // Only use the web app frame for apps in ash, and only if the web app frame
  // is enabled.
  if (!is_app())
    return false;

  if (hosted_app_controller_)
    return hosted_app_controller_->should_use_web_app_frame();

  return false;
}

bool Browser::SupportsWindowFeatureImpl(WindowFeature feature,
                                        bool check_fullscreen) const {
  bool hide_ui_for_fullscreen = check_fullscreen && ShouldHideUIForFullscreen();

  unsigned int features = FEATURE_INFOBAR | FEATURE_DOWNLOADSHELF;

  if (is_type_tabbed())
    features |= FEATURE_BOOKMARKBAR;

  if (!hide_ui_for_fullscreen) {
    if (!is_type_tabbed())
      features |= FEATURE_TITLEBAR;

    if (is_type_tabbed())
      features |= FEATURE_TABSTRIP;

    if (is_type_tabbed())
      features |= FEATURE_TOOLBAR;

    if (SupportsLocationBar())
      features |= FEATURE_LOCATIONBAR;

    if (ShouldUseWebAppFrame())
      features |= FEATURE_WEBAPPFRAME;
  }
  return !!(features & feature);
}

void Browser::UpdateBookmarkBarState(BookmarkBarStateChangeReason reason) {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "467185 Browser::UpdateBookmarkBarState1"));
  BookmarkBar::State state;
  // The bookmark bar is always hidden for Guest Sessions and in fullscreen
  // mode, unless on the new tab page.
  if (profile_->IsGuestSession()) {
    state = BookmarkBar::HIDDEN;
  } else if (browser_defaults::bookmarks_enabled &&
      profile_->GetPrefs()->GetBoolean(bookmarks::prefs::kShowBookmarkBar) &&
      !ShouldHideUIForFullscreen()) {
    state = BookmarkBar::SHOW;
  } else {
    // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile2(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "467185 Browser::UpdateBookmarkBarState2"));
    WebContents* web_contents = tab_strip_model_->GetActiveWebContents();
    BookmarkTabHelper* bookmark_tab_helper =
        web_contents ? BookmarkTabHelper::FromWebContents(web_contents) : NULL;
    if (bookmark_tab_helper && bookmark_tab_helper->ShouldShowBookmarkBar())
      state = BookmarkBar::DETACHED;
    else
      state = BookmarkBar::HIDDEN;
  }

  if (state == bookmark_bar_state_)
    return;

  bookmark_bar_state_ = state;

  if (!window_)
    return;  // This is called from the constructor when window_ is NULL.

  if (reason == BOOKMARK_BAR_STATE_CHANGE_TAB_SWITCH) {
    // Don't notify BrowserWindow on a tab switch as at the time this is invoked
    // BrowserWindow hasn't yet switched tabs. The BrowserWindow implementations
    // end up querying state once they process the tab switch.
    return;
  }

  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/467185 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "467185 Browser::UpdateBookmarkBarState3"));

  bool should_animate = reason == BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE;
  window_->BookmarkBarStateChanged(should_animate ?
      BookmarkBar::ANIMATE_STATE_CHANGE :
      BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
}

bool Browser::ShouldHideUIForFullscreen() const {
  // Windows and GTK remove the top controls in fullscreen, but Mac and Ash
  // keep the controls in a slide-down panel.
  return window_ && window_->ShouldHideUIForFullscreen();
}

bool Browser::ShouldStartShutdown() const {
  return BrowserList::GetInstance(host_desktop_type())->size() <= 1;
}

bool Browser::MaybeCreateBackgroundContents(
    int32_t route_id,
    int32_t main_frame_route_id,
    int32_t main_frame_widget_route_id,
    WebContents* opener_web_contents,
    const std::string& frame_name,
    const GURL& target_url,
    const std::string& partition_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  GURL opener_url = opener_web_contents->GetURL();
  ExtensionService* extensions_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();

  if (!opener_url.is_valid() ||
      frame_name.empty() ||
      !extensions_service ||
      !extensions_service->is_ready())
    return false;

  // Only hosted apps have web extents, so this ensures that only hosted apps
  // can create BackgroundContents. We don't have to check for background
  // permission as that is checked in RenderMessageFilter when the CreateWindow
  // message is processed.
  const Extension* extension = extensions::ExtensionRegistry::Get(profile_)
                                   ->enabled_extensions()
                                   .GetHostedAppByURL(opener_url);
  if (!extension)
    return false;

  // No BackgroundContents allowed if BackgroundContentsService doesn't exist.
  BackgroundContentsService* service =
      BackgroundContentsServiceFactory::GetForProfile(profile_);
  if (!service)
    return false;

  // Ensure that we're trying to open this from the extension's process.
  SiteInstance* opener_site_instance = opener_web_contents->GetSiteInstance();
  extensions::ProcessMap* process_map = extensions::ProcessMap::Get(profile_);
  if (!opener_site_instance->GetProcess() ||
      !process_map->Contains(
          extension->id(), opener_site_instance->GetProcess()->GetID())) {
    return false;
  }

  // Only allow a single background contents per app.
  bool allow_js_access = extensions::BackgroundInfo::AllowJSAccess(extension);
  BackgroundContents* existing =
      service->GetAppBackgroundContents(base::ASCIIToUTF16(extension->id()));
  if (existing) {
    // For non-scriptable background contents, ignore the request altogether,
    // (returning true, so that a regular WebContents isn't created either).
    if (!allow_js_access)
      return true;
    // For scriptable background pages, if one already exists, close it (even
    // if it was specified in the manifest).
    delete existing;
  }

  // If script access is not allowed, create the the background contents in a
  // new SiteInstance, so that a separate process is used.
  scoped_refptr<content::SiteInstance> site_instance =
      allow_js_access ?
      opener_site_instance :
      content::SiteInstance::Create(opener_web_contents->GetBrowserContext());

  // Passed all the checks, so this should be created as a BackgroundContents.
  BackgroundContents* contents = service->CreateBackgroundContents(
      site_instance.get(), route_id, main_frame_route_id,
      main_frame_widget_route_id, profile_, frame_name,
      base::ASCIIToUTF16(extension->id()), partition_id,
      session_storage_namespace);

  // When a separate process is used, the original renderer cannot access the
  // new window later, thus we need to navigate the window now.
  if (contents && !allow_js_access) {
    contents->web_contents()->GetController().LoadURL(
        target_url,
        content::Referrer(),
        ui::PAGE_TRANSITION_LINK,
        std::string());  // No extra headers.
  }

  return contents != NULL;
}
