// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#endif  // defined(OS_WIN)

#include <algorithm>
#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/process_info.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/api/infobars/simple_alert_infobar_delegate.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
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
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_infobar_delegate.h"
#include "chrome/browser/devtools/devtools_toggle_action.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/pepper_broker_infobar_delegate.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/repost_form_warning_controller.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_dialog_manager.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tab_contents.h"
#include "chrome/browser/ui/browser_tab_restore_service_delegate.h"
#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_toolbar_model_delegate.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/screen_capture_infobar_delegate.h"
#include "chrome/browser/ui/search/search.h"
#include "chrome/browser/ui/search/search_delegate.h"
#include "chrome/browser/ui/search/search_model.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/toolbar_model_impl.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
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
#include "chrome/common/search_types.h"
#include "chrome/common/startup_metric_utils.h"
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
#include "content/public/common/content_restriction.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/renderer_preferences.h"
#include "extensions/common/constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/point.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "webkit/glue/webkit_glue.h"
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
#include "chrome/browser/chromeos/drive/drive_file_system_util.h"
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

// How long we wait before updating the browser chrome while loading a page.
const int kUIUpdateCoalescingTimeMS = 200;

BrowserWindow* CreateBrowserWindow(Browser* browser) {
  return BrowserWindow::CreateBrowserWindow(browser);
}

#if defined(OS_CHROMEOS)
chrome::HostDesktopType kDefaultHostDesktopType = chrome::HOST_DESKTOP_TYPE_ASH;
#else
chrome::HostDesktopType kDefaultHostDesktopType =
    chrome::HOST_DESKTOP_TYPE_NATIVE;
#endif

bool ShouldReloadCrashedTab(WebContents* contents) {
#if defined(OS_CHROMEOS)
  return contents->IsCrashed();
#else
  return false;
#endif
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Browser, CreateParams:

// Deprecated: please use the form taking |host_desktop_type| below.
Browser::CreateParams::CreateParams(Profile* profile)
    : type(TYPE_TABBED),
      profile(profile),
      host_desktop_type(kDefaultHostDesktopType),
      app_type(APP_TYPE_HOST),
      initial_show_state(ui::SHOW_STATE_DEFAULT),
      is_session_restore(false),
      window(NULL) {
}

// Deprecated: please use the form taking |host_desktop_type| below.
Browser::CreateParams::CreateParams(Type type, Profile* profile)
    : type(type),
      profile(profile),
      host_desktop_type(kDefaultHostDesktopType),
      app_type(APP_TYPE_HOST),
      initial_show_state(ui::SHOW_STATE_DEFAULT),
      is_session_restore(false),
      window(NULL) {
}

Browser::CreateParams::CreateParams(Profile* profile,
                                    chrome::HostDesktopType host_desktop_type)
    : type(TYPE_TABBED),
      profile(profile),
      host_desktop_type(host_desktop_type),
      app_type(APP_TYPE_HOST),
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
      app_type(APP_TYPE_HOST),
      initial_show_state(ui::SHOW_STATE_DEFAULT),
      is_session_restore(false),
      window(NULL) {
}

// static
Browser::CreateParams Browser::CreateParams::CreateForApp(
    Type type,
    const std::string& app_name,
    const gfx::Rect& window_bounds,
    Profile* profile) {
  DCHECK(type != TYPE_TABBED);
  DCHECK(!app_name.empty());

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

Browser::Browser(const CreateParams& params)
    : type_(params.type),
      profile_(params.profile),
      window_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          tab_strip_model_delegate_(
            new chrome::BrowserTabStripModelDelegate(this))),
      tab_strip_model_(new TabStripModel(tab_strip_model_delegate_.get(),
                                         params.profile)),
      app_name_(params.app_name),
      app_type_(params.app_type),
      chrome_updater_factory_(this),
      cancel_download_confirmation_state_(NOT_PROMPTED),
      override_bounds_(params.initial_bounds),
      initial_show_state_(params.initial_show_state),
      is_session_restore_(params.is_session_restore),
      host_desktop_type_(params.host_desktop_type),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          unload_controller_(new chrome::UnloadController(this))),
      weak_factory_(this),
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
      ALLOW_THIS_IN_INITIALIZER_LIST(
          command_controller_(new chrome::BrowserCommandController(
              this, g_browser_process->profile_manager()))),
      window_has_shown_(false) {
  if (!app_name_.empty())
    chrome::RegisterAppPrefs(app_name_, profile_);
  tab_strip_model_->AddObserver(this);

  toolbar_model_.reset(new ToolbarModelImpl(toolbar_model_delegate_.get()));
  search_model_.reset(new chrome::search::SearchModel(NULL));
  search_delegate_.reset(
      new chrome::search::SearchDelegate(search_model_.get(),
                                         toolbar_model_.get()));

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
  profile_pref_registrar_.Add(
      prefs::kDevToolsDisabled,
      base::Bind(&Browser::OnDevToolsDisabledChanged, base::Unretained(this)));
  profile_pref_registrar_.Add(
      prefs::kShowBookmarkBar,
      base::Bind(&Browser::UpdateBookmarkBarState, base::Unretained(this),
                 BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE));
  profile_pref_registrar_.Add(
      prefs::kHomePage,
      base::Bind(&Browser::MarkHomePageAsChanged, base::Unretained(this)));

  BrowserList::AddBrowser(this);

  // NOTE: These prefs all need to be explicitly destroyed in the destructor
  // or you'll get a nasty surprise when you run the incognito tests.
  encoding_auto_detect_.Init(prefs::kWebKitUsesUniversalDetector,
                             profile_->GetPrefs());

  if (is_type_tabbed())
    instant_controller_.reset(new chrome::BrowserInstantController(this));

  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_INIT);

  base::FilePath profile_path = profile_->GetPath();
  ProfileMetrics::LogProfileLaunch(profile_path);

  window_ = params.window ? params.window : CreateBrowserWindow(this);

  // TODO(beng): move to BrowserFrameWin.
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
    ui::win::SetAppIconForWindow(ShellIntegration::GetChromiumIconLocation(),
                                 window()->GetNativeWindow());
  }
#endif

  // Create the extension window controller before sending notifications.
  extension_window_controller_.reset(
      new BrowserExtensionWindowController(this));

  // TODO(beng): Move BrowserList::AddBrowser() to the end of this function and
  //             replace uses of this with BL's notifications.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::Source<Browser>(this),
      content::NotificationService::NoDetails());

  // TODO(beng): move to ChromeBrowserMain:
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

  fullscreen_controller_.reset(new FullscreenController(this));
  search_model_->AddObserver(this);
}

Browser::~Browser() {
  // The tab strip should not have any tabs at this point.
  if (!browser_shutdown::ShuttingDownWithoutClosingBrowsers()) {
    // This CHECKs rather than DCHECKs in an attempt to catch
    // http://crbug.com/144879 . In any case, if there are WebContents in this
    // Browser, if we proceed further then they will almost certainly call
    // through their delegate pointers to this deallocated Browser and crash.
    CHECK(tab_strip_model_->empty());
  }

  search_model_->RemoveObserver(this);
  tab_strip_model_->RemoveObserver(this);

  // Destroy the BrowserCommandController before removing the browser, so that
  // it doesn't act on any notifications that are sent as a result of removing
  // the browser.
  command_controller_.reset();
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
  if (!chrome::GetBrowserCount(profile_)) {
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

///////////////////////////////////////////////////////////////////////////////
// Getters & Setters

FindBarController* Browser::GetFindBarController() {
  if (!find_bar_controller_.get()) {
    FindBar* find_bar = window_->CreateFindBar();
    find_bar_controller_.reset(new FindBarController(find_bar));
    find_bar->SetFindBarController(find_bar_controller_.get());
    find_bar_controller_->ChangeWebContents(
        tab_strip_model_->GetActiveWebContents());
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

gfx::Image Browser::GetCurrentPageIcon() const {
  WebContents* web_contents = tab_strip_model_->GetActiveWebContents();
  // |web_contents| can be NULL since GetCurrentPageIcon() is called by the
  // window during the window's creation (before tabs have been added).
  FaviconTabHelper* favicon_tab_helper =
      web_contents ? FaviconTabHelper::FromWebContents(web_contents) : NULL;
  return favicon_tab_helper ? favicon_tab_helper->GetFavicon() : gfx::Image();
}

string16 Browser::GetWindowTitleForCurrentTab() const {
  WebContents* contents = tab_strip_model_->GetActiveWebContents();
  string16 title;

  // |contents| can be NULL because GetWindowTitleForCurrentTab is called by the
  // window during the window's creation (before tabs have been added).
  if (contents) {
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

  return unload_controller_->ShouldCloseWindow();
}

bool Browser::IsAttemptingToCloseBrowser() const {
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

  if (should_quit_if_last_browser &&
      BrowserList::GetInstance(host_desktop_type_)->size() == 1) {
    browser_shutdown::OnShutdownStarting(browser_shutdown::WINDOW_CLOSE);
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

  if (tab_restore_service && is_type_tabbed() && tab_strip_model_->count())
    tab_restore_service->BrowserClosing(tab_restore_service_delegate());

  // TODO(sky): convert session/tab restore to use notification.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_CLOSING,
      content::Source<Browser>(this),
      content::NotificationService::NoDetails());

  tab_strip_model_->CloseAllTabs();
}

void Browser::OnWindowActivated() {
  // On some platforms we want to automatically reload tabs that are
  // killed when the user selects them.
  WebContents* contents = tab_strip_model_->GetActiveWebContents();
  if (contents && ShouldReloadCrashedTab(contents))
    chrome::Reload(this, CURRENT_TAB);
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

  if (IsAttemptingToCloseBrowser())
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
// Browser, Tab adding/showing functions:

void Browser::WindowFullscreenStateChanged() {
  fullscreen_controller_->WindowFullscreenStateChanged();
  command_controller_->FullscreenStateChanged();
  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TOGGLE_FULLSCREEN);
}

void Browser::VisibleSSLStateChanged(content::WebContents* web_contents) {
  // When the current tab's SSL state changes, we need to update the URL
  // bar to reflect the new state.
  DCHECK(web_contents);
  if (tab_strip_model_->GetActiveWebContents() == web_contents)
    UpdateToolbar(false);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted browser commands:

void Browser::ToggleFullscreenModeWithExtension(const GURL& extension_url) {
  fullscreen_controller_->ToggleFullscreenModeWithExtension(extension_url);
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
                                  string16(), directory,
                                  &file_types, 0, FILE_PATH_LITERAL(""),
                                  parent_window, NULL);
}

void Browser::UpdateDownloadShelfVisibility(bool visible) {
  if (GetStatusBubble())
    GetStatusBubble()->UpdateDownloadShelfVisibility(visible);
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
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  if (!infobar_service)
    return;

  SimpleAlertInfoBarDelegate::Create(
      infobar_service, NULL,
      l10n_util::GetStringUTF16(IDS_JS_OUT_OF_MEMORY_PROMPT), true);
}

// static
void Browser::RegisterProtocolHandlerHelper(WebContents* web_contents,
                                            const std::string& protocol,
                                            const GURL& url,
                                            const string16& title,
                                            bool user_gesture,
                                            BrowserWindow* window) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return;

  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(protocol, url, title);

  ProtocolHandlerRegistry* registry = profile->GetProtocolHandlerRegistry();
  TabSpecificContentSettings* tab_content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);

  if (registry->SilentlyHandleRegisterHandlerRequest(handler))
    return;

  if (!user_gesture && window) {
    tab_content_settings->set_pending_protocol_handler(handler);
    tab_content_settings->set_previous_protocol_handler(
        registry->GetHandlerFor(handler.protocol()));
    window->GetLocationBar()->UpdateContentSettingsIcons();
    return;
  }

  // Make sure content-setting icon is turned off in case the page does
  // ungestured and gestured RPH calls.
  if (window) {
    tab_content_settings->ClearPendingProtocolHandler();
    window->GetLocationBar()->UpdateContentSettingsIcons();
  }

  RegisterProtocolHandlerInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents), registry, handler);
}

// static
void Browser::FindReplyHelper(WebContents* web_contents,
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

void Browser::UpdateUIForNavigationInTab(WebContents* contents,
                                         content::PageTransition transition,
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
    contents->Focus();
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
  SetAsDelegate(contents, this);
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
  LoadingStateChanged(contents);

  registrar_.Add(this, content::NOTIFICATION_INTERSTITIAL_ATTACHED,
                 content::Source<WebContents>(contents));

  registrar_.Add(this, content::NOTIFICATION_INTERSTITIAL_DETACHED,
                 content::Source<WebContents>(contents));
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (session_service)
    session_service->TabInserted(contents);
}

void Browser::TabClosingAt(TabStripModel* tab_strip_model,
                           WebContents* contents,
                           int index) {
  fullscreen_controller_->OnTabClosing(contents);
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (session_service)
    session_service->TabClosing(contents);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CLOSING,
      content::Source<NavigationController>(&contents->GetController()),
      content::NotificationService::NoDetails());

  // Sever the WebContents' connection back to us.
  SetAsDelegate(contents, NULL);
}

void Browser::TabDetachedAt(WebContents* contents, int index) {
  TabDetachedAtImpl(contents, index, DETACH_TYPE_DETACH);
}

void Browser::TabDeactivated(WebContents* contents) {
  fullscreen_controller_->OnTabDeactivated(contents);
  search_delegate_->OnTabDeactivated(contents);

  // Save what the user's currently typing, so it can be restored when we
  // switch back to this tab.
  window_->GetLocationBar()->SaveStateToContents(contents);

  if (instant_controller_)
    instant_controller_->TabDeactivated(contents);
}

void Browser::ActiveTabChanged(WebContents* old_contents,
                               WebContents* new_contents,
                               int index,
                               bool user_gesture) {
  // On some platforms we want to automatically reload tabs that are
  // killed when the user selects them.
  bool did_reload = false;
  if (user_gesture && ShouldReloadCrashedTab(new_contents)) {
    LOG(WARNING) << "Reloading killed tab at " << index;
    static int reload_count = 0;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Tabs.SadTab.ReloadCount", ++reload_count, 1, 1000, 50);
    chrome::Reload(this, CURRENT_TAB);
    did_reload = true;
  }

  // Discarded tabs always get reloaded.
  if (!did_reload && tab_strip_model_->IsTabDiscarded(index)) {
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
    find_bar_controller_->find_bar()->MoveWindowIfNecessary(gfx::Rect(), true);
  }

  // Update sessions. Don't force creation of sessions. If sessions doesn't
  // exist, the change will be picked up by sessions when created.
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile_);
  if (session_service && !tab_strip_model_->closing_all()) {
    session_service->SetSelectedTabInWindow(session_id(),
                                            tab_strip_model_->active_index());
  }

  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_SWITCH);

  // This needs to be called after UpdateSearchState().
  if (instant_controller_)
    instant_controller_->ActiveTabChanged();
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
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (session_service)
    session_service->TabClosing(old_contents);
  TabInsertedAt(new_contents,
                index,
                (index == tab_strip_model_->active_index()));

  int entry_count = new_contents->GetController().GetEntryCount();
  if (entry_count > 0) {
    // Send out notification so that observers are updated appropriately.
    new_contents->GetController().NotifyEntryChanged(
        new_contents->GetController().GetEntryAtIndex(entry_count - 1),
        entry_count - 1);
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
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&Browser::CloseFrame, weak_factory_.GetWeakPtr()));

  // Instant may have visible WebContents that need to be detached before the
  // window system closes.
  instant_controller_.reset();
}

bool Browser::CanOverscrollContent() const {
#if defined(USE_AURA)
  bool overscroll_enabled = CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableOverscrollHistoryNavigation);
  return overscroll_enabled ? !is_app() && !is_devtools() && is_type_tabbed() :
                              false;
#else
  return false;
#endif
}

bool Browser::PreHandleKeyboardEvent(content::WebContents* source,
                                     const NativeWebKeyboardEvent& event,
                                     bool* is_keyboard_shortcut) {
  // Escape exits tabbed fullscreen mode.
  // TODO(koz): Write a test for this http://crbug.com/100441.
  if (event.windowsKeyCode == 27 &&
      fullscreen_controller_->HandleUserPressedEscape()) {
    return true;
  }
  return window()->PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void Browser::HandleKeyboardEvent(content::WebContents* source,
                                  const NativeWebKeyboardEvent& event) {
  window()->HandleKeyboardEvent(event);
}

bool Browser::TabsNeedBeforeUnloadFired() {
  return unload_controller_->TabsNeedBeforeUnloadFired();
}

bool Browser::IsMouseLocked() const {
  return fullscreen_controller_->IsMouseLocked();
}

void Browser::OnWindowDidShow() {
  if (window_has_shown_)
    return;
  window_has_shown_ = true;

// CurrentProcessInfo::CreationTime() is currently only implemented on Mac and
// Windows.
#if defined(OS_MACOSX) || defined(OS_WIN)
  // Measure the latency from startup till the first browser window becomes
  // visible.
  static bool is_first_browser_window = true;
  if (is_first_browser_window &&
      !startup_metric_utils::WasNonBrowserUIDisplayed()) {
    is_first_browser_window = false;
    const base::Time* process_creation_time =
        base::CurrentProcessInfo::CreationTime();

    if (process_creation_time) {
      UMA_HISTOGRAM_LONG_TIMES(
          "Startup.BrowserWindowDisplay",
          base::Time::Now() - *process_creation_time);
    }
  }
#endif  // defined(OS_MACOSX) || defined(OS_WIN)

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

void Browser::MaybeUpdateBookmarkBarStateForInstantPreview(
    const chrome::search::Mode& mode) {
  // This is invoked by a platform-specific implementation of
  // |InstantPreviewController| to update bookmark bar state according to
  // instant preview state.
  // ModeChanged() updates bookmark bar state for all mode transitions except
  // when new mode is |SEARCH_SUGGESTIONS|, because that needs to be done when
  // the suggestions are ready.
  if (mode.is_search_suggestions() &&
      bookmark_bar_state_ == BookmarkBar::SHOW) {
    UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, content::WebContentsDelegate implementation:

WebContents* Browser::OpenURLFromTab(WebContents* source,
                                     const OpenURLParams& params) {
  chrome::NavigateParams nav_params(this, params.url, params.transition);
  nav_params.source_contents = source;
  nav_params.referrer = params.referrer;
  nav_params.extra_headers = params.extra_headers;
  nav_params.disposition = params.disposition;
  nav_params.tabstrip_add_types = TabStripModel::ADD_NONE;
  nav_params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  nav_params.user_gesture = true;
  nav_params.override_encoding = params.override_encoding;
  nav_params.is_renderer_initiated = params.is_renderer_initiated;
  nav_params.transferred_global_request_id =
      params.transferred_global_request_id;
  nav_params.is_cross_site_redirect = params.is_cross_site_redirect;
  chrome::Navigate(&nav_params);

  return nav_params.target_contents;
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
                             bool user_gesture,
                             bool* was_blocked) {
  chrome::AddWebContents(this, source, new_contents, disposition, initial_pos,
                         user_gesture, was_blocked);
}

void Browser::ActivateContents(WebContents* contents) {
  tab_strip_model_->ActivateTabAt(
      tab_strip_model_->GetIndexOfWebContents(contents), false);
  window_->Activate();
}

void Browser::DeactivateContents(WebContents* contents) {
  window_->Deactivate();
}

void Browser::LoadingStateChanged(WebContents* source) {
  window_->UpdateLoadingAnimations(tab_strip_model_->TabsAreLoading());
  window_->UpdateTitleBar();

  WebContents* selected_contents = tab_strip_model_->GetActiveWebContents();
  if (source == selected_contents) {
    bool is_loading = source->IsLoading();
    command_controller_->LoadingStateChanged(is_loading, false);
    if (GetStatusBubble()) {
      GetStatusBubble()->SetStatus(CoreTabHelper::FromWebContents(
          tab_strip_model_->GetActiveWebContents())->GetStatusText());
    }
  }
}

void Browser::CloseContents(WebContents* source) {
  if (unload_controller_->CanCloseContents(source))
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
  // A non-tabbed BROWSER is an unconstrained popup.
  return is_type_popup() || is_type_panel();
}

void Browser::UpdateTargetURL(WebContents* source, int32 page_id,
                              const GURL& url) {
  if (!GetStatusBubble())
    return;

  if (source == tab_strip_model_->GetActiveWebContents()) {
    PrefService* prefs = profile_->GetPrefs();
    GetStatusBubble()->SetURL(url, prefs->GetString(prefs::kAcceptLanguages));
  }
}

void Browser::ContentsMouseEvent(
    WebContents* source, const gfx::Point& location, bool motion) {
  if (!GetStatusBubble())
    return;

  if (source == tab_strip_model_->GetActiveWebContents()) {
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
  *proceed_to_fire_unload =
      unload_controller_->BeforeUnloadFired(web_contents, proceed);
}

bool Browser::ShouldFocusLocationBarByDefault(WebContents* source) {
  return source->GetURL() == GURL(chrome::kChromeUINewTabURL);
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
  if (!DownloadItemModel(download).ShouldShowInShelf())
    return;

  WebContents* constrained = GetConstrainingWebContents(source);
  if (constrained != source) {
    // Download in a constrained popup is shown in the tab that opened it.
    constrained->GetDelegate()->OnStartDownload(constrained, download);
    return;
  }

  if (!window())
    return;

  // GetDownloadShelf creates the download shelf if it was not yet created.
  DownloadShelf* shelf = window()->GetDownloadShelf();
  shelf->AddDownload(download);

  // If the download occurs in a new tab, and it's not a save page
  // download (started before initial navigation completed), close it.
  if (source->GetController().IsInitialNavigation() &&
      tab_strip_model_->count() > 1 && !download->IsSavePackageDownload())
    CloseContents(source);
}

void Browser::ViewSourceForTab(WebContents* source, const GURL& page_url) {
  DCHECK(source);
  chrome::ViewSource(this, source);
}

void Browser::ViewSourceForFrame(WebContents* source,
                                 const GURL& frame_url,
                                 const std::string& frame_content_state) {
  DCHECK(source);
  chrome::ViewSource(this, source, frame_url, frame_content_state);
}

void Browser::ShowRepostFormWarningDialog(WebContents* source) {
  TabModalConfirmDialog::Create(new RepostFormWarningController(source),
                                source);
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
  // Adopt the WebContents now, so all observers are in place, as the network
  // requests for its initial navigation will start immediately. The WebContents
  // will later be inserted into this browser using Browser::Navigate via
  // AddNewContents.
  BrowserTabContents::AttachTabHelpers(new_contents);

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
  // Ignore hangs if a tab is blocked.
  int index = tab_strip_model_->GetIndexOfWebContents(source);
  DCHECK_NE(TabStripModel::kNoTab, index);
  if (tab_strip_model_->IsTabBlocked(index))
    return;

  chrome::ShowHungRendererDialog(source);
}

void Browser::RendererResponsive(WebContents* source) {
  chrome::HideHungRendererDialog(source);
}

void Browser::WorkerCrashed(WebContents* source) {
  SimpleAlertInfoBarDelegate::Create(
      InfoBarService::FromWebContents(source), NULL,
      l10n_util::GetStringUTF16(IDS_WEBWORKER_CRASHED_PROMPT), true);
}

void Browser::DidNavigateMainFramePostCommit(WebContents* web_contents) {
  if (web_contents == tab_strip_model_->GetActiveWebContents())
    UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
}

void Browser::DidNavigateToPendingEntry(WebContents* web_contents) {
  if (web_contents == tab_strip_model_->GetActiveWebContents())
    UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
}

content::JavaScriptDialogManager* Browser::GetJavaScriptDialogManager() {
  return GetJavaScriptDialogManagerInstance();
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
                                 const base::FilePath& path) {
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
      web_contents, protocol, url, title, user_gesture, window());
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
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  // The case when microphone access is requested together with screen capturing
  // is not supported yet. Just check requested video type to decide which
  // infobar to show.
  //
  // TODO(sergeyu): Add support for video stream with microphone, e.g. refactor
  // MediaStreamDevicesController to use a single infobar for both permissions,
  // or maybe show two infobars.
  if (request.video_type == content::MEDIA_SCREEN_VIDEO_CAPTURE)
    ScreenCaptureInfoBarDelegate::Create(web_contents, request, callback);
  else
    MediaStreamInfoBarDelegate::Create(web_contents, request, callback);
}

bool Browser::RequestPpapiBrokerPermission(
    WebContents* web_contents,
    const GURL& url,
    const base::FilePath& plugin_path,
    const base::Callback<void(bool)>& callback) {
  PepperBrokerInfoBarDelegate::Create(web_contents, url, plugin_path, callback);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, CoreTabHelperDelegate implementation:

void Browser::SwapTabContents(content::WebContents* old_contents,
                              content::WebContents* new_contents) {
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
// Browser, WebContentsModalDialogManagerDelegate implementation:

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

bool Browser::GetDialogTopCenter(gfx::Point* point) {
  int y = 0;
  if (window_->GetConstrainedWindowTopY(&y)) {
    *point = gfx::Point(window_->GetBounds().width() / 2, y);
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, BlockedContentTabHelperDelegate implementation:

content::WebContents* Browser::GetConstrainingWebContents(
    content::WebContents* source) {
  return source;
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

void Browser::OnZoomChanged(content::WebContents* source,
                            bool can_show_bubble) {
  if (source == tab_strip_model_->GetActiveWebContents()) {
    // Only show the zoom bubble for zoom changes in the active window.
    window_->ZoomChangedForActiveTab(can_show_bubble && window_->IsActive());
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ui::SelectFileDialog::Listener implementation:

void Browser::FileSelected(const base::FilePath& path, int index,
                           void* params) {
  FileSelectedWithExtraInfo(ui::SelectedFileInfo(path, path), index, params);
}

void Browser::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file_info,
    int index,
    void* params) {
  profile_->set_last_selected_directory(file_info.file_path.DirName());

  const base::FilePath& path = file_info.local_path;
  GURL file_url = net::FilePathToFileURL(path);

#if defined(OS_CHROMEOS)
  drive::util::ModifyDriveFileResourceUrl(profile_, path, &file_url);
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
        // Iterate backwards as we may remove items while iterating.
        for (int i = tab_strip_model_->count() - 1; i >= 0; --i) {
          WebContents* web_contents = tab_strip_model_->GetWebContentsAt(i);
          // Two cases are handled here:
          // - The scheme check is for when an extension page is loaded in a
          //   tab, e.g. chrome-extension://id/page.html.
          // - The extension_app check is for apps, which can have non-extension
          //   schemes, e.g. https://mail.google.com if you have the Gmail app
          //   installed.
          if ((web_contents->GetURL().SchemeIs(extensions::kExtensionScheme) &&
               web_contents->GetURL().host() == extension->id()) ||
              (extensions::TabHelper::FromWebContents(
                   web_contents)->extension_app() == extension)) {
            tab_strip_model_->CloseWebContentsAt(i, TabStripModel::CLOSE_NONE);
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

    case chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      if (web_contents == tab_strip_model_->GetActiveWebContents()) {
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

void Browser::ModeChanged(const chrome::search::Mode& old_mode,
                          const chrome::search::Mode& new_mode) {
  // If new mode is |SEARCH_SUGGESTIONS|, don't update bookmark bar state now;
  // wait till the instant preview is ready to show suggestions before hiding
  // the bookmark bar (in MaybeUpdateBookmarkBarStateForInstantPreview()).
  // TODO(kuan): but for now, only delay updating bookmark bar state if origin
  // is |DEFAULT|; other origins require more complex logic to be implemented
  // to prevent jankiness caused by hiding bookmark bar, so just hide the
  // bookmark bar immediately and tolerate the jankiness for a while.
  // For other mode transitions, update bookmark bar state accordingly.
  if (new_mode.is_search_suggestions() &&
      new_mode.is_origin_default() &&
      bookmark_bar_state_ == BookmarkBar::SHOW) {
    return;
  }
  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Command and state updating (private):

void Browser::OnDevToolsDisabledChanged() {
  if (profile_->GetPrefs()->GetBoolean(prefs::kDevToolsDisabled))
    content::DevToolsManager::GetInstance()->CloseAllClientHosts();
}

void Browser::MarkHomePageAsChanged() {
  profile_->GetPrefs()->SetBoolean(prefs::kHomePageChanged, true);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, UI update coalescing and handling (private):

void Browser::UpdateToolbar(bool should_restore_state) {
  window_->UpdateToolbar(tab_strip_model_->GetActiveWebContents(),
                         should_restore_state);
}

void Browser::UpdateSearchState(WebContents* contents) {
  if (chrome::search::IsInstantExtendedAPIEnabled(profile_))
    search_delegate_->OnTabActivated(contents);
}

void Browser::ScheduleUIUpdate(const WebContents* source,
                               unsigned changed_flags) {
  if (!source)
    return;

  // Do some synchronous updates.
  if (changed_flags & content::INVALIDATE_TYPE_URL &&
      source == tab_strip_model_->GetActiveWebContents()) {
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

  for (UpdateMap::const_iterator i = scheduled_updates_.begin();
       i != scheduled_updates_.end(); ++i) {
    // Do not dereference |contents|, it may be out-of-date!
    const WebContents* contents = i->first;
    unsigned flags = i->second;

    if (contents == tab_strip_model_->GetActiveWebContents()) {
      // Updates that only matter when the tab is selected go here.

      if (flags & content::INVALIDATE_TYPE_PAGE_ACTIONS) {
        LocationBar* location_bar = window()->GetLocationBar();
        if (location_bar)
          location_bar->UpdatePageActions();
      }
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
      tab_strip_model_->UpdateWebContentsStateAt(
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

void Browser::SetAsDelegate(WebContents* web_contents, Browser* delegate) {
  // WebContents...
  web_contents->SetDelegate(delegate);

  // ...and all the helpers.
  BlockedContentTabHelper::FromWebContents(web_contents)->
      set_delegate(delegate);
  BookmarkTabHelper::FromWebContents(web_contents)->set_delegate(delegate);
  WebContentsModalDialogManager::FromWebContents(web_contents)->
      set_delegate(delegate);
  CoreTabHelper::FromWebContents(web_contents)->set_delegate(delegate);
  SearchEngineTabHelper::FromWebContents(web_contents)->set_delegate(delegate);
  ZoomController::FromWebContents(web_contents)->set_observer(delegate);
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

  SetAsDelegate(contents, NULL);
  RemoveScheduledUpdatesFor(contents);

  if (find_bar_controller_.get() && index == tab_strip_model_->active_index()) {
    find_bar_controller_->ChangeWebContents(NULL);
  }

  // Stop observing search model changes for this tab.
  search_delegate_->OnTabDetached(contents);

  registrar_.Remove(this, content::NOTIFICATION_INTERSTITIAL_ATTACHED,
                    content::Source<WebContents>(contents));
  registrar_.Remove(this, content::NOTIFICATION_INTERSTITIAL_DETACHED,
                    content::Source<WebContents>(contents));
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

    if (!is_app())
      features |= FEATURE_LOCATIONBAR;
  }
  return !!(features & feature);
}

void Browser::UpdateBookmarkBarState(BookmarkBarStateChangeReason reason) {
  BookmarkBar::State state;
  // The bookmark bar is hidden in fullscreen mode, unless on the new tab page.
  if (browser_defaults::bookmarks_enabled &&
      profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) &&
      !ShouldHideUIForFullscreen()) {
    state = BookmarkBar::SHOW;
  } else {
    WebContents* web_contents = tab_strip_model_->GetActiveWebContents();
    BookmarkTabHelper* bookmark_tab_helper =
        web_contents ? BookmarkTabHelper::FromWebContents(web_contents) : NULL;
    if (bookmark_tab_helper && bookmark_tab_helper->ShouldShowBookmarkBar())
      state = BookmarkBar::DETACHED;
    else
      state = BookmarkBar::HIDDEN;
  }

  // Don't allow the bookmark bar to be shown in suggestions mode.
#if !defined(OS_MACOSX)
  if (search_model_->mode().is_search_suggestions())
    state = BookmarkBar::HIDDEN;
#endif

  if (state == bookmark_bar_state_)
    return;

  bookmark_bar_state_ = state;

  // Inform NTP page of change in bookmark bar state, so that it can adjust the
  // theme image top offset if necessary.
  if (instant_controller_)
    instant_controller_->UpdateThemeInfo(true);

  if (!window_)
    return;  // This is called from the constructor when window_ is NULL.

  if (reason == BOOKMARK_BAR_STATE_CHANGE_TAB_SWITCH) {
    // Don't notify BrowserWindow on a tab switch as at the time this is invoked
    // BrowserWindow hasn't yet switched tabs. The BrowserWindow implementations
    // end up querying state once they process the tab switch.
    return;
  }

  bool shouldAnimate = reason == BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE;
  window_->BookmarkBarStateChanged(shouldAnimate ?
      BookmarkBar::ANIMATE_STATE_CHANGE :
      BookmarkBar::DONT_ANIMATE_STATE_CHANGE);
}

bool Browser::ShouldHideUIForFullscreen() const {
  // On Mac, fullscreen mode has most normal things (in a slide-down panel). On
  // other platforms, we hide some controls when in fullscreen mode.
#if defined(OS_MACOSX)
  return false;
#endif
  return window_ && window_->IsFullscreen();
}

bool Browser::MaybeCreateBackgroundContents(int route_id,
                                            WebContents* opener_web_contents,
                                            const string16& frame_name,
                                            const GURL& target_url) {
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
