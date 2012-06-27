// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#endif  // OS_WIN

#include <algorithm>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_started_animation.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_unload_handler.h"
#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/media/media_stream_devices_controller.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_dialog_creator.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tab_restore_service_delegate.h"
#include "chrome/browser/ui/browser_toolbar_model_delegate.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/fullscreen_controller.h"
#include "chrome/browser/ui/global_error.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/browser/ui/hung_plugin_tab_helper.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_delegate.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/ui/webui/feedback_ui.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/custom_handlers/protocol_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiling.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/web_apps.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/common/content_restriction.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/renderer_preferences.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources_standard.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/animation/animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/point.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_service_data.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/plugins/webplugininfo.h"

#if defined(OS_WIN)
#include "base/win/metro.h"
#include "chrome/browser/autofill/autofill_ie_toolbar_import_win.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/view_ids.h"
#include "ui/base/win/shell.h"
#endif  // OS_WIN

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#endif

#if defined(USE_ASH)
#include "ash/ash_switches.h"
#endif

using base::TimeDelta;
using content::NativeWebKeyboardEvent;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::PluginService;
using content::Referrer;
using content::SiteInstance;
using content::UserMetricsAction;
using content::WebContents;
using extensions::Extension;
using ui::WebDialogDelegate;

///////////////////////////////////////////////////////////////////////////////

namespace {

// The URL to be loaded to display the "Report a broken page" form.
const char kBrokenPageUrl[] =
    "https://www.google.com/support/chrome/bin/request.py?contact_type="
    "broken_website&format=inproduct&p.page_title=$1&p.page_url=$2";

// The URL for the privacy dashboard.
const char kPrivacyDashboardUrl[] = "https://www.google.com/dashboard";

// How long we wait before updating the browser chrome while loading a page.
const int kUIUpdateCoalescingTimeMS = 200;

bool AllowPanels(const std::string& app_name) {
  return PanelManager::ShouldUsePanels(
      web_app::GetExtensionIdFromApplicationName(app_name));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Browser, CreateParams:

Browser::CreateParams::CreateParams()
    : type(TYPE_TABBED),
      profile(NULL),
      initial_show_state(ui::SHOW_STATE_DEFAULT),
      is_session_restore(false) {
}

Browser::CreateParams::CreateParams(Type type, Profile* profile)
    : type(type),
      profile(profile),
      app_type(APP_TYPE_HOST),
      initial_show_state(ui::SHOW_STATE_DEFAULT),
      is_session_restore(false) {
}

// static
Browser::CreateParams Browser::CreateParams::CreateForApp(
    Type type,
    const std::string& app_name,
    const gfx::Rect& window_bounds,
    Profile* profile) {
  DCHECK(type != TYPE_TABBED);
  DCHECK(!app_name.empty());

  if (type == TYPE_PANEL && !AllowPanels(app_name))
    type = TYPE_POPUP;

  CreateParams params(type, profile);
  params.app_name = app_name;
  params.app_type = APP_TYPE_CHILD;
  params.initial_bounds = window_bounds;

  return params;
}

// static
Browser::CreateParams Browser::CreateParams::CreateForDevTools(
    Profile* profile) {
  CreateParams params(TYPE_POPUP, profile);
  params.app_name = DevToolsWindow::kDevToolsApp;
  return params;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Constructors, Creation, Showing:

Browser::Browser(Type type, Profile* profile)
    : type_(type),
      profile_(profile),
      window_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          tab_strip_model_(new TabStripModel(this, profile))),
      app_type_(APP_TYPE_HOST),
      chrome_updater_factory_(this),
      is_attempting_to_close_browser_(false),
      cancel_download_confirmation_state_(NOT_PROMPTED),
      initial_show_state_(ui::SHOW_STATE_DEFAULT),
      is_session_restore_(false),
      weak_factory_(this),
      pending_web_app_action_(NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          content_setting_bubble_model_delegate_(
              new BrowserContentSettingBubbleModelDelegate(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          toolbar_model_delegate_(
              new BrowserToolbarModelDelegate(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          tab_restore_service_delegate_(
              new BrowserTabRestoreServiceDelegate(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          synced_window_delegate_(
              new BrowserSyncedWindowDelegate(this))),
      bookmark_bar_state_(BookmarkBar::HIDDEN),
      device_attached_intent_source_(this, this),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          command_controller_(new chrome::BrowserCommandController(this))),
      window_has_shown_(false) {
  tab_strip_model_->AddObserver(this);

  toolbar_model_.reset(new ToolbarModel(toolbar_model_delegate_.get()));
  search_model_.reset(new chrome::search::SearchModel(NULL));
  search_delegate_.reset(
      new chrome::search::SearchDelegate(search_model_.get()));

  registrar_.Add(this, content::NOTIFICATION_SSL_VISIBLE_STATE_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
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
  profile_pref_registrar_.Add(prefs::kDevToolsDisabled, this);
  profile_pref_registrar_.Add(prefs::kShowBookmarkBar, this);
  profile_pref_registrar_.Add(prefs::kHomePage, this);
  profile_pref_registrar_.Add(prefs::kInstantEnabled, this);

  BrowserList::AddBrowser(this);

  // NOTE: These prefs all need to be explicitly destroyed in the destructor
  // or you'll get a nasty surprise when you run the incognito tests.
  encoding_auto_detect_.Init(prefs::kWebKitUsesUniversalDetector,
                             profile_->GetPrefs(), NULL);

  CreateInstantIfNecessary();

  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_INIT);

  FilePath profile_path = profile->GetPath();
  ProfileMetrics::LogProfileLaunch(profile_path);
}

Browser::~Browser() {
  // The tab strip should not have any tabs at this point.
  if (!browser_shutdown::ShuttingDownWithoutClosingBrowsers())
    DCHECK(tab_strip_model_->empty());
  tab_strip_model_->RemoveObserver(this);

  BrowserList::RemoveBrowser(this);

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (session_service)
    session_service->WindowClosed(session_id_);

  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  if (tab_restore_service)
    tab_restore_service->BrowserClosed(tab_restore_service_delegate());

#if !defined(OS_MACOSX)
  if (!browser::GetBrowserCount(profile_)) {
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

  command_controller_.reset();

  if (profile_->IsOffTheRecord() &&
      !BrowserList::IsOffTheRecordSessionActiveForProfile(profile_)) {
    // An incognito profile is no longer needed, this indirectly frees
    // its cache and cookies once it gets destroyed at the appropriate time.
    ProfileDestroyer::DestroyProfileWhenAppropriate(profile_);
  }

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
}

// static
Browser* Browser::Create(Profile* profile) {
  Browser* browser = new Browser(TYPE_TABBED, profile);
  browser->InitBrowserWindow();
  return browser;
}

// static
Browser* Browser::CreateWithParams(const CreateParams& params) {
  if (!params.app_name.empty())
    chrome::RegisterAppPrefs(params.app_name, params.profile);

  Browser* browser = new Browser(params.type, params.profile);
  browser->app_name_ = params.app_name;
  browser->app_type_ = params.app_type;
  browser->set_override_bounds(params.initial_bounds);
  browser->set_initial_show_state(params.initial_show_state);
  browser->set_is_session_restore(params.is_session_restore);

  browser->InitBrowserWindow();
  return browser;
}

void Browser::InitBrowserWindow() {
  DCHECK(!window_);

  window_ = CreateBrowserWindow();
  fullscreen_controller_ = new FullscreenController(window_, profile_, this);

#if defined(OS_WIN) && !defined(USE_AURA)
  // Set the app user model id for this application to that of the application
  // name.  See http://crbug.com/7028.
  ui::win::SetAppIdForWindow(
      is_app() && !is_type_panel() ?
      ShellIntegration::GetAppModelIdForProfile(UTF8ToWide(app_name_),
                                                profile_->GetPath()) :
      ShellIntegration::GetChromiumModelIdForProfile(profile_->GetPath()),
      window()->GetNativeWindow());

  if (is_type_panel()) {
    ui::win::SetAppIconForWindow(ShellIntegration::GetChromiumIconPath(),
                                 window()->GetNativeWindow());
  }
#endif

  // Create the extension window controller before sending notifications.
  extension_window_controller_.reset(
      new BrowserExtensionWindowController(this));

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::Source<Browser>(this),
      content::NotificationService::NoDetails());

  PrefService* local_state = g_browser_process->local_state();
  if (local_state && local_state->FindPreference(
      prefs::kAutofillPersonalDataManagerFirstRun) &&
      local_state->GetBoolean(prefs::kAutofillPersonalDataManagerFirstRun)) {
    // Notify PDM that this is a first run.
#if defined(OS_WIN)
    ImportAutofillDataWin(PersonalDataManagerFactory::GetForProfile(profile_));
#endif  // defined(OS_WIN)
    // Reset the preference so we don't call it again for subsequent windows.
    local_state->ClearPref(prefs::kAutofillPersonalDataManagerFirstRun);
  }
}

void Browser::SetWindowForTesting(BrowserWindow* window) {
  DCHECK(!window_);
  window_ = window;
  fullscreen_controller_ = new FullscreenController(window_, profile_, this);
}

///////////////////////////////////////////////////////////////////////////////
// Getters & Setters

FindBarController* Browser::GetFindBarController() {
  if (!find_bar_controller_.get()) {
    FindBar* find_bar = window_->CreateFindBar();
    find_bar_controller_.reset(new FindBarController(find_bar));
    find_bar->SetFindBarController(find_bar_controller_.get());
    find_bar_controller_->ChangeTabContents(GetActiveTabContents());
    find_bar_controller_->find_bar()->MoveWindowIfNecessary(gfx::Rect(), true);
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

SkBitmap Browser::GetCurrentPageIcon() const {
  TabContents* contents = GetActiveTabContents();
  // |contents| can be NULL since GetCurrentPageIcon() is called by the window
  // during the window's creation (before tabs have been added).
  return contents ? contents->favicon_tab_helper()->GetFavicon() : SkBitmap();
}

string16 Browser::GetWindowTitleForCurrentTab() const {
  WebContents* contents = GetActiveWebContents();
  string16 title;

  // |contents| can be NULL because GetWindowTitleForCurrentTab is called by the
  // window during the window's creation (before tabs have been added).
  if (contents) {
    title = contents->GetTitle();
    FormatTitleForDisplay(&title);
  }
  if (title.empty())
    title = CoreTabHelper::GetDefaultTitle();

#if defined(OS_MACOSX) || defined(USE_ASH)
  // On Mac or Ash, we don't want to suffix the page title with
  // the application name.
  return title;
#else
  int string_id = IDS_BROWSER_WINDOW_TITLE_FORMAT;
  // Don't append the app name to window titles on app frames and app popups
  if (is_app())
    string_id = IDS_BROWSER_WINDOW_TITLE_FORMAT_NO_LOGO;
  return l10n_util::GetStringFUTF16(string_id, title);
#endif
}

// static
void Browser::FormatTitleForDisplay(string16* title) {
  size_t current_index = 0;
  size_t match_index;
  while ((match_index = title->find(L'\n', current_index)) != string16::npos) {
    title->replace(match_index, 1, string16());
    current_index = match_index;
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, OnBeforeUnload handling:

bool Browser::ShouldCloseWindow() {
  if (!CanCloseWithInProgressDownloads())
    return false;

  if (HasCompletedUnloadProcessing())
    return true;

  is_attempting_to_close_browser_ = true;

  if (!TabsNeedBeforeUnloadFired())
    return true;

  ProcessPendingTabs();
  return false;
}

void Browser::OnWindowClosing() {
  if (!ShouldCloseWindow())
    return;

  bool exiting = false;

  // Application should shutdown on last window close if the user is explicitly
  // trying to quit, or if there is nothing keeping the browser alive (such as
  // AppController on the Mac, or BackgroundContentsService for background
  // pages).
  bool should_quit_if_last_browser =
      browser_shutdown::IsTryingToQuit() || !browser::WillKeepAlive();

  if (should_quit_if_last_browser && BrowserList::size() == 1) {
    browser_shutdown::OnShutdownStarting(browser_shutdown::WINDOW_CLOSE);
    exiting = true;
  }

  // Don't use GetForProfileIfExisting here, we want to force creation of the
  // session service so that user can restore what was open.
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile());
  if (session_service)
    session_service->WindowClosing(session_id());

  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());

#if defined(USE_AURA)
  if (tab_restore_service && is_app())
    tab_restore_service->BrowserClosing(tab_restore_service_delegate());
#endif

  if (tab_restore_service && is_type_tabbed() && tab_count())
    tab_restore_service->BrowserClosing(tab_restore_service_delegate());

  // TODO(sky): convert session/tab restore to use notification.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_CLOSING,
      content::Source<Browser>(this),
      content::Details<bool>(&exiting));

  CloseAllTabs();
}

void Browser::OnWindowActivated() {
  // On some platforms we want to automatically reload tabs that are
  // killed when the user selects them.
  WebContents* contents = GetActiveWebContents();
  if (contents && contents->GetCrashedStatus() ==
     base::TERMINATION_STATUS_PROCESS_WAS_KILLED) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kReloadKilledTabs)) {
      chrome::Reload(this, CURRENT_TAB);
    }
  }
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
}

Browser::DownloadClosePreventionType Browser::OkToCloseWithInProgressDownloads(
    int* num_downloads_blocking) const {
  DCHECK(num_downloads_blocking);
  *num_downloads_blocking = 0;

  if (is_attempting_to_close_browser_)
    return DOWNLOAD_CLOSE_OK;

  // If we're not running a full browser process with a profile manager
  // (testing), it's ok to close the browser.
  if (!g_browser_process->profile_manager())
    return DOWNLOAD_CLOSE_OK;

  int total_download_count = DownloadService::DownloadCountAllProfiles();
  if (total_download_count == 0)
    return DOWNLOAD_CLOSE_OK;   // No downloads; can definitely close.

  // Figure out how many windows are open total, and associated with this
  // profile, that are relevant for the ok-to-close decision.
  int profile_window_count = 0;
  int total_window_count = 0;
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    // Don't count this browser window or any other in the process of closing.
    Browser* const browser = *iter;
    // Check is_attempting_to_close_browser_ as window closing may be
    // delayed, and windows that are in the process of closing don't
    // count against our totals.
    if (browser == this || browser->is_attempting_to_close_browser_)
      continue;

    if ((*iter)->profile() == profile())
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
      DownloadServiceFactory::GetForProfile(profile());
  if (profile_window_count == 0 && download_service->DownloadCount() > 0 &&
      profile()->IsOffTheRecord()) {
    *num_downloads_blocking = download_service->DownloadCount();
    return DOWNLOAD_CLOSE_LAST_WINDOW_IN_INCOGNITO_PROFILE;
  }

  // Those are the only conditions under which we will block shutdown.
  return DOWNLOAD_CLOSE_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModel pass-thrus:

int Browser::tab_count() const {
  return tab_strip_model_->count();
}

int Browser::active_index() const {
  return tab_strip_model_->active_index();
}

int Browser::GetIndexOfController(
    const NavigationController* controller) const {
  return tab_strip_model_->GetIndexOfController(controller);
}

TabContents* Browser::GetActiveTabContents() const {
  return tab_strip_model_->GetActiveTabContents();
}

WebContents* Browser::GetActiveWebContents() const {
  TabContents* tab_contents = GetActiveTabContents();
  return tab_contents ? tab_contents->web_contents() : NULL;
}

TabContents* Browser::GetTabContentsAt(int index) const {
  return tab_strip_model_->GetTabContentsAt(index);
}

WebContents* Browser::GetWebContentsAt(int index) const {
  TabContents* tab_contents = GetTabContentsAt(index);
  if (tab_contents)
    return tab_contents->web_contents();
  return NULL;
}

void Browser::ActivateTabAt(int index, bool user_gesture) {
  tab_strip_model_->ActivateTabAt(index, user_gesture);
}

bool Browser::IsTabPinned(int index) const {
  return tab_strip_model_->IsTabPinned(index);
}

bool Browser::IsTabDiscarded(int index) const {
  return tab_strip_model_->IsTabDiscarded(index);
}

void Browser::CloseAllTabs() {
  tab_strip_model_->CloseAllTabs();
}

////////////////////////////////////////////////////////////////////////////////
// Browser, Tab adding/showing functions:

bool Browser::IsTabStripEditable() const {
  return window()->IsTabStripEditable();
}

int Browser::GetIndexForInsertionDuringRestore(int relative_index) {
  return (tab_strip_model_->insertion_policy() == TabStripModel::INSERT_AFTER) ?
      tab_count() : relative_index;
}

TabContents* Browser::AddSelectedTabWithURL(
    const GURL& url,
    content::PageTransition transition) {
  browser::NavigateParams params(this, url, transition);
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
  return params.target_contents;
}

WebContents* Browser::AddTab(TabContents* tab_contents,
                             content::PageTransition type) {
  tab_strip_model_->AddTabContents(tab_contents, -1, type,
                                   TabStripModel::ADD_ACTIVE);
  return tab_contents->web_contents();
}

WebContents* Browser::AddRestoredTab(
    const std::vector<TabNavigation>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    bool select,
    bool pin,
    bool from_last_session,
    content::SessionStorageNamespace* session_storage_namespace) {
  GURL restore_url = navigations.at(selected_navigation).virtual_url();
  TabContents* tab_contents = TabContentsFactory(
      profile(),
      tab_util::GetSiteInstanceForNewTab(profile_, restore_url),
      MSG_ROUTING_NONE,
      GetActiveWebContents(),
      session_storage_namespace);
  WebContents* new_tab = tab_contents->web_contents();
  tab_contents->extension_tab_helper()->SetExtensionAppById(extension_app_id);
  std::vector<NavigationEntry*> entries;
  TabNavigation::CreateNavigationEntriesFromTabNavigations(
      profile_, navigations, &entries);
  new_tab->GetController().Restore(
      selected_navigation, from_last_session, &entries);
  DCHECK_EQ(0u, entries.size());

  int add_types = select ? TabStripModel::ADD_ACTIVE :
      TabStripModel::ADD_NONE;
  if (pin) {
    int first_mini_tab_idx = tab_strip_model_->IndexOfFirstNonMiniTab();
    tab_index = std::min(tab_index, first_mini_tab_idx);
    add_types |= TabStripModel::ADD_PINNED;
  }
  tab_strip_model_->InsertTabContentsAt(tab_index, tab_contents, add_types);
  if (select) {
    window_->Activate();
  } else {
    // We set the size of the view here, before WebKit does its initial
    // layout.  If we don't, the initial layout of background tabs will be
    // performed with a view width of 0, which may cause script outputs and
    // anchor link location calculations to be incorrect even after a new
    // layout with proper view dimensions. TabStripModel::AddTabContents()
    // contains similar logic.
    new_tab->GetView()->SizeContents(window_->GetRestoredBounds().size());
    new_tab->WasHidden();
  }
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile_);
  if (session_service)
    session_service->TabRestored(tab_contents, pin);
  return new_tab;
}

void Browser::AddWebContents(WebContents* new_contents,
                             WindowOpenDisposition disposition,
                             const gfx::Rect& initial_pos,
                             bool user_gesture) {
  AddNewContents(NULL, new_contents, disposition, initial_pos, user_gesture);
}

void Browser::CloseTabContents(WebContents* contents) {
  CloseContents(contents);
}

void Browser::ReplaceRestoredTab(
    const std::vector<TabNavigation>& navigations,
    int selected_navigation,
    bool from_last_session,
    const std::string& extension_app_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  GURL restore_url = navigations.at(selected_navigation).virtual_url();
  TabContents* tab_contents = TabContentsFactory(
      profile(),
      tab_util::GetSiteInstanceForNewTab(profile_, restore_url),
      MSG_ROUTING_NONE,
      GetActiveWebContents(),
      session_storage_namespace);
  tab_contents->extension_tab_helper()->SetExtensionAppById(extension_app_id);
  WebContents* replacement = tab_contents->web_contents();
  std::vector<NavigationEntry*> entries;
  TabNavigation::CreateNavigationEntriesFromTabNavigations(
      profile_, navigations, &entries);
  replacement->GetController().Restore(
      selected_navigation, from_last_session, &entries);
  DCHECK_EQ(0u, entries.size());

  tab_strip_model_->ReplaceNavigationControllerAt(active_index(), tab_contents);
}

void Browser::WindowFullscreenStateChanged() {
  fullscreen_controller_->WindowFullscreenStateChanged();
  command_controller_->FullscreenStateChanged();
  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TOGGLE_FULLSCREEN);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted browser commands:

void Browser::ToggleFullscreenMode() {
  fullscreen_controller_->ToggleFullscreenMode();
}

void Browser::ToggleFullscreenModeWithExtension(const GURL& extension_url) {
  fullscreen_controller_->ToggleFullscreenModeWithExtension(extension_url);
}

#if defined(OS_MACOSX)
void Browser::TogglePresentationMode() {
  fullscreen_controller_->TogglePresentationMode();
}
#endif

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
    WebContents* contents = GetActiveWebContents();
    if (contents)
      contents->ResetOverrideEncoding();
  }
}

void Browser::OverrideEncoding(int encoding_id) {
  content::RecordAction(UserMetricsAction("OverrideEncoding"));
  const std::string selected_encoding =
      CharacterEncoding::GetCanonicalEncodingNameByCommandId(encoding_id);
  WebContents* contents = GetActiveWebContents();
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
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  const FilePath directory = profile_->last_selected_directory();

  // TODO(beng): figure out how to juggle this.
  gfx::NativeWindow parent_window = window_->GetNativeWindow();
  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                                  string16(), directory,
                                  NULL, 0, FILE_PATH_LITERAL(""),
                                  GetActiveWebContents(),
                                  parent_window, NULL);
}

void Browser::OpenCreateShortcutsDialog() {
  content::RecordAction(UserMetricsAction("CreateShortcut"));
#if !defined(OS_MACOSX)
  TabContents* current_tab = GetActiveTabContents();
  DCHECK(current_tab &&
      web_app::IsValidUrl(current_tab->web_contents()->GetURL())) <<
          "Menu item should be disabled.";

  NavigationEntry* entry =
      current_tab->web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return;

  // RVH's GetApplicationInfo should not be called before it returns.
  DCHECK(pending_web_app_action_ == NONE);
  pending_web_app_action_ = CREATE_SHORTCUT;

  // Start fetching web app info for CreateApplicationShortcut dialog and show
  // the dialog when the data is available in OnDidGetApplicationInfo.
  current_tab->extension_tab_helper()->GetApplicationInfo(entry->GetPageID());
#else
  NOTIMPLEMENTED();
#endif
}

void Browser::UpdateDownloadShelfVisibility(bool visible) {
  if (GetStatusBubble())
    GetStatusBubble()->UpdateDownloadShelfVisibility(visible);
}

bool Browser::OpenInstant(WindowOpenDisposition disposition) {
  if (!instant() || !instant()->PrepareForCommit() ||
      disposition == NEW_BACKGROUND_TAB) {
    // NEW_BACKGROUND_TAB results in leaving the omnibox open, so we don't
    // attempt to use the instant preview.
    return false;
  }

  if (disposition == CURRENT_TAB) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_COMMITTED,
        content::Source<TabContents>(instant()->CommitCurrentPreview(
            INSTANT_COMMIT_PRESSED_ENTER)),
        content::NotificationService::NoDetails());
    return true;
  }
  if (disposition == NEW_FOREGROUND_TAB) {
    TabContents* preview_contents = instant()->ReleasePreviewContents(
        INSTANT_COMMIT_PRESSED_ENTER, NULL);
    // HideInstant is invoked after release so that InstantController is not
    // active when HideInstant asks it for its state.
    HideInstant();
    preview_contents->web_contents()->GetController().PruneAllButActive();
    tab_strip_model_->AddTabContents(
        preview_contents,
        -1,
        instant()->last_transition_type(),
        TabStripModel::ADD_ACTIVE);
    instant()->CompleteRelease(preview_contents);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_COMMITTED,
        content::Source<TabContents>(preview_contents),
        content::NotificationService::NoDetails());
    return true;
  }
  // The omnibox currently doesn't use other dispositions, so we don't attempt
  // to handle them. If you hit this NOTREACHED file a bug and I'll (sky) add
  // support for the new disposition.
  NOTREACHED();
  return false;
}

///////////////////////////////////////////////////////////////////////////////

// static
bool Browser::RunUnloadEventsHelper(WebContents* contents) {
  // If the WebContents is not connected yet, then there's no unload
  // handler we can fire even if the WebContents has an unload listener.
  // One case where we hit this is in a tab that has an infinite loop
  // before load.
  if (contents->NeedToFireBeforeUnload()) {
    // If the page has unload listeners, then we tell the renderer to fire
    // them. Once they have fired, we'll get a message back saying whether
    // to proceed closing the page or not, which sends us back to this method
    // with the NeedToFireBeforeUnload bit cleared.
    contents->GetRenderViewHost()->FirePageBeforeUnload(false);
    return true;
  }
  return false;
}

// static
void Browser::JSOutOfMemoryHelper(WebContents* web_contents) {
  TabContents* tab_contents = TabContents::FromWebContents(web_contents);
  if (!tab_contents)
    return;

  InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();
  infobar_helper->AddInfoBar(new SimpleAlertInfoBarDelegate(
      infobar_helper,
      NULL,
      l10n_util::GetStringUTF16(IDS_JS_OUT_OF_MEMORY_PROMPT),
      true));
}

// static
void Browser::RegisterProtocolHandlerHelper(WebContents* web_contents,
                                            const std::string& protocol,
                                            const GURL& url,
                                            const string16& title,
                                            bool user_gesture) {
  TabContents* tab_contents = TabContents::FromWebContents(web_contents);
  if (!tab_contents || tab_contents->profile()->IsOffTheRecord())
    return;

  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(protocol, url, title);

  ProtocolHandlerRegistry* registry =
      tab_contents->profile()->GetProtocolHandlerRegistry();

  if (!registry->SilentlyHandleRegisterHandlerRequest(handler)) {
    content::RecordAction(
        UserMetricsAction("RegisterProtocolHandler.InfoBar_Shown"));
    InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();

    RegisterProtocolHandlerInfoBarDelegate* rph_delegate =
        new RegisterProtocolHandlerInfoBarDelegate(infobar_helper,
                                                   registry,
                                                   handler);

    for (size_t i = 0; i < infobar_helper->infobar_count(); i++) {
      InfoBarDelegate* delegate = infobar_helper->GetInfoBarDelegateAt(i);
      RegisterProtocolHandlerInfoBarDelegate* cast_delegate =
          delegate->AsRegisterProtocolHandlerInfoBarDelegate();
      if (cast_delegate != NULL && cast_delegate->IsReplacedBy(rph_delegate)) {
        infobar_helper->ReplaceInfoBar(cast_delegate, rph_delegate);
        rph_delegate = NULL;
        break;
      }
    }

    if (rph_delegate != NULL)
      infobar_helper->AddInfoBar(rph_delegate);
  }
}

// static
void Browser::FindReplyHelper(WebContents* web_contents,
                              int request_id,
                              int number_of_matches,
                              const gfx::Rect& selection_rect,
                              int active_match_ordinal,
                              bool final_update) {
  TabContents* tab_contents = TabContents::FromWebContents(web_contents);
  if (!tab_contents || !tab_contents->find_tab_helper())
    return;

  tab_contents->find_tab_helper()->HandleFindReply(request_id,
                                                   number_of_matches,
                                                   selection_rect,
                                                   active_match_ordinal,
                                                   final_update);
}

void Browser::UpdateUIForNavigationInTab(TabContents* contents,
                                         content::PageTransition transition,
                                         bool user_initiated) {
  tab_strip_model_->TabNavigating(contents, transition);

  bool contents_is_selected = contents == GetActiveTabContents();
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
  ScheduleUIUpdate(contents->web_contents(), content::INVALIDATE_TYPE_URL);

  if (contents_is_selected)
    contents->web_contents()->Focus();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, PageNavigator implementation:

WebContents* Browser::OpenURL(const OpenURLParams& params) {
  return OpenURLFromTab(NULL, params);
}

// Centralized method for creating a TabContents, configuring and
// installing all its supporting objects and observers.
TabContents* Browser::TabContentsFactory(
    Profile* profile,
    SiteInstance* site_instance,
    int routing_id,
    const WebContents* base_web_contents,
    content::SessionStorageNamespace* session_storage_namespace) {
  WebContents* new_contents = WebContents::Create(
      profile, site_instance, routing_id, base_web_contents,
      session_storage_namespace);
  TabContents* tab_contents = new TabContents(new_contents);
  return tab_contents;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModelDelegate implementation:

TabContents* Browser::AddBlankTab(bool foreground) {
  return AddBlankTabAt(-1, foreground);
}

TabContents* Browser::AddBlankTabAt(int index, bool foreground) {
  // Time new tab page creation time.  We keep track of the timing data in
  // WebContents, but we want to include the time it takes to create the
  // WebContents object too.
  base::TimeTicks new_tab_start_time = base::TimeTicks::Now();
  browser::NavigateParams params(this, GURL(chrome::kChromeUINewTabURL),
                                 content::PAGE_TRANSITION_TYPED);
  params.disposition = foreground ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  params.tabstrip_index = index;
  browser::Navigate(&params);
  params.target_contents->web_contents()->SetNewTabStartTime(
      new_tab_start_time);
  return params.target_contents;
}

Browser* Browser::CreateNewStripWithContents(
    TabContents* detached_contents,
    const gfx::Rect& window_bounds,
    const DockInfo& dock_info,
    bool maximize) {
  DCHECK(CanSupportWindowFeature(FEATURE_TABSTRIP));

  gfx::Rect new_window_bounds = window_bounds;
  if (dock_info.GetNewWindowBounds(&new_window_bounds, &maximize))
    dock_info.AdjustOtherWindowBounds();

  // Create an empty new browser window the same size as the old one.
  Browser* browser = new Browser(TYPE_TABBED, profile_);
  browser->set_override_bounds(new_window_bounds);
  browser->set_initial_show_state(
      maximize ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_NORMAL);
  browser->InitBrowserWindow();
  browser->tab_strip_model()->AppendTabContents(detached_contents, true);
  // Make sure the loading state is updated correctly, otherwise the throbber
  // won't start if the page is loading.
  browser->LoadingStateChanged(detached_contents->web_contents());
  return browser;
}

int Browser::GetDragActions() const {
  return TabStripModelDelegate::TAB_TEAROFF_ACTION | (tab_count() > 1 ?
      TabStripModelDelegate::TAB_MOVE_ACTION : 0);
}

TabContents* Browser::CreateTabContentsForURL(
    const GURL& url, const content::Referrer& referrer, Profile* profile,
    content::PageTransition transition, bool defer_load,
    SiteInstance* instance) const {
  TabContents* contents = TabContentsFactory(profile, instance,
      MSG_ROUTING_NONE, GetActiveWebContents(), NULL);
  if (!defer_load) {
    // Load the initial URL before adding the new tab contents to the tab strip
    // so that the tab contents has navigation state.
    contents->web_contents()->GetController().LoadURL(
        url, referrer, transition, std::string());
  }

  return contents;
}

bool Browser::CanDuplicateContentsAt(int index) {
  NavigationController& nc = GetWebContentsAt(index)->GetController();
  return nc.GetWebContents() && nc.GetLastCommittedEntry();
}

void Browser::DuplicateContentsAt(int index) {
  TabContents* contents = GetTabContentsAt(index);
  CHECK(contents);
  TabContents* contents_dupe = contents->Clone();

  bool pinned = false;
  if (CanSupportWindowFeature(FEATURE_TABSTRIP)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    int index = tab_strip_model_->GetIndexOfTabContents(contents);
    pinned = IsTabPinned(index);
    int add_types = TabStripModel::ADD_ACTIVE |
        TabStripModel::ADD_INHERIT_GROUP |
        (pinned ? TabStripModel::ADD_PINNED : 0);
    tab_strip_model_->InsertTabContentsAt(index + 1, contents_dupe, add_types);
  } else {
    Browser* browser = NULL;
    if (is_app()) {
      CHECK(!is_type_popup());
      CHECK(!is_type_panel());
      browser = Browser::CreateWithParams(
          Browser::CreateParams::CreateForApp(
              TYPE_POPUP, app_name_, gfx::Rect(),profile_));
    } else if (is_type_popup()) {
      browser = Browser::CreateWithParams(
          Browser::CreateParams(TYPE_POPUP, profile_));
    }

    // Preserve the size of the original window. The new window has already
    // been given an offset by the OS, so we shouldn't copy the old bounds.
    BrowserWindow* new_window = browser->window();
    new_window->SetBounds(gfx::Rect(new_window->GetRestoredBounds().origin(),
                          window()->GetRestoredBounds().size()));

    // We need to show the browser now.  Otherwise ContainerWin assumes the
    // WebContents is invisible and won't size it.
    browser->window()->Show();

    // The page transition below is only for the purpose of inserting the tab.
    browser->AddTab(contents_dupe, content::PAGE_TRANSITION_LINK);
  }

  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile_);
  if (session_service)
    session_service->TabRestored(contents_dupe, pinned);
}

void Browser::CloseFrameAfterDragSession() {
#if !defined(OS_MACOSX)
  // This is scheduled to run after we return to the message loop because
  // otherwise the frame will think the drag session is still active and ignore
  // the request.
  // TODO(port): figure out what is required here in a cross-platform world
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&Browser::CloseFrame, weak_factory_.GetWeakPtr()));
#endif
}

void Browser::CreateHistoricalTab(TabContents* contents) {
  // We don't create historical tabs for incognito windows or windows without
  // profiles.
  if (!profile() || profile()->IsOffTheRecord())
    return;

  // We don't create historical tabs for print preview tabs.
  if (contents->web_contents()->GetURL() == GURL(chrome::kChromeUIPrintURL))
    return;

  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile());

  // We only create historical tab entries for tabbed browser windows.
  if (service && CanSupportWindowFeature(FEATURE_TABSTRIP)) {
    service->CreateHistoricalTab(contents->web_contents(),
        tab_strip_model_->GetIndexOfTabContents(contents));
  }
}

bool Browser::RunUnloadListenerBeforeClosing(TabContents* contents) {
  return Browser::RunUnloadEventsHelper(contents->web_contents());
}

bool Browser::CanBookmarkAllTabs() const {
  return chrome::CanBookmarkAllTabs(this);
}

void Browser::BookmarkAllTabs() {
  chrome::BookmarkAllTabs(this);
}

bool Browser::CanRestoreTab() {
  return chrome::CanRestoreTab(this);
}

void Browser::RestoreTab() {
  chrome::RestoreTab(this);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModelObserver implementation:

void Browser::TabInsertedAt(TabContents* contents,
                            int index,
                            bool foreground) {
  SetAsDelegate(contents, this);
  contents->restore_tab_helper()->SetWindowID(session_id());

  SyncHistoryWithTabs(index);

  // Make sure the loading state is updated correctly, otherwise the throbber
  // won't start if the page is loading.
  LoadingStateChanged(contents->web_contents());

  // If the tab crashes in the beforeunload or unload handler, it won't be
  // able to ack. But we know we can close it.
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                 content::Source<WebContents>(contents->web_contents()));

  registrar_.Add(this, content::NOTIFICATION_INTERSTITIAL_ATTACHED,
                 content::Source<WebContents>(contents->web_contents()));

  registrar_.Add(this, content::NOTIFICATION_INTERSTITIAL_DETACHED,
                 content::Source<WebContents>(contents->web_contents()));
}

void Browser::TabClosingAt(TabStripModel* tab_strip_model,
                           TabContents* contents,
                           int index) {
  fullscreen_controller_->OnTabClosing(contents->web_contents());
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CLOSING,
      content::Source<NavigationController>(
          &contents->web_contents()->GetController()),
      content::NotificationService::NoDetails());

  // Sever the WebContents' connection back to us.
  SetAsDelegate(contents, NULL);
}

void Browser::TabDetachedAt(TabContents* contents, int index) {
  TabDetachedAtImpl(contents, index, DETACH_TYPE_DETACH);
}

void Browser::TabDeactivated(TabContents* contents) {
  fullscreen_controller_->OnTabDeactivated(contents);
  search_delegate_->OnTabDeactivated(contents);

  if (instant())
    instant()->Hide();

  // Save what the user's currently typing, so it can be restored when we
  // switch back to this tab.
  window_->GetLocationBar()->SaveStateToContents(contents->web_contents());
}

void Browser::ActiveTabChanged(TabContents* old_contents,
                               TabContents* new_contents,
                               int index,
                               bool user_gesture) {
  // On some platforms we want to automatically reload tabs that are
  // killed when the user selects them.
  bool did_reload = false;
  if (user_gesture && new_contents->web_contents()->GetCrashedStatus() ==
        base::TERMINATION_STATUS_PROCESS_WAS_KILLED) {
    const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
    if (parsed_command_line.HasSwitch(switches::kReloadKilledTabs)) {
      LOG(WARNING) << "Reloading killed tab at " << index;
      static int reload_count = 0;
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Tabs.SadTab.ReloadCount", ++reload_count, 1, 1000, 50);
      chrome::Reload(this, CURRENT_TAB);
      did_reload = true;
    }
  }

  // Discarded tabs always get reloaded.
  if (!did_reload && IsTabDiscarded(index)) {
    LOG(WARNING) << "Reloading discarded tab at " << index;
    static int reload_count = 0;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Tabs.Discard.ReloadCount", ++reload_count, 1, 1000, 50);
    chrome::Reload(this, CURRENT_TAB);
  }

  // If we have any update pending, do it now.
  if (chrome_updater_factory_.HasWeakPtrs() && old_contents)
    ProcessPendingUIUpdates();

  // Propagate the profile to the location bar.
  UpdateToolbar(true);

  // Propagate tab state to toolbar, tab-strip, etc.
  UpdateSearchState(new_contents);

  // Update reload/stop state.
  command_controller_->LoadingStateChanged(
      new_contents->web_contents()->IsLoading(), true);

  // Update commands to reflect current state.
  command_controller_->TabStateChanged();

  // Reset the status bubble.
  StatusBubble* status_bubble = GetStatusBubble();
  if (status_bubble) {
    status_bubble->Hide();

    // Show the loading state (if any).
    status_bubble->SetStatus(
        GetActiveTabContents()->core_tab_helper()->GetStatusText());
  }

  if (HasFindBarController()) {
    find_bar_controller_->ChangeTabContents(new_contents);
    find_bar_controller_->find_bar()->MoveWindowIfNecessary(gfx::Rect(), true);
  }

  // Update sessions. Don't force creation of sessions. If sessions doesn't
  // exist, the change will be picked up by sessions when created.
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile_);
  if (session_service && !tab_strip_model_->closing_all()) {
    session_service->SetSelectedTabInWindow(session_id(), active_index());
  }

  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_SWITCH);
}

void Browser::TabMoved(TabContents* contents,
                       int from_index,
                       int to_index) {
  DCHECK(from_index >= 0 && to_index >= 0);
  // Notify the history service.
  SyncHistoryWithTabs(std::min(from_index, to_index));
}

void Browser::TabReplacedAt(TabStripModel* tab_strip_model,
                            TabContents* old_contents,
                            TabContents* new_contents,
                            int index) {
  TabDetachedAtImpl(old_contents, index, DETACH_TYPE_REPLACE);
  TabInsertedAt(new_contents, index, (index == active_index()));

  int entry_count =
      new_contents->web_contents()->GetController().GetEntryCount();
  if (entry_count > 0) {
    // Send out notification so that observers are updated appropriately.
    new_contents->web_contents()->GetController().NotifyEntryChanged(
        new_contents->web_contents()->GetController().GetEntryAtIndex(
            entry_count - 1),
        entry_count - 1);
  }

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile());
  if (session_service) {
    // The new_contents may end up with a different navigation stack. Force
    // the session service to update itself.
    session_service->TabRestored(new_contents, IsTabPinned(index));
  }

  content::DevToolsManager::GetInstance()->ContentsReplaced(
      old_contents->web_contents(), new_contents->web_contents());
}

void Browser::TabPinnedStateChanged(TabContents* contents, int index) {
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile());
  if (session_service) {
    session_service->SetPinnedState(
        session_id(),
        GetTabContentsAt(index)->restore_tab_helper()->session_id(),
        IsTabPinned(index));
  }
}

void Browser::TabStripEmpty() {
  // Close the frame after we return to the message loop (not immediately,
  // otherwise it will destroy this object before the stack has a chance to
  // cleanly unwind.)
  // Note: This will be called several times if TabStripEmpty is called several
  //       times. This is because it does not close the window if tabs are
  //       still present.
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&Browser::CloseFrame, weak_factory_.GetWeakPtr()));
  // Set is_attempting_to_close_browser_ here, so that extensions, etc, do not
  // attempt to add tabs to the browser before it closes.
  is_attempting_to_close_browser_ = true;
}

bool Browser::PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                     bool* is_keyboard_shortcut) {
  // Escape exits tabbed fullscreen mode.
  // TODO(koz): Write a test for this http://crbug.com/100441.
  if (event.windowsKeyCode == 27 &&
      fullscreen_controller_->HandleUserPressedEscape()) {
    return true;
  }
  return window()->PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void Browser::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  window()->HandleKeyboardEvent(event);
}

void Browser::OnAcceptFullscreenPermission(
    const GURL& url,
    FullscreenExitBubbleType bubble_type) {
  fullscreen_controller_->OnAcceptFullscreenPermission(url, bubble_type);
}

void Browser::OnDenyFullscreenPermission(FullscreenExitBubbleType bubble_type) {
  fullscreen_controller_->OnDenyFullscreenPermission(bubble_type);
}

bool Browser::TabsNeedBeforeUnloadFired() {
  if (tabs_needing_before_unload_fired_.empty()) {
    for (int i = 0; i < tab_count(); ++i) {
      WebContents* contents = GetTabContentsAt(i)->web_contents();
      if (contents->NeedToFireBeforeUnload())
        tabs_needing_before_unload_fired_.insert(contents);
    }
  }
  return !tabs_needing_before_unload_fired_.empty();
}

bool Browser::IsFullscreenForTabOrPending() const {
  return fullscreen_controller_->IsFullscreenForTabOrPending();
}

bool Browser::IsMouseLocked() const {
  return fullscreen_controller_->IsMouseLocked();
}

void Browser::OnWindowDidShow() {
  if (window_has_shown_)
    return;
  window_has_shown_ = true;

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
// Browser, protected:

BrowserWindow* Browser::CreateBrowserWindow() {
  bool create_panel = false;
#if defined(USE_ASH)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAuraPanelManager))
    create_panel = is_type_panel();
#elif !defined(OS_CHROMEOS)
  create_panel = is_type_panel();
#endif
  if (create_panel)
    return PanelManager::GetInstance()->CreatePanel(this)->browser_window();

  return BrowserWindow::CreateBrowserWindow(this);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, content::WebContentsDelegate implementation:

WebContents* Browser::OpenURLFromTab(WebContents* source,
                                     const OpenURLParams& params) {
  browser::NavigateParams nav_params(this, params.url, params.transition);
  nav_params.source_contents = GetTabContentsAt(
      tab_strip_model_->GetIndexOfWebContents(source));
  nav_params.referrer = params.referrer;
  nav_params.disposition = params.disposition;
  nav_params.tabstrip_add_types = TabStripModel::ADD_NONE;
  nav_params.window_action = browser::NavigateParams::SHOW_WINDOW;
  nav_params.user_gesture = true;
  nav_params.override_encoding = params.override_encoding;
  nav_params.is_renderer_initiated = params.is_renderer_initiated;
  nav_params.transferred_global_request_id =
      params.transferred_global_request_id;
  browser::Navigate(&nav_params);

  return nav_params.target_contents ?
      nav_params.target_contents->web_contents() : NULL;
}

void Browser::NavigationStateChanged(const WebContents* source,
                                     unsigned changed_flags) {
  // Only update the UI when something visible has changed.
  if (changed_flags)
    ScheduleUIUpdate(source, changed_flags);

  // We can synchronously update commands since they will only change once per
  // navigation, so we don't have to worry about flickering. We do, however,
  // need to update the command state early on load to always present usable
  // actions in the face of slow-to-commit pages.
  if (changed_flags & (content::INVALIDATE_TYPE_URL |
                       content::INVALIDATE_TYPE_LOAD))
    command_controller_->TabStateChanged();
}

void Browser::AddNewContents(WebContents* source,
                             WebContents* new_contents,
                             WindowOpenDisposition disposition,
                             const gfx::Rect& initial_pos,
                             bool user_gesture) {
  // No code for this yet.
  DCHECK(disposition != SAVE_TO_DISK);
  // Can't create a new contents for the current tab - invalid case.
  DCHECK(disposition != CURRENT_TAB);

  TabContents* source_tab_contents = NULL;
  BlockedContentTabHelper* source_blocked_content = NULL;
  TabContents* new_tab_contents = TabContents::FromWebContents(new_contents);
  if (!new_tab_contents) {
    new_tab_contents = new TabContents(new_contents);
  }
  if (source) {
    source_tab_contents = TabContents::FromWebContents(source);
    source_blocked_content = source_tab_contents->blocked_content_tab_helper();
  }

  if (source_tab_contents) {
    // Handle blocking of all contents.
    if (source_blocked_content->all_contents_blocked()) {
      source_blocked_content->AddTabContents(new_tab_contents,
                                             disposition,
                                             initial_pos,
                                             user_gesture);
      return;
    }

    // Handle blocking of popups.
    if ((disposition == NEW_POPUP) && !user_gesture &&
        !CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisablePopupBlocking)) {
      // Unrequested popups from normal pages are constrained unless they're in
      // the whitelist.  The popup owner will handle checking this.
      GetConstrainingTabContents(source_tab_contents)->
          blocked_content_tab_helper()->
              AddPopup(new_tab_contents, initial_pos, user_gesture);
      return;
    }

    new_contents->GetRenderViewHost()->DisassociateFromPopupCount();
  }

  browser::NavigateParams params(this, new_tab_contents);
  params.source_contents = source ?
      GetTabContentsAt(tab_strip_model_->GetIndexOfWebContents(source)): NULL;
  params.disposition = disposition;
  params.window_bounds = initial_pos;
  params.window_action = browser::NavigateParams::SHOW_WINDOW;
  params.user_gesture = user_gesture;
  browser::Navigate(&params);
}

void Browser::ActivateContents(WebContents* contents) {
  ActivateTabAt(tab_strip_model_->GetIndexOfWebContents(contents), false);
  window_->Activate();
}

void Browser::DeactivateContents(WebContents* contents) {
  window_->Deactivate();
}

void Browser::LoadingStateChanged(WebContents* source) {
  window_->UpdateLoadingAnimations(tab_strip_model_->TabsAreLoading());
  window_->UpdateTitleBar();

  WebContents* selected_contents = GetActiveWebContents();
  if (source == selected_contents) {
    bool is_loading = source->IsLoading();
    command_controller_->LoadingStateChanged(is_loading, false);
    if (GetStatusBubble()) {
      GetStatusBubble()->SetStatus(
          GetActiveTabContents()->core_tab_helper()->GetStatusText());
    }

    if (!is_loading && pending_web_app_action_ == UPDATE_SHORTCUT) {
      // Schedule a shortcut update when web application info is available if
      // last committed entry is not NULL. Last committed entry could be NULL
      // when an interstitial page is injected (e.g. bad https certificate,
      // malware site etc). When this happens, we abort the shortcut update.
      NavigationEntry* entry = source->GetController().GetLastCommittedEntry();
      if (entry) {
        TabContents::FromWebContents(source)->
            extension_tab_helper()->GetApplicationInfo(entry->GetPageID());
      } else {
        pending_web_app_action_ = NONE;
      }
    }
  }
}

void Browser::CloseContents(WebContents* source) {
  if (is_attempting_to_close_browser_) {
    // If we're trying to close the browser, just clear the state related to
    // waiting for unload to fire. Don't actually try to close the tab as it
    // will go down the slow shutdown path instead of the fast path of killing
    // all the renderer processes.
    ClearUnloadState(source, true);
    return;
  }

  int index = tab_strip_model_->GetIndexOfWebContents(source);
  if (index == TabStripModel::kNoTab) {
    NOTREACHED() << "CloseContents called for tab not in our strip";
    return;
  }
  tab_strip_model_->CloseTabContentsAt(index,
      TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
}

void Browser::MoveContents(WebContents* source, const gfx::Rect& pos) {
  if (!IsPopupOrPanel(source)) {
    NOTREACHED() << "moving invalid browser type";
    return;
  }
  window_->SetBounds(pos);
}

void Browser::DetachContents(WebContents* source) {
  int index = tab_strip_model_->GetIndexOfWebContents(source);
  if (index >= 0)
    tab_strip_model_->DetachTabContentsAt(index);
}

bool Browser::IsPopupOrPanel(const WebContents* source) const {
  // A non-tabbed BROWSER is an unconstrained popup.
  return is_type_popup() || is_type_panel();
}

void Browser::UpdateTargetURL(WebContents* source, int32 page_id,
                              const GURL& url) {
  if (!GetStatusBubble())
    return;

  if (source == GetActiveWebContents()) {
    PrefService* prefs = profile_->GetPrefs();
    GetStatusBubble()->SetURL(url, prefs->GetString(prefs::kAcceptLanguages));
  }
}

void Browser::ContentsMouseEvent(
    WebContents* source, const gfx::Point& location, bool motion) {
  if (!GetStatusBubble())
    return;

  if (source == GetActiveWebContents()) {
    GetStatusBubble()->MouseMoved(location, !motion);
    if (!motion)
      GetStatusBubble()->SetURL(GURL(), std::string());
  }
}

void Browser::ContentsZoomChange(bool zoom_in) {
  chrome::ExecuteCommand(this, zoom_in ? IDC_ZOOM_PLUS : IDC_ZOOM_MINUS);
}

void Browser::WebContentsFocused(WebContents* contents) {
  window_->WebContentsFocused(contents);
}

bool Browser::TakeFocus(bool reverse) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FOCUS_RETURNED_TO_BROWSER,
      content::Source<Browser>(this),
      content::NotificationService::NoDetails());
  return false;
}

bool Browser::IsApplication() const {
  return is_app();
}

void Browser::ConvertContentsToApplication(WebContents* contents) {
  const GURL& url = contents->GetController().GetActiveEntry()->GetURL();
  std::string app_name = web_app::GenerateApplicationNameFromURL(url);

  DetachContents(contents);
  Browser* app_browser = Browser::CreateWithParams(
      Browser::CreateParams::CreateForApp(
          TYPE_POPUP, app_name, gfx::Rect(), profile_));
  TabContents* tab_contents = TabContents::FromWebContents(contents);
  if (!tab_contents)
    tab_contents = new TabContents(contents);
  app_browser->tab_strip_model()->AppendTabContents(tab_contents, true);

  contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  contents->GetRenderViewHost()->SyncRendererPrefs();
  app_browser->window()->Show();
}

gfx::Rect Browser::GetRootWindowResizerRect() const {
  return window_->GetRootWindowResizerRect();
}

void Browser::BeforeUnloadFired(WebContents* web_contents,
                                bool proceed,
                                bool* proceed_to_fire_unload) {
  if (!is_attempting_to_close_browser_) {
    *proceed_to_fire_unload = proceed;
    if (!proceed)
      web_contents->SetClosedByUserGesture(false);
    return;
  }

  if (!proceed) {
    CancelWindowClose();
    *proceed_to_fire_unload = false;
    web_contents->SetClosedByUserGesture(false);
    return;
  }

  if (RemoveFromSet(&tabs_needing_before_unload_fired_, web_contents)) {
    // Now that beforeunload has fired, put the tab on the queue to fire
    // unload.
    tabs_needing_unload_fired_.insert(web_contents);
    ProcessPendingTabs();
    // We want to handle firing the unload event ourselves since we want to
    // fire all the beforeunload events before attempting to fire the unload
    // events should the user cancel closing the browser.
    *proceed_to_fire_unload = false;
    return;
  }

  *proceed_to_fire_unload = true;
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

void Browser::RenderWidgetShowing() {
  window_->DisableInactiveFrame();
}

int Browser::GetExtraRenderViewHeight() const {
  return window_->GetExtraRenderViewHeight();
}

void Browser::OnStartDownload(WebContents* source,
                              content::DownloadItem* download) {
  TabContents* tab_contents = TabContents::FromWebContents(source);
  TabContents* constrained = GetConstrainingTabContents(tab_contents);
  if (constrained != tab_contents) {
    // Download in a constrained popup is shown in the tab that opened it.
    WebContents* constrained_tab = constrained->web_contents();
    constrained_tab->GetDelegate()->OnStartDownload(constrained_tab, download);
    return;
  }

  if (!window())
    return;

  // GetDownloadShelf creates the download shelf if it was not yet created.
  DownloadShelf* shelf = window()->GetDownloadShelf();
  shelf->AddDownload(new DownloadItemModel(download));
  // Don't show the animation for "Save file" downloads.
  // For non-theme extensions, we don't show the download animation.
  // Show animation in same window as the download shelf. Download shelf
  // may not be in the same window that initiated the download, e.g.
  // Panels.
  // Don't show the animation if the selected tab is not visible (i.e. the
  // window is minimized, we're in a unit test, etc.).
  WebContents* shelf_tab = shelf->browser()->GetActiveWebContents();
  if ((download->GetTotalBytes() > 0) &&
      !download_crx_util::IsExtensionDownload(*download) &&
      platform_util::IsVisible(shelf_tab->GetNativeView()) &&
      ui::Animation::ShouldRenderRichAnimation()) {
    DownloadStartedAnimation::Show(shelf_tab);
  }

  // If the download occurs in a new tab, close it.
  if (source->GetController().IsInitialNavigation() && tab_count() > 1)
    CloseContents(source);
}

void Browser::ViewSourceForTab(WebContents* source, const GURL& page_url) {
  DCHECK(source);
  TabContents* tab_contents = GetTabContentsAt(
      tab_strip_model_->GetIndexOfWebContents(source));
  chrome::ViewSource(this, tab_contents);
}

void Browser::ViewSourceForFrame(WebContents* source,
                                 const GURL& frame_url,
                                 const std::string& frame_content_state) {
  DCHECK(source);
  TabContents* tab_contents = GetTabContentsAt(
      tab_strip_model_->GetIndexOfWebContents(source));
  chrome::ViewSource(this, tab_contents, frame_url, frame_content_state);
}

void Browser::ShowRepostFormWarningDialog(WebContents* source) {
  browser::ShowTabModalConfirmDialog(
      new RepostFormWarningController(source),
      TabContents::FromWebContents(source));
}

bool Browser::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    content::NavigationType navigation_type) {
  // Don't update history if running as app.
  return !IsApplication();
}

bool Browser::ShouldCreateWebContents(
    WebContents* web_contents,
    int route_id,
    WindowContainerType window_container_type,
    const string16& frame_name,
    const GURL& target_url) {
  if (window_container_type == WINDOW_CONTAINER_TYPE_BACKGROUND) {
    // If a BackgroundContents is created, suppress the normal WebContents.
    return !MaybeCreateBackgroundContents(
        route_id, web_contents, frame_name, target_url);
  }

  return true;
}

void Browser::WebContentsCreated(WebContents* source_contents,
                                 int64 source_frame_id,
                                 const GURL& target_url,
                                 WebContents* new_contents) {
  // Create a TabContents now, so all observers are in place, as the network
  // requests for its initial navigation will start immediately. The WebContents
  // will later be inserted into this browser using Browser::Navigate via
  // AddNewContents. The latter will retrieve the newly created TabContents from
  // WebContents object.
  new TabContents(new_contents);

  // Notify.
  RetargetingDetails details;
  details.source_web_contents = source_contents;
  details.source_frame_id = source_frame_id;
  details.target_url = target_url;
  details.target_web_contents = new_contents;
  details.not_yet_in_tabstrip = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_RETARGETING,
      content::Source<Profile>(profile_),
      content::Details<RetargetingDetails>(&details));
}

void Browser::ContentRestrictionsChanged(WebContents* source) {
  command_controller_->ContentRestrictionsChanged();
}

void Browser::RendererUnresponsive(WebContents* source) {
  // Ignore hangs if print preview is open.
  TabContents* tab_contents = TabContents::FromWebContents(source);
  if (tab_contents) {
    printing::PrintPreviewTabController* controller =
        printing::PrintPreviewTabController::GetInstance();
    if (controller) {
      TabContents* preview_tab =
          controller->GetPrintPreviewForTab(tab_contents);
      if (preview_tab && preview_tab != tab_contents) {
        return;
      }
    }
  }

  browser::ShowHungRendererDialog(source);
}

void Browser::RendererResponsive(WebContents* source) {
  browser::HideHungRendererDialog(source);
}

void Browser::WorkerCrashed(WebContents* source) {
  TabContents* tab_contents = TabContents::FromWebContents(source);
  InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();
  infobar_helper->AddInfoBar(new SimpleAlertInfoBarDelegate(
      infobar_helper,
      NULL,
      l10n_util::GetStringUTF16(IDS_WEBWORKER_CRASHED_PROMPT),
      true));
}

void Browser::DidNavigateMainFramePostCommit(WebContents* web_contents) {
  if (web_contents == GetActiveWebContents())
    UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
}

void Browser::DidNavigateToPendingEntry(WebContents* web_contents) {
  if (web_contents == GetActiveWebContents())
    UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
}

content::JavaScriptDialogCreator* Browser::GetJavaScriptDialogCreator() {
  return GetJavaScriptDialogCreatorInstance();
}

content::ColorChooser* Browser::OpenColorChooser(WebContents* web_contents,
                                                 int color_chooser_id,
                                                 SkColor color) {
#if defined(OS_WIN)
  // On Windows, only create a color chooser if one doesn't exist, because we
  // can't close the old color chooser dialog.
  if (!color_chooser_.get())
    color_chooser_.reset(content::ColorChooser::Create(color_chooser_id,
                                                       web_contents,
                                                       color));
#else
  if (color_chooser_.get())
    color_chooser_->End();
  color_chooser_.reset(content::ColorChooser::Create(color_chooser_id,
                                                     web_contents,
                                                     color));
#endif
  return color_chooser_.get();
}

void Browser::DidEndColorChooser() {
  color_chooser_.reset();
}

void Browser::RunFileChooser(WebContents* web_contents,
                             const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(web_contents, params);
}

void Browser::EnumerateDirectory(WebContents* web_contents,
                                 int request_id,
                                 const FilePath& path) {
  FileSelectHelper::EnumerateDirectory(web_contents, request_id, path);
}

void Browser::ToggleFullscreenModeForTab(WebContents* web_contents,
                                         bool enter_fullscreen) {
  fullscreen_controller_->ToggleFullscreenModeForTab(web_contents,
                                                     enter_fullscreen);
}

bool Browser::IsFullscreenForTabOrPending(
    const WebContents* web_contents) const {
  return fullscreen_controller_->IsFullscreenForTabOrPending(web_contents);
}

void Browser::JSOutOfMemory(WebContents* web_contents) {
  JSOutOfMemoryHelper(web_contents);
}

void Browser::RegisterProtocolHandler(WebContents* web_contents,
                                      const std::string& protocol,
                                      const GURL& url,
                                      const string16& title,
                                      bool user_gesture) {
  RegisterProtocolHandlerHelper(
      web_contents, protocol, url, title, user_gesture);
}

void Browser::RegisterIntentHandler(
    WebContents* web_contents,
    const webkit_glue::WebIntentServiceData& data,
    bool user_gesture) {
  RegisterIntentHandlerHelper(web_contents, data, user_gesture);
}

void Browser::WebIntentDispatch(
    WebContents* web_contents,
    content::WebIntentsDispatcher* intents_dispatcher) {
  if (!web_intents::IsWebIntentsEnabledForProfile(profile_))
    return;

  UMA_HISTOGRAM_COUNTS("WebIntents.Dispatch", 1);

  if (!web_contents) {
    // Intent is system-caused and the picker will show over the currently
    // active web contents.
    web_contents = GetActiveWebContents();
  }
  TabContents* tab_contents = TabContents::FromWebContents(web_contents);
  tab_contents->web_intent_picker_controller()->SetIntentsDispatcher(
      intents_dispatcher);
  tab_contents->web_intent_picker_controller()->ShowDialog(
      intents_dispatcher->GetIntent().action,
      intents_dispatcher->GetIntent().type);
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
  FindReplyHelper(web_contents, request_id, number_of_matches, selection_rect,
                  active_match_ordinal, final_update);
}

void Browser::RequestToLockMouse(WebContents* web_contents,
                                 bool user_gesture,
                                 bool last_unlocked_by_target) {
  fullscreen_controller_->RequestToLockMouse(web_contents,
                                             user_gesture,
                                             last_unlocked_by_target);
}

void Browser::LostMouseLock() {
  fullscreen_controller_->LostMouseLock();
}

void Browser::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest* request,
    const content::MediaResponseCallback& callback) {
  TabContents* tab = TabContents::FromWebContents(web_contents);
  DCHECK(tab);

  scoped_ptr<MediaStreamDevicesController>
      controller(new MediaStreamDevicesController(tab->profile(),
                                                  request,
                                                  callback));
  if (!controller->DismissInfoBarAndTakeActionOnSettings()) {
    InfoBarTabHelper* infobar_helper = tab->infobar_tab_helper();
    InfoBarDelegate* old_infobar = NULL;
    for (size_t i = 0; i < infobar_helper->infobar_count(); ++i) {
      old_infobar = infobar_helper->GetInfoBarDelegateAt(i)->
          AsMediaStreamInfoBarDelegate();
      if (old_infobar)
        break;
    }

    InfoBarDelegate* infobar =
        new MediaStreamInfoBarDelegate(infobar_helper, controller.release());
    if (old_infobar)
      infobar_helper->ReplaceInfoBar(old_infobar, infobar);
    else
      infobar_helper->AddInfoBar(infobar);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, CoreTabHelperDelegate implementation:

void Browser::SwapTabContents(TabContents* old_tab_contents,
                              TabContents* new_tab_contents) {
  int index = tab_strip_model_->GetIndexOfTabContents(old_tab_contents);
  DCHECK_NE(TabStripModel::kNoTab, index);
  tab_strip_model_->ReplaceTabContentsAt(index, new_tab_contents);
}

bool Browser::CanReloadContents(TabContents* source) const {
  return chrome::CanReload(this);
}

bool Browser::CanSaveContents(TabContents* source) const {
  return chrome::CanSavePage(this);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, SearchEngineTabHelperDelegate implementation:

void Browser::ConfirmAddSearchProvider(TemplateURL* template_url,
                                       Profile* profile) {
  window()->ConfirmAddSearchProvider(template_url, profile);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ConstrainedWindowTabHelperDelegate implementation:

void Browser::SetTabContentBlocked(TabContents* tab_contents, bool blocked) {
  int index = tab_strip_model_->GetIndexOfTabContents(tab_contents);
  if (index == TabStripModel::kNoTab) {
    NOTREACHED();
    return;
  }
  tab_strip_model_->SetTabBlocked(index, blocked);
  command_controller_->PrintingStateChanged();
  if (!blocked && GetActiveTabContents() == tab_contents)
    tab_contents->web_contents()->Focus();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, BlockedContentTabHelperDelegate implementation:

TabContents* Browser::GetConstrainingTabContents(TabContents* source) {
  return source;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, BookmarkTabHelperDelegate implementation:

void Browser::URLStarredChanged(TabContents* source, bool starred) {
  if (source == GetActiveTabContents())
    window_->SetStarredState(starred);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ZoomObserver implementation:

void Browser::OnZoomIconChanged(TabContents* source,
                                ZoomController::ZoomIconState state) {
  if (source == GetActiveTabContents())
    window_->SetZoomIconState(state);
}

void Browser::OnZoomChanged(TabContents* source,
                            int zoom_percent,
                            bool can_show_bubble) {
  if (source == GetActiveTabContents()) {
    window_->SetZoomIconTooltipPercent(zoom_percent);

    // Only show the zoom bubble for zoom changes in the active window.
    if (can_show_bubble && window_->IsActive())
      window_->ShowZoomBubble(zoom_percent);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ExtensionTabHelperDelegate implementation:

void Browser::OnDidGetApplicationInfo(TabContents* source,
                                      int32 page_id) {
  if (GetActiveTabContents() != source)
    return;

  NavigationEntry* entry =
      source->web_contents()->GetController().GetLastCommittedEntry();
  if (!entry || (entry->GetPageID() != page_id))
    return;

  switch (pending_web_app_action_) {
    case CREATE_SHORTCUT: {
      window()->ShowCreateWebAppShortcutsDialog(source);
      break;
    }
    case UPDATE_SHORTCUT: {
      web_app::UpdateShortcutForTabContents(source);
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  pending_web_app_action_ = NONE;
}

void Browser::OnInstallApplication(TabContents* source,
                                   const WebApplicationInfo& web_app) {
  ExtensionService* extension_service = profile()->GetExtensionService();
  if (!extension_service)
    return;

  scoped_refptr<CrxInstaller> installer(CrxInstaller::Create(
      extension_service,
      extension_service->show_extensions_prompts() ?
      new ExtensionInstallPrompt(this) : NULL));
  installer->InstallWebApp(web_app);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, SelectFileDialog::Listener implementation:

void Browser::FileSelected(const FilePath& path, int index, void* params) {
  profile_->set_last_selected_directory(path.DirName());
  GURL file_url = net::FilePathToFileURL(path);

#if defined(OS_CHROMEOS)
  gdata::util::ModifyGDataFileResourceUrl(profile_, path, &file_url);
#endif

  if (file_url.is_empty())
    return;

  OpenURL(OpenURLParams(
      file_url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED,
      false));
}

///////////////////////////////////////////////////////////////////////////////
// Browser, content::NotificationObserver implementation:

void Browser::Observe(int type,
                      const content::NotificationSource& source,
                      const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED:
      if (is_attempting_to_close_browser_) {
        // Pass in false so that we delay processing. We need to delay the
        // processing as it may close the tab, which is currently on the call
        // stack above us.
        ClearUnloadState(content::Source<WebContents>(source).ptr(), false);
      }
      break;

    case content::NOTIFICATION_SSL_VISIBLE_STATE_CHANGED:
      // When the current tab's SSL state changes, we need to update the URL
      // bar to reflect the new state. Note that it's possible for the selected
      // tab contents to be NULL. This is because we listen for all sources
      // (NavigationControllers) for convenience, so the notification could
      // actually be for a different window while we're doing asynchronous
      // closing of this one.
      if (GetActiveWebContents() &&
          &GetActiveWebContents()->GetController() ==
          content::Source<NavigationController>(source).ptr())
        UpdateToolbar(false);
      break;

    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      if (window()->GetLocationBar())
        window()->GetLocationBar()->UpdatePageActions();

      // Close any tabs from the unloaded extension, unless it's terminated,
      // in which case let the sad tabs remain.
      if (content::Details<extensions::UnloadedExtensionInfo>(
            details)->reason != extension_misc::UNLOAD_REASON_TERMINATE) {
        const Extension* extension =
            content::Details<extensions::UnloadedExtensionInfo>(
                details)->extension;
        for (int i = tab_strip_model_->count() - 1; i >= 0; --i) {
          WebContents* tc =
              tab_strip_model_->GetTabContentsAt(i)->web_contents();
          bool close_tab_contents =
              tc->GetURL().SchemeIs(chrome::kExtensionScheme) &&
              tc->GetURL().host() == extension->id();
          // We want to close all panels originated by the unloaded extension.
          close_tab_contents = close_tab_contents ||
              (type_ == TYPE_PANEL &&
               (web_app::GetExtensionIdFromApplicationName(app_name_) ==
                extension->id()));
          if (close_tab_contents) {
            CloseTabContents(tc);
          }
        }
      }
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile_->IsSameProfile(profile) && window()->GetLocationBar())
        window()->GetLocationBar()->InvalidatePageActions();
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
    case chrome::NOTIFICATION_EXTENSION_LOADED:
      // During window creation on Windows we may end up calling into
      // SHAppBarMessage, which internally spawns a nested message loop. This
      // makes it possible for us to end up here before window creation has
      // completed,at which point window_ is NULL. See 94752 for details.
      if (window() && window()->GetLocationBar())
        window()->GetLocationBar()->UpdatePageActions();
      break;

#if defined(ENABLE_THEMES)
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      window()->UserChangedTheme();
      break;
#endif

    case chrome::NOTIFICATION_PREF_CHANGED: {
      const std::string& pref_name =
          *content::Details<std::string>(details).ptr();
      if (pref_name == prefs::kInstantEnabled) {
        if (browser_shutdown::ShuttingDownWithoutClosingBrowsers() ||
            !InstantController::IsEnabled(profile())) {
          if (instant()) {
            instant()->DestroyPreviewContents();
            instant_.reset();
            instant_unload_handler_.reset();
          }
        } else {
          CreateInstantIfNecessary();
        }
      } else if (pref_name == prefs::kDevToolsDisabled) {
        if (profile_->GetPrefs()->GetBoolean(prefs::kDevToolsDisabled))
          content::DevToolsManager::GetInstance()->CloseAllClientHosts();
      } else if (pref_name == prefs::kShowBookmarkBar) {
        UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE);
      } else if (pref_name == prefs::kHomePage) {
        PrefService* pref_service = content::Source<PrefService>(source).ptr();
        MarkHomePageAsChanged(pref_service);
      } else {
        NOTREACHED();
      }
      break;
    }

    case chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      if (web_contents == GetActiveWebContents()) {
        LocationBar* location_bar = window()->GetLocationBar();
        if (location_bar)
          location_bar->UpdateContentSettingsIcons();
      }
      break;
    }

    case content::NOTIFICATION_INTERSTITIAL_ATTACHED:
      UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
      break;

    case content::NOTIFICATION_INTERSTITIAL_DETACHED:
      UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
      break;

    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, InstantDelegate implementation:

void Browser::ShowInstant(TabContents* preview_contents) {
  window_->ShowInstant(preview_contents);

  // TODO(beng): investigate if we can avoid this and instead rely on the
  //             visibility of the WebContentsView
  GetActiveWebContents()->WasHidden();
  preview_contents->web_contents()->WasRestored();
}

void Browser::HideInstant() {
  window_->HideInstant();
  if (GetActiveWebContents())
    GetActiveWebContents()->WasRestored();
  if (instant_->GetPreviewContents())
    instant_->GetPreviewContents()->web_contents()->WasHidden();
}

void Browser::CommitInstant(TabContents* preview_contents) {
  TabContents* tab_contents = GetActiveTabContents();
  int index = tab_strip_model_->GetIndexOfTabContents(tab_contents);
  DCHECK_NE(TabStripModel::kNoTab, index);
  // TabStripModel takes ownership of preview_contents.
  tab_strip_model_->ReplaceTabContentsAt(index, preview_contents);
  // InstantUnloadHandler takes ownership of tab_contents.
  instant_unload_handler_->RunUnloadListenersOrDestroy(tab_contents, index);

  GURL url = preview_contents->web_contents()->GetURL();
  DCHECK(profile_->GetExtensionService());
  if (profile_->GetExtensionService()->IsInstalledApp(url)) {
    AppLauncherHandler::RecordAppLaunchType(
        extension_misc::APP_LAUNCH_OMNIBOX_INSTANT);
  }
}

void Browser::SetSuggestedText(const string16& text,
                               InstantCompleteBehavior behavior) {
  if (window()->GetLocationBar())
    window()->GetLocationBar()->SetSuggestedText(text, behavior);
}

gfx::Rect Browser::GetInstantBounds() {
  return window()->GetInstantBounds();
}

void Browser::InstantPreviewFocused() {
  // NOTE: This is only invoked on aura.
  window_->WebContentsFocused(instant_->GetPreviewContents()->web_contents());
}

TabContents* Browser::GetInstantHostTabContents() const {
  return GetActiveTabContents();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Command and state updating (private):

void Browser::MarkHomePageAsChanged(PrefService* pref_service) {
  pref_service->SetBoolean(prefs::kHomePageChanged, true);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, UI update coalescing and handling (private):

void Browser::UpdateToolbar(bool should_restore_state) {
  window_->UpdateToolbar(GetActiveTabContents(), should_restore_state);
}

void Browser::UpdateSearchState(TabContents* contents) {
  if (chrome::search::IsInstantExtendedAPIEnabled(profile_))
    search_delegate_->OnTabActivated(contents);
}

void Browser::ScheduleUIUpdate(const WebContents* source,
                               unsigned changed_flags) {
  if (!source)
    return;

  // Do some synchronous updates.
  if (changed_flags & content::INVALIDATE_TYPE_URL &&
      source == GetActiveWebContents()) {
    // Only update the URL for the current tab. Note that we do not update
    // the navigation commands since those would have already been updated
    // synchronously by NavigationStateChanged.
    UpdateToolbar(false);
    changed_flags &= ~content::INVALIDATE_TYPE_URL;
  }
  if (changed_flags & content::INVALIDATE_TYPE_LOAD) {
    // Update the loading state synchronously. This is so the throbber will
    // immediately start/stop, which gives a more snappy feel. We want to do
    // this for any tab so they start & stop quickly.
    tab_strip_model_->UpdateTabContentsStateAt(
        GetIndexOfController(&source->GetController()),
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
    tab_strip_model_->UpdateTabContentsStateAt(
        GetIndexOfController(&source->GetController()),
        TabStripModelObserver::TITLE_NOT_LOADING);
  }

  // If the only updates were synchronously handled above, we're done.
  if (changed_flags == 0)
    return;

  // Save the dirty bits.
  scheduled_updates_[source] |= changed_flags;

  if (!chrome_updater_factory_.HasWeakPtrs()) {
    // No task currently scheduled, start another.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&Browser::ProcessPendingUIUpdates,
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
    for (int tab = 0; tab < tab_count(); tab++) {
      if (GetWebContentsAt(tab) == i->first) {
        found = true;
        break;
      }
    }
    DCHECK(found);
  }
#endif

  chrome_updater_factory_.InvalidateWeakPtrs();

  for (UpdateMap::const_iterator i = scheduled_updates_.begin();
       i != scheduled_updates_.end(); ++i) {
    // Do not dereference |contents|, it may be out-of-date!
    const WebContents* contents = i->first;
    unsigned flags = i->second;

    if (contents == GetActiveWebContents()) {
      // Updates that only matter when the tab is selected go here.

      if (flags & content::INVALIDATE_TYPE_PAGE_ACTIONS) {
        LocationBar* location_bar = window()->GetLocationBar();
        if (location_bar)
          location_bar->UpdatePageActions();
      }
      // Updating the URL happens synchronously in ScheduleUIUpdate.
      if (flags & content::INVALIDATE_TYPE_LOAD && GetStatusBubble()) {
        GetStatusBubble()->SetStatus(
            GetActiveTabContents()->
                core_tab_helper()->GetStatusText());
      }

      if (flags & (content::INVALIDATE_TYPE_TAB |
                   content::INVALIDATE_TYPE_TITLE)) {
        window_->UpdateTitleBar();
      }
    }

    // Updates that don't depend upon the selected state go here.
    if (flags &
        (content::INVALIDATE_TYPE_TAB | content::INVALIDATE_TYPE_TITLE)) {
      tab_strip_model_->UpdateTabContentsStateAt(
          tab_strip_model_->GetIndexOfWebContents(contents),
          TabStripModelObserver::ALL);
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
  // In kiosk mode, we want to always hide the status bubble.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return NULL;
  return window_ ? window_->GetStatusBubble() : NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Session restore functions (private):

void Browser::SyncHistoryWithTabs(int index) {
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile());
  if (session_service) {
    for (int i = index; i < tab_count(); ++i) {
      TabContents* tab = GetTabContentsAt(i);
      if (tab) {
        session_service->SetTabIndexInWindow(
            session_id(), tab->restore_tab_helper()->session_id(), i);
        session_service->SetPinnedState(
            session_id(),
            tab->restore_tab_helper()->session_id(),
            IsTabPinned(i));
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, OnBeforeUnload handling (private):

void Browser::ProcessPendingTabs() {
  if (!is_attempting_to_close_browser_) {
    // Because we might invoke this after a delay it's possible for the value of
    // is_attempting_to_close_browser_ to have changed since we scheduled the
    // task.
    return;
  }

  if (HasCompletedUnloadProcessing()) {
    // We've finished all the unload events and can proceed to close the
    // browser.
    OnWindowClosing();
    return;
  }

  // Process beforeunload tabs first. When that queue is empty, process
  // unload tabs.
  if (!tabs_needing_before_unload_fired_.empty()) {
    WebContents* web_contents = *(tabs_needing_before_unload_fired_.begin());
    // Null check render_view_host here as this gets called on a PostTask and
    // the tab's render_view_host may have been nulled out.
    if (web_contents->GetRenderViewHost()) {
      web_contents->GetRenderViewHost()->FirePageBeforeUnload(false);
    } else {
      ClearUnloadState(web_contents, true);
    }
  } else if (!tabs_needing_unload_fired_.empty()) {
    // We've finished firing all beforeunload events and can proceed with unload
    // events.
    // TODO(ojan): We should add a call to browser_shutdown::OnShutdownStarting
    // somewhere around here so that we have accurate measurements of shutdown
    // time.
    // TODO(ojan): We can probably fire all the unload events in parallel and
    // get a perf benefit from that in the cases where the tab hangs in it's
    // unload handler or takes a long time to page in.
    WebContents* web_contents = *(tabs_needing_unload_fired_.begin());
    // Null check render_view_host here as this gets called on a PostTask and
    // the tab's render_view_host may have been nulled out.
    if (web_contents->GetRenderViewHost()) {
      web_contents->GetRenderViewHost()->ClosePage();
    } else {
      ClearUnloadState(web_contents, true);
    }
  } else {
    NOTREACHED();
  }
}

bool Browser::HasCompletedUnloadProcessing() const {
  return is_attempting_to_close_browser_ &&
      tabs_needing_before_unload_fired_.empty() &&
      tabs_needing_unload_fired_.empty();
}

void Browser::CancelWindowClose() {
  // Closing of window can be canceled from a beforeunload handler.
  DCHECK(is_attempting_to_close_browser_);
  tabs_needing_before_unload_fired_.clear();
  tabs_needing_unload_fired_.clear();
  is_attempting_to_close_browser_ = false;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
      content::Source<Browser>(this),
      content::NotificationService::NoDetails());
}

bool Browser::RemoveFromSet(UnloadListenerSet* set, WebContents* web_contents) {
  DCHECK(is_attempting_to_close_browser_);

  UnloadListenerSet::iterator iter =
      std::find(set->begin(), set->end(), web_contents);
  if (iter != set->end()) {
    set->erase(iter);
    return true;
  }
  return false;
}

void Browser::ClearUnloadState(WebContents* web_contents, bool process_now) {
  if (is_attempting_to_close_browser_) {
    RemoveFromSet(&tabs_needing_before_unload_fired_, web_contents);
    RemoveFromSet(&tabs_needing_unload_fired_, web_contents);
    if (process_now) {
      ProcessPendingTabs();
    } else {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&Browser::ProcessPendingTabs, weak_factory_.GetWeakPtr()));
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
  if (DOWNLOAD_CLOSE_OK ==
      OkToCloseWithInProgressDownloads(&num_downloads_blocking))
    return true;

  // Closing this window will kill some downloads; prompt to make sure
  // that's ok.
  cancel_download_confirmation_state_ = WAITING_FOR_RESPONSE;
  window_->ConfirmBrowserCloseWithPendingDownloads();

  // Return false so the browser does not close.  We'll close if the user
  // confirms in the dialog.
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted utility functions (private):

void Browser::SetAsDelegate(TabContents* tab, Browser* delegate) {
  // WebContents...
  tab->web_contents()->SetDelegate(delegate);

  // ...and all the helpers.
  tab->blocked_content_tab_helper()->set_delegate(delegate);
  tab->bookmark_tab_helper()->set_delegate(delegate);
  tab->zoom_controller()->set_observer(delegate);
  tab->constrained_window_tab_helper()->set_delegate(delegate);
  tab->core_tab_helper()->set_delegate(delegate);
  tab->extension_tab_helper()->set_delegate(delegate);
  tab->search_engine_tab_helper()->set_delegate(delegate);
}

void Browser::CloseFrame() {
  window_->Close();
}

void Browser::TabDetachedAtImpl(TabContents* contents, int index,
                                DetachType type) {
  if (type == DETACH_TYPE_DETACH) {
    // Save the current location bar state, but only if the tab being detached
    // is the selected tab.  Because saving state can conditionally revert the
    // location bar, saving the current tab's location bar state to a
    // non-selected tab can corrupt both tabs.
    if (contents == GetActiveTabContents()) {
      LocationBar* location_bar = window()->GetLocationBar();
      if (location_bar)
        location_bar->SaveStateToContents(contents->web_contents());
    }

    if (!tab_strip_model_->closing_all())
      SyncHistoryWithTabs(0);
  }

  SetAsDelegate(contents, NULL);
  RemoveScheduledUpdatesFor(contents->web_contents());

  if (find_bar_controller_.get() && index == active_index()) {
    find_bar_controller_->ChangeTabContents(NULL);
  }

  if (is_attempting_to_close_browser_) {
    // If this is the last tab with unload handlers, then ProcessPendingTabs
    // would call back into the TabStripModel (which is invoking this method on
    // us). Avoid that by passing in false so that the call to
    // ProcessPendingTabs is delayed.
    ClearUnloadState(contents->web_contents(), false);
  }

  // Stop observing search model changes for this tab.
  search_delegate_->OnTabDetached(contents);

  registrar_.Remove(this, content::NOTIFICATION_INTERSTITIAL_ATTACHED,
                    content::Source<WebContents>(contents->web_contents()));
  registrar_.Remove(this, content::NOTIFICATION_INTERSTITIAL_DETACHED,
                    content::Source<WebContents>(contents->web_contents()));
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                    content::Source<WebContents>(contents->web_contents()));
}

bool Browser::SupportsWindowFeatureImpl(WindowFeature feature,
                                        bool check_fullscreen) const {
  // On Mac, fullscreen mode has most normal things (in a slide-down panel). On
  // other platforms, we hide some controls when in fullscreen mode.
  bool hide_ui_for_fullscreen = false;
#if !defined(OS_MACOSX)
  hide_ui_for_fullscreen = check_fullscreen && window_ &&
      window_->IsFullscreen();
#endif

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

    if (!is_app())
      features |= FEATURE_LOCATIONBAR;
  }
  return !!(features & feature);
}

void Browser::CreateInstantIfNecessary() {
  if (is_type_tabbed() && InstantController::IsEnabled(profile()) &&
      !profile()->IsOffTheRecord()) {
    instant_.reset(new InstantController(this, InstantController::INSTANT));
    instant_unload_handler_.reset(new InstantUnloadHandler(this));
  }
}

void Browser::UpdateBookmarkBarState(BookmarkBarStateChangeReason reason) {
  BookmarkBar::State state;
  // The bookmark bar is hidden in fullscreen mode, unless on the new tab page.
  if (browser_defaults::bookmarks_enabled &&
      profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) &&
      (!window_ || !window_->IsFullscreen())) {
    state = BookmarkBar::SHOW;
  } else {
    TabContents* tab = GetActiveTabContents();
    if (tab && tab->bookmark_tab_helper()->ShouldShowBookmarkBar())
      state = BookmarkBar::DETACHED;
    else
      state = BookmarkBar::HIDDEN;
  }

  // Only allow the bookmark bar to be shown in default mode.
  if (!search_model_->mode().is_default())
    state = BookmarkBar::HIDDEN;

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

  BookmarkBar::AnimateChangeType animate_type =
      (reason == BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE) ?
      BookmarkBar::ANIMATE_STATE_CHANGE :
      BookmarkBar::DONT_ANIMATE_STATE_CHANGE;
  window_->BookmarkBarStateChanged(animate_type);
}

bool Browser::MaybeCreateBackgroundContents(int route_id,
                                            WebContents* opener_web_contents,
                                            const string16& frame_name,
                                            const GURL& target_url) {
  GURL opener_url = opener_web_contents->GetURL();
  ExtensionService* extensions_service = profile_->GetExtensionService();

  if (!opener_url.is_valid() ||
      frame_name.empty() ||
      !extensions_service ||
      !extensions_service->is_ready())
    return false;

  // Only hosted apps have web extents, so this ensures that only hosted apps
  // can create BackgroundContents. We don't have to check for background
  // permission as that is checked in RenderMessageFilter when the CreateWindow
  // message is processed.
  const Extension* extension =
      extensions_service->extensions()->GetHostedAppByURL(
          ExtensionURLInfo(opener_url));
  if (!extension)
    return false;

  // No BackgroundContents allowed if BackgroundContentsService doesn't exist.
  BackgroundContentsService* service =
      BackgroundContentsServiceFactory::GetForProfile(profile_);
  if (!service)
    return false;

  // Ensure that we're trying to open this from the extension's process.
  SiteInstance* opener_site_instance = opener_web_contents->GetSiteInstance();
  extensions::ProcessMap* process_map = extensions_service->process_map();
  if (!opener_site_instance->GetProcess() ||
      !process_map->Contains(
          extension->id(), opener_site_instance->GetProcess()->GetID())) {
    return false;
  }

  // Only allow a single background contents per app.
  bool allow_js_access = extension->allow_background_js_access();
  BackgroundContents* existing =
      service->GetAppBackgroundContents(ASCIIToUTF16(extension->id()));
  if (existing) {
    // For non-scriptable background contents, ignore the request altogether,
    // (returning true, so that a regular WebContents isn't created either).
    if (!allow_js_access)
      return true;
    // For scriptable background pages, if one already exists, close it (even
    // if it was specified in the manifest).
    DLOG(INFO) << "Closing existing BackgroundContents for " << opener_url;
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
      site_instance,
      route_id,
      profile_,
      frame_name,
      ASCIIToUTF16(extension->id()));

  // When a separate process is used, the original renderer cannot access the
  // new window later, thus we need to navigate the window now.
  if (contents && !allow_js_access) {
    contents->web_contents()->GetController().LoadURL(
        target_url,
        content::Referrer(),
        content::PAGE_TRANSITION_LINK,
        std::string());  // No extra headers.
  }

  return contents != NULL;
}
