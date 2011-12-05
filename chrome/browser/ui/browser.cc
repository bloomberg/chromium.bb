// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_started_animation.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_disabled_infobar_delegate.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_unload_handler.h"
#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"
#include "chrome/browser/intents/web_intents_registry_factory.h"
#include "chrome/browser/net/browser_url_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/restore_tab_helper.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/tab_closeable_state_watcher.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/tabs/tab_finder.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/app_modal_dialogs/message_box_handler.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tab_restore_service_delegate.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/global_error.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/ui/webui/bug_report_ui.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/options/content_settings_handler.h"
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
#include "content/browser/browser_url_handler.h"
#include "content/browser/child_process_security_policy.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/save_package.h"
#include "content/browser/host_zoom_map.h"
#include "content/browser/plugin_service.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/intents_host.h"
#include "content/public/common/content_restriction.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_zoom.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources_standard.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/animation/animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/point.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"

#if defined(OS_WIN)
#include "chrome/browser/autofill/autofill_ie_toolbar_import_win.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/base/win/shell.h"
#endif  // OS_WIN

#if defined(OS_MACOSX)
#include "content/browser/find_pasteboard.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/screen_lock_library.h"
#include "chrome/browser/ui/webui/active_downloads_ui.h"
#endif

#if !defined(OS_CHROMEOS) || defined(USE_AURA)
#include "chrome/browser/download/download_shelf.h"
#endif

#if defined(FILE_MANAGER_EXTENSION)
#include "chrome/browser/extensions/file_manager_util.h"
#endif

using base::TimeDelta;

///////////////////////////////////////////////////////////////////////////////

namespace {

// The URL to be loaded to display Help.
const char kHelpContentUrl[] =
#if defined(OS_CHROMEOS)
  #if defined(OFFICIAL_BUILD)
      "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
  #else
      "https://www.google.com/support/chromeos/";
  #endif
#else
    "https://www.google.com/support/chrome/";
#endif

// The URL to be opened when the Help link on the Autofill dialog is clicked.
const char kAutofillHelpUrl[] =
#if defined(OS_CHROMEOS)
    "https://www.google.com/support/chromeos/bin/answer.py?answer=142893";
#else
    "https://www.google.com/support/chrome/bin/answer.py?answer=142893";
#endif

// The URL to be loaded to display the "Report a broken page" form.
const char kBrokenPageUrl[] =
    "https://www.google.com/support/chrome/bin/request.py?contact_type="
    "broken_website&format=inproduct&p.page_title=$1&p.page_url=$2";

// The URL for the privacy dashboard.
const char kPrivacyDashboardUrl[] = "https://www.google.com/dashboard";

// How long we wait before updating the browser chrome while loading a page.
const int kUIUpdateCoalescingTimeMS = 200;

const char kHashMark[] = "#";

// Returns |true| if entry has an internal chrome:// URL, |false| otherwise.
bool HasInternalURL(const NavigationEntry* entry) {
  if (!entry)
    return false;

  // Check the |virtual_url()| first. This catches regular chrome:// URLs
  // including URLs that were rewritten (such as chrome://bookmarks).
  if (entry->virtual_url().SchemeIs(chrome::kChromeUIScheme))
    return true;

  // If the |virtual_url()| isn't a chrome:// URL, check if it's actually
  // view-source: of a chrome:// URL.
  if (entry->virtual_url().SchemeIs(chrome::kViewSourceScheme))
    return entry->url().SchemeIs(chrome::kChromeUIScheme);

  return false;
}

// Get the launch URL for a given extension, with optional override/fallback.
// |override_url|, if non-empty, will be preferred over the extension's
// launch url.
GURL UrlForExtension(const Extension* extension, const GURL& override_url) {
  if (!extension)
    return override_url;

  GURL url;
  if (!override_url.is_empty()) {
    DCHECK(extension->web_extent().MatchesURL(override_url));
    url = override_url;
  } else {
    url = extension->GetFullLaunchURL();
  }

  // For extensions lacking launch urls, determine a reasonable fallback.
  if (!url.is_valid()) {
    url = extension->options_url();
    if (!url.is_valid()) {
      url = GURL(std::string(chrome::kChromeUISettingsURL) +
                 chrome::kExtensionsSubPage);
    }
  }

  return url;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Browser, CreateParams:

Browser::CreateParams::CreateParams(Type type, Profile* profile)
    : type(type),
      profile(profile) {
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Constructors, Creation, Showing:

Browser::Browser(Type type, Profile* profile)
    : type_(type),
      profile_(profile),
      window_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          tab_handler_(TabHandler::CreateTabHandler(this))),
      command_updater_(this),
      toolbar_model_(this),
      chrome_updater_factory_(this),
      is_attempting_to_close_browser_(false),
      cancel_download_confirmation_state_(NOT_PROMPTED),
      show_state_(ui::SHOW_STATE_DEFAULT),
      is_session_restore_(false),
      weak_factory_(this),
      block_command_execution_(false),
      last_blocked_command_id_(-1),
      last_blocked_command_disposition_(CURRENT_TAB),
      pending_web_app_action_(NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          tab_restore_service_delegate_(
              new BrowserTabRestoreServiceDelegate(this))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          synced_window_delegate_(
              new BrowserSyncedWindowDelegate(this))),
      bookmark_bar_state_(BookmarkBar::HIDDEN),
      window_has_shown_(false) {
  registrar_.Add(this, content::NOTIFICATION_SSL_VISIBLE_STATE_CHANGED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());
  registrar_.Add(
      this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile_)));
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENT_SETTINGS_CHANGED,
                 content::NotificationService::AllSources());

  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    local_pref_registrar_.Init(local_state);
    local_pref_registrar_.Add(prefs::kPrintingEnabled, this);
    local_pref_registrar_.Add(prefs::kAllowFileSelectionDialogs, this);
    local_pref_registrar_.Add(prefs::kMetricsReportingEnabled, this);
  }

  profile_pref_registrar_.Init(profile_->GetPrefs());
  profile_pref_registrar_.Add(prefs::kDevToolsDisabled, this);
  profile_pref_registrar_.Add(prefs::kEditBookmarksEnabled, this);
  profile_pref_registrar_.Add(prefs::kShowBookmarkBar, this);
  profile_pref_registrar_.Add(prefs::kHomePage, this);
  profile_pref_registrar_.Add(prefs::kInstantEnabled, this);
  profile_pref_registrar_.Add(prefs::kIncognitoModeAvailability, this);
  profile_pref_registrar_.Add(prefs::kSearchSuggestEnabled, this);

  InitCommandState();
  BrowserList::AddBrowser(this);

  // NOTE: These prefs all need to be explicitly destroyed in the destructor
  // or you'll get a nasty surprise when you run the incognito tests.
  encoding_auto_detect_.Init(prefs::kWebKitUsesUniversalDetector,
                             profile_->GetPrefs(), NULL);

  tab_restore_service_ = TabRestoreServiceFactory::GetForProfile(profile);
  if (tab_restore_service_) {
    tab_restore_service_->AddObserver(this);
    TabRestoreServiceChanged(tab_restore_service_);
  }

  if (profile_->GetProfileSyncService())
    profile_->GetProfileSyncService()->AddObserver(this);

  CreateInstantIfNecessary();

  // Make sure TabFinder has been created. This does nothing if TabFinder is
  // not enabled.
  TabFinder::GetInstance();

  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_INIT);
}

Browser::~Browser() {
  if (profile_->GetProfileSyncService())
    profile_->GetProfileSyncService()->RemoveObserver(this);

  BrowserList::RemoveBrowser(this);

#if !defined(OS_MACOSX)
  if (!BrowserList::HasBrowserWithProfile(profile_)) {
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

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_);
  if (session_service)
    session_service->WindowClosed(session_id_);

  TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(profile());
  if (tab_restore_service)
    tab_restore_service->BrowserClosed(tab_restore_service_delegate());

  profile_pref_registrar_.RemoveAll();
  local_pref_registrar_.RemoveAll();

  encoding_auto_detect_.Destroy();

  if (profile_->IsOffTheRecord() &&
      !BrowserList::IsOffTheRecordSessionActiveForProfile(profile_)) {
    // An incognito profile is no longer needed, this indirectly
    // frees its cache and cookies.
    profile_->GetOriginalProfile()->DestroyOffTheRecordProfile();
  }

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  TabRestoreServiceDestroyed(tab_restore_service_);
}

bool Browser::IsFullscreenForTab() const {
  return fullscreen_controller_->IsFullscreenForTab();
}

// static
Browser* Browser::Create(Profile* profile) {
  Browser* browser = new Browser(TYPE_TABBED, profile);
  browser->InitBrowserWindow();
  return browser;
}

// static
Browser* Browser::CreateWithParams(const CreateParams& params) {
  Browser* browser = new Browser(params.type, params.profile);
  browser->app_name_ = params.app_name;
  browser->set_override_bounds(params.initial_bounds);
  browser->InitBrowserWindow();
  return browser;
}

// static
Browser* Browser::CreateForType(Type type, Profile* profile) {
  CreateParams params(type, profile);
  return CreateWithParams(params);
}

// static
Browser* Browser::CreateForApp(Type type,
                               const std::string& app_name,
                               const gfx::Rect& window_bounds,
                               Profile* profile) {
  DCHECK(type != TYPE_TABBED);
  DCHECK(!app_name.empty());

  RegisterAppPrefs(app_name, profile);

  if (type == TYPE_PANEL &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisablePanels)) {
    type = TYPE_POPUP;
  }
#if defined(TOOLKIT_GTK)
  // Panels are only supported on a white list of window managers for Linux.
  if (type == TYPE_PANEL) {
    ui::WindowManagerName wm_type = ui::GuessWindowManager();
    if (wm_type != ui::WM_COMPIZ &&
        wm_type != ui::WM_ICE_WM &&
        wm_type != ui::WM_KWIN &&
        wm_type != ui::WM_METACITY &&
        wm_type != ui::WM_MUTTER) {
      type = TYPE_POPUP;
    }
  }
#endif  // TOOLKIT_GTK

  CreateParams params(type, profile);
  params.app_name = app_name;
  params.initial_bounds = window_bounds;
  return CreateWithParams(params);
}

// static
Browser* Browser::CreateForDevTools(Profile* profile) {
#if defined(OS_CHROMEOS)
  CreateParams params(TYPE_TABBED, profile);
#else
  CreateParams params(TYPE_POPUP, profile);
#endif
  params.app_name = DevToolsWindow::kDevToolsApp;
  return CreateWithParams(params);
}

void Browser::InitBrowserWindow() {
  DCHECK(!window_);

  window_ = CreateBrowserWindow();
  fullscreen_controller_ = new FullscreenController(window_, profile_, this);

#if defined(OS_WIN) && !defined(USE_AURA)
  {
    // TODO: This might hit the disk
    //   http://code.google.com/p/chromium/issues/detail?id=61638
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    // Set the app user model id for this application to that of the application
    // name.  See http://crbug.com/7028.
    ui::win::SetAppIdForWindow(
        is_app() && !is_type_panel() ?
        ShellIntegration::GetAppId(UTF8ToWide(app_name_), profile_->GetPath()) :
        ShellIntegration::GetChromiumAppId(profile_->GetPath()),
        window()->GetNativeHandle());

    if (is_type_panel()) {
      ui::win::SetAppIconForWindow(
          ShellIntegration::GetChromiumIconPath(),
          window()->GetNativeHandle());
    }
  }
#endif

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

  // Permanently dismiss ntp4 bubble for new users.
  if (FirstRun::IsChromeFirstRun())
    NewTabPageHandler::DismissIntroMessage(local_state);
}

///////////////////////////////////////////////////////////////////////////////
// Getters & Setters

FindBarController* Browser::GetFindBarController() {
  if (!find_bar_controller_.get()) {
    FindBar* find_bar = window_->CreateFindBar();
    find_bar_controller_.reset(new FindBarController(find_bar));
    find_bar->SetFindBarController(find_bar_controller_.get());
    find_bar_controller_->ChangeTabContents(GetSelectedTabContentsWrapper());
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
// Browser, Creation Helpers:

// static
void Browser::OpenEmptyWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->AddBlankTab(true);
  browser->window()->Show();
}

// static
void Browser::OpenWindowWithRestoredTabs(Profile* profile) {
  TabRestoreService* service = TabRestoreServiceFactory::GetForProfile(profile);
  if (service)
    service->RestoreMostRecentEntry(NULL);
}

// static
void Browser::OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  Browser* browser = GetOrCreateTabbedBrowser(
      profile->GetOffTheRecordProfile());
  browser->AddSelectedTabWithURL(url, content::PAGE_TRANSITION_LINK);
  browser->window()->Show();
}

// static
TabContents* Browser::OpenApplication(
    Profile* profile,
    const Extension* extension,
    extension_misc::LaunchContainer container,
    const GURL& override_url,
    WindowOpenDisposition disposition) {
  TabContents* tab = NULL;
  ExtensionPrefs* prefs = profile->GetExtensionService()->extension_prefs();
  prefs->SetActiveBit(extension->id(), true);

  UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunchContainer", container, 100);

  switch (container) {
    case extension_misc::LAUNCH_WINDOW:
    case extension_misc::LAUNCH_PANEL:
    case extension_misc::LAUNCH_SHELL:
      tab = Browser::OpenApplicationWindow(profile, extension, container,
                                           override_url, NULL);
      break;
    case extension_misc::LAUNCH_TAB: {
      tab = Browser::OpenApplicationTab(profile, extension, override_url,
                                        disposition);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
  return tab;
}

// static
TabContents* Browser::OpenApplicationWindow(
    Profile* profile,
    const Extension* extension,
    extension_misc::LaunchContainer container,
    const GURL& url_input,
    Browser** app_browser) {
  DCHECK(!url_input.is_empty() || extension);
  GURL url = UrlForExtension(extension, url_input);

  std::string app_name;
  app_name = extension ?
      web_app::GenerateApplicationNameFromExtensionId(extension->id()) :
      web_app::GenerateApplicationNameFromURL(url);

  Type type = TYPE_POPUP;
  if (extension) {
    switch (container) {
      case extension_misc::LAUNCH_PANEL:
        type = TYPE_PANEL;
        break;
      case extension_misc::LAUNCH_SHELL:
        type = TYPE_SHELL;
        break;
      default: break;
    }
  }

  gfx::Rect window_bounds;
  if (extension) {
    window_bounds.set_width(extension->launch_width());
    window_bounds.set_height(extension->launch_height());
  }

  Browser* browser = Browser::CreateForApp(type, app_name, window_bounds,
                                           profile);

  if (app_browser)
    *app_browser = browser;

  TabContentsWrapper* wrapper =
      browser->AddSelectedTabWithURL(url, content::PAGE_TRANSITION_START_PAGE);
  TabContents* contents = wrapper->tab_contents();
  contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  contents->render_view_host()->SyncRendererPrefs();
  browser->window()->Show();

  // TODO(jcampan): http://crbug.com/8123 we should not need to set the initial
  //                focus explicitly.
  contents->view()->SetInitialFocus();
  return contents;
}

TabContents* Browser::OpenAppShortcutWindow(Profile* profile,
                                            const GURL& url,
                                            bool update_shortcut) {
  Browser* app_browser;
  TabContents* tab = OpenApplicationWindow(
      profile,
      NULL,  // this is a URL app.  No extension.
      extension_misc::LAUNCH_WINDOW,
      url,
      &app_browser);

  if (!tab)
    return NULL;

  if (update_shortcut) {
    // Set UPDATE_SHORTCUT as the pending web app action. This action is picked
    // up in LoadingStateChanged to schedule a GetApplicationInfo. And when
    // the web app info is available, TabContents notifies Browser via
    // OnDidGetApplicationInfo, which calls
    // web_app::UpdateShortcutForTabContents when it sees UPDATE_SHORTCUT as
    // pending web app action.
    app_browser->pending_web_app_action_ = UPDATE_SHORTCUT;
  }
  return tab;
}

// static
TabContents* Browser::OpenApplicationTab(Profile* profile,
                                         const Extension* extension,
                                         const GURL& override_url,
                                         WindowOpenDisposition disposition) {
  Browser* browser = BrowserList::FindTabbedBrowser(profile, false);
  TabContents* contents = NULL;
  if (!browser) {
    // No browser for this profile, need to open a new one.
    browser = Browser::Create(profile);
    browser->window()->Show();
    // There's no current tab in this browser window, so add a new one.
    disposition = NEW_FOREGROUND_TAB;
  }

  // Check the prefs for overridden mode.
  ExtensionService* extension_service = profile->GetExtensionService();
  DCHECK(extension_service);

  ExtensionPrefs::LaunchType launch_type =
      extension_service->extension_prefs()->GetLaunchType(
          extension->id(), ExtensionPrefs::LAUNCH_DEFAULT);
  UMA_HISTOGRAM_ENUMERATION("Extensions.AppTabLaunchType", launch_type, 100);

  static bool default_apps_trial_exists =
      base::FieldTrialList::TrialExists(kDefaultAppsTrial_Name);
  if (default_apps_trial_exists) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("Extensions.AppTabLaunchType",
                                   kDefaultAppsTrial_Name),
        launch_type, 100);
  }

  int add_type = TabStripModel::ADD_ACTIVE;
  if (launch_type == ExtensionPrefs::LAUNCH_PINNED)
    add_type |= TabStripModel::ADD_PINNED;

  GURL extension_url = UrlForExtension(extension, override_url);
  // TODO(erikkay): START_PAGE doesn't seem like the right transition in all
  // cases.
  browser::NavigateParams params(browser, extension_url,
                                 content::PAGE_TRANSITION_START_PAGE);
  params.tabstrip_add_types = add_type;
  params.disposition = disposition;

  if (disposition == CURRENT_TAB) {
    TabContents* existing_tab = browser->GetSelectedTabContents();
    TabStripModel* model = browser->tabstrip_model();
    int tab_index = model->GetWrapperIndex(existing_tab);

    existing_tab->OpenURL(OpenURLParams(
          extension_url,
          content::Referrer(existing_tab->GetURL(),
                            WebKit::WebReferrerPolicyDefault),
          disposition, content::PAGE_TRANSITION_LINK, false));
    if (params.tabstrip_add_types & TabStripModel::ADD_PINNED) {
      model->SetTabPinned(tab_index, true);
      tab_index = model->GetWrapperIndex(existing_tab);
    }
    if (params.tabstrip_add_types & TabStripModel::ADD_ACTIVE)
      model->ActivateTabAt(tab_index, true);

    contents = existing_tab;
  } else {
    browser::Navigate(&params);
    contents = params.target_contents->tab_contents();
  }

  // TODO(skerner):  If we are already in full screen mode, and the user
  // set the app to open as a regular or pinned tab, what should happen?
  // Today we open the tab, but stay in full screen mode.  Should we leave
  // full screen mode in this case?
  if (launch_type == ExtensionPrefs::LAUNCH_FULLSCREEN &&
      !browser->window()->IsFullscreen()) {
    browser->ToggleFullscreenMode(false);
  }

  return contents;
}

// static
void Browser::OpenBookmarkManagerWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->OpenBookmarkManager();
  browser->window()->Show();
}

#if defined(OS_MACOSX)
// static
void Browser::OpenHistoryWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->ShowHistoryTab();
  browser->window()->Show();
}

// static
void Browser::OpenDownloadsWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->ShowDownloadsTab();
  browser->window()->Show();
}

// static
void Browser::OpenHelpWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->ShowHelpTab();
  browser->window()->Show();
}

// static
void Browser::OpenOptionsWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->OpenOptionsDialog();
  browser->window()->Show();
}

// static
void Browser::OpenClearBrowsingDataDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->OpenClearBrowsingDataDialog();
  browser->window()->Show();
}

// static
void Browser::OpenImportSettingsDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->OpenImportSettingsDialog();
  browser->window()->Show();
}

// static
void Browser::OpenInstantConfirmDialogWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->OpenInstantConfirmDialog();
  browser->window()->Show();
}
#endif

// static
void Browser::OpenExtensionsWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->ShowExtensionsTab();
  browser->window()->Show();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, State Storage and Retrieval for UI:

std::string Browser::GetWindowPlacementKey() const {
  std::string name(prefs::kBrowserWindowPlacement);
  if (!app_name_.empty()) {
    name.append("_");
    name.append(app_name_);
  }
  return name;
}

bool Browser::ShouldSaveWindowPlacement() const {
  switch (type_) {
    case TYPE_TABBED:
      return true;
    case TYPE_POPUP:
      // Only save the window placement of popups if they are restored,
      // or the window belongs to DevTools.
      return browser_defaults::kRestorePopups || is_devtools();
    case TYPE_PANEL:
      // Do not save the window placement of panels.
      return false;
    case TYPE_SHELL:
      return true;
    default:
      return false;
  }
}

void Browser::SaveWindowPlacement(const gfx::Rect& bounds,
                                  ui::WindowShowState show_state) {
  // Save to the session storage service, used when reloading a past session.
  // Note that we don't want to be the ones who cause lazy initialization of
  // the session service. This function gets called during initial window
  // showing, and we don't want to bring in the session service this early.
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile());
  if (session_service)
    session_service->SetWindowBounds(session_id_, bounds, show_state);
}

gfx::Rect Browser::GetSavedWindowBounds() const {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  bool record_mode = parsed_command_line.HasSwitch(switches::kRecordMode);
  bool playback_mode = parsed_command_line.HasSwitch(switches::kPlaybackMode);
  if (record_mode || playback_mode) {
    // In playback/record mode we always fix the size of the browser and
    // move it to (0,0).  The reason for this is two reasons:  First we want
    // resize/moves in the playback to still work, and Second we want
    // playbacks to work (as much as possible) on machines w/ different
    // screen sizes.
    return gfx::Rect(0, 0, 800, 600);
  }

  gfx::Rect restored_bounds = override_bounds_;
  WindowSizer::GetBrowserWindowBounds(app_name_, restored_bounds, this,
                                      &restored_bounds);
  return restored_bounds;
}

ui::WindowShowState Browser::GetSavedWindowShowState() const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kStartMaximized))
    return ui::SHOW_STATE_MAXIMIZED;

  if (show_state_ != ui::SHOW_STATE_DEFAULT)
    return show_state_;

  const DictionaryValue* window_pref =
      profile()->GetPrefs()->GetDictionary(GetWindowPlacementKey().c_str());
  bool maximized = false;
  window_pref->GetBoolean("maximized", &maximized);

  return maximized ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_NORMAL;
}

SkBitmap Browser::GetCurrentPageIcon() const {
  TabContentsWrapper* contents = GetSelectedTabContentsWrapper();
  // |contents| can be NULL since GetCurrentPageIcon() is called by the window
  // during the window's creation (before tabs have been added).
  return contents ? contents->favicon_tab_helper()->GetFavicon() : SkBitmap();
}

string16 Browser::GetWindowTitleForCurrentTab() const {
  TabContents* contents = GetSelectedTabContents();
  string16 title;

  // |contents| can be NULL because GetWindowTitleForCurrentTab is called by the
  // window during the window's creation (before tabs have been added).
  if (contents) {
    title = contents->GetTitle();
    FormatTitleForDisplay(&title);
  }
  if (title.empty())
    title = TabContentsWrapper::GetDefaultTitle();

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  // On Mac or ChromeOS, we don't want to suffix the page title with
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

bool Browser::TabsNeedBeforeUnloadFired() {
  if (tabs_needing_before_unload_fired_.empty()) {
    for (int i = 0; i < tab_count(); ++i) {
      TabContents* contents = GetTabContentsAt(i);
      if (contents->NeedToFireBeforeUnload())
        tabs_needing_before_unload_fired_.insert(contents);
    }
  }
  return !tabs_needing_before_unload_fired_.empty();
}

bool Browser::ShouldCloseWindow() {
  if (!CanCloseWithInProgressDownloads())
    return false;

  if (HasCompletedUnloadProcessing())
    return IsClosingPermitted();

  is_attempting_to_close_browser_ = true;

  if (!TabsNeedBeforeUnloadFired())
    return IsClosingPermitted();

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
      browser_shutdown::IsTryingToQuit() || !BrowserList::WillKeepAlive();

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
  TabContents* contents = GetSelectedTabContents();
  if (contents && contents->crashed_status() ==
     base::TERMINATION_STATUS_PROCESS_WAS_KILLED) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kReloadKilledTabs)) {
      Reload(CURRENT_TAB);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// In-progress download termination handling:

void Browser::InProgressDownloadResponse(bool cancel_downloads) {
  if (cancel_downloads) {
    cancel_download_confirmation_state_ = RESPONSE_RECEIVED;
    CloseWindow();
    return;
  }

  // Sets the confirmation state to NOT_PROMPTED so that if the user tries to
  // close again we'll show the warning again.
  cancel_download_confirmation_state_ = NOT_PROMPTED;

  // Show the download page so the user can figure-out what downloads are still
  // in-progress.
  ShowDownloadsTab();
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
  return tab_handler_->GetTabStripModel()->count();
}

int Browser::active_index() const {
  return tab_handler_->GetTabStripModel()->active_index();
}

int Browser::GetIndexOfController(
    const NavigationController* controller) const {
  return tab_handler_->GetTabStripModel()->GetIndexOfController(controller);
}

TabContentsWrapper* Browser::GetSelectedTabContentsWrapper() const {
  return tabstrip_model()->GetActiveTabContents();
}

TabContentsWrapper* Browser::GetTabContentsWrapperAt(int index) const {
  return tabstrip_model()->GetTabContentsAt(index);
}

TabContents* Browser::GetSelectedTabContents() const {
  TabContentsWrapper* wrapper = GetSelectedTabContentsWrapper();
  if (wrapper)
    return wrapper->tab_contents();
  return NULL;
}

TabContents* Browser::GetTabContentsAt(int index) const {
  TabContentsWrapper* wrapper = tabstrip_model()->GetTabContentsAt(index);
  if (wrapper)
    return wrapper->tab_contents();
  return NULL;
}

void Browser::ActivateTabAt(int index, bool user_gesture) {
  tab_handler_->GetTabStripModel()->ActivateTabAt(index, user_gesture);
}

bool Browser::IsTabPinned(int index) const {
  return tabstrip_model()->IsTabPinned(index);
}

bool Browser::IsTabDiscarded(int index) const {
  return tab_handler_->GetTabStripModel()->IsTabDiscarded(index);
}

void Browser::CloseAllTabs() {
  tab_handler_->GetTabStripModel()->CloseAllTabs();
}

////////////////////////////////////////////////////////////////////////////////
// Browser, Tab adding/showing functions:

bool Browser::IsTabStripEditable() const {
  return window()->IsTabStripEditable();
}

int Browser::GetIndexForInsertionDuringRestore(int relative_index) {
  return (tab_handler_->GetTabStripModel()->insertion_policy() ==
      TabStripModel::INSERT_AFTER) ? tab_count() : relative_index;
}

TabContentsWrapper* Browser::AddSelectedTabWithURL(
    const GURL& url,
    content::PageTransition transition) {
  browser::NavigateParams params(this, url, transition);
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
  return params.target_contents;
}

TabContents* Browser::AddTab(TabContentsWrapper* tab_contents,
                             content::PageTransition type) {
  tab_handler_->GetTabStripModel()->AddTabContents(
      tab_contents, -1, type, TabStripModel::ADD_ACTIVE);
  return tab_contents->tab_contents();
}

void Browser::AddTabContents(TabContents* new_contents,
                             WindowOpenDisposition disposition,
                             const gfx::Rect& initial_pos,
                             bool user_gesture) {
  AddNewContents(NULL, new_contents, disposition, initial_pos, user_gesture);
}

void Browser::CloseTabContents(TabContents* contents) {
  CloseContents(contents);
}

gfx::NativeWindow Browser::BrowserShowHtmlDialog(
    HtmlDialogUIDelegate* delegate,
    gfx::NativeWindow parent_window,
    DialogStyle style) {
#if defined(OS_CHROMEOS)
  // For Chrome OS, first try to parent the dialog over the current browser --
  // it's likely to be maximized onscreen.  If it isn't tabbed (e.g. it's a
  // panel), find a browser that is.
  parent_window = window_->GetNativeHandle();
  if (!is_type_tabbed()) {
    Browser* tabbed_browser = BrowserList::FindTabbedBrowser(profile_, true);
    if (tabbed_browser && tabbed_browser->window())
      parent_window = tabbed_browser->window()->GetNativeHandle();
  }
#endif  // defined(OS_CHROMEOS)
  if (!parent_window)
    parent_window = window_->GetNativeHandle();

  return browser::ShowHtmlDialog(parent_window, profile_, delegate, style);
}

void Browser::BrowserRenderWidgetShowing() {
  RenderWidgetShowing();
}

void Browser::BookmarkBarSizeChanged(bool is_animating) {
  window_->ToolbarSizeChanged(is_animating);
}

TabContents* Browser::AddRestoredTab(
    const std::vector<TabNavigation>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    bool select,
    bool pin,
    bool from_last_session,
    SessionStorageNamespace* session_storage_namespace) {
  GURL restore_url = navigations.at(selected_navigation).virtual_url();
  TabContentsWrapper* wrapper = TabContentsFactory(
      profile(),
      tab_util::GetSiteInstanceForNewTab(NULL, profile_, restore_url),
      MSG_ROUTING_NONE,
      GetSelectedTabContents(),
      session_storage_namespace);
  TabContents* new_tab = wrapper->tab_contents();
  wrapper->extension_tab_helper()->SetExtensionAppById(extension_app_id);
  std::vector<NavigationEntry*> entries;
  TabNavigation::CreateNavigationEntriesFromTabNavigations(
      profile_, navigations, &entries);
  new_tab->controller().Restore(
      selected_navigation, from_last_session, &entries);
  DCHECK_EQ(0u, entries.size());

  int add_types = select ? TabStripModel::ADD_ACTIVE :
      TabStripModel::ADD_NONE;
  if (pin) {
    tab_index = std::min(tab_index, tabstrip_model()->IndexOfFirstNonMiniTab());
    add_types |= TabStripModel::ADD_PINNED;
  }
  tab_handler_->GetTabStripModel()->InsertTabContentsAt(tab_index, wrapper,
                                                        add_types);
  if (select) {
    window_->Activate();
  } else {
    // We set the size of the view here, before WebKit does its initial
    // layout.  If we don't, the initial layout of background tabs will be
    // performed with a view width of 0, which may cause script outputs and
    // anchor link location calculations to be incorrect even after a new
    // layout with proper view dimensions. TabStripModel::AddTabContents()
    // contains similar logic.
    new_tab->view()->SizeContents(window_->GetRestoredBounds().size());
    new_tab->HideContents();
  }
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile_);
  if (session_service)
    session_service->TabRestored(wrapper, pin);
  return new_tab;
}

void Browser::ReplaceRestoredTab(
    const std::vector<TabNavigation>& navigations,
    int selected_navigation,
    bool from_last_session,
    const std::string& extension_app_id,
    SessionStorageNamespace* session_storage_namespace) {
  GURL restore_url = navigations.at(selected_navigation).virtual_url();
  TabContentsWrapper* wrapper = TabContentsFactory(
      profile(),
      tab_util::GetSiteInstanceForNewTab(NULL, profile_, restore_url),
      MSG_ROUTING_NONE,
      GetSelectedTabContents(),
      session_storage_namespace);
  wrapper->extension_tab_helper()->SetExtensionAppById(extension_app_id);
  TabContents* replacement = wrapper->tab_contents();
  std::vector<NavigationEntry*> entries;
  TabNavigation::CreateNavigationEntriesFromTabNavigations(
      profile_, navigations, &entries);
  replacement->controller().Restore(
      selected_navigation, from_last_session, &entries);
  DCHECK_EQ(0u, entries.size());

  tab_handler_->GetTabStripModel()->ReplaceNavigationControllerAt(
      tab_handler_->GetTabStripModel()->active_index(),
      wrapper);
}

bool Browser::CanRestoreTab() {
  return command_updater_.IsCommandEnabled(IDC_RESTORE_TAB);
}

bool Browser::NavigateToIndexWithDisposition(int index,
                                             WindowOpenDisposition disp) {
  NavigationController& controller =
      GetOrCloneTabForDisposition(disp)->controller();
  if (index < 0 || index >= controller.entry_count())
    return false;
  controller.GoToIndex(index);
  return true;
}

browser::NavigateParams Browser::GetSingletonTabNavigateParams(
    const GURL& url) {
  browser::NavigateParams params(
      this, url, content::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = SINGLETON_TAB;
  params.window_action = browser::NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;
  return params;
}

void Browser::ShowSingletonTab(const GURL& url) {
  browser::NavigateParams params(GetSingletonTabNavigateParams(url));
  browser::Navigate(&params);
}

void Browser::ShowSingletonTabRespectRef(const GURL& url) {
  browser::NavigateParams params(GetSingletonTabNavigateParams(url));
  params.ref_behavior = browser::NavigateParams::RESPECT_REF;
  browser::Navigate(&params);
}

void Browser::ShowSingletonTabOverwritingNTP(
    const browser::NavigateParams& params) {
  browser::NavigateParams local_params(params);
  TabContents* contents = GetSelectedTabContents();
  if (contents) {
    const GURL& contents_url = contents->GetURL();
    if ((contents_url == GURL(chrome::kChromeUINewTabURL) ||
         contents_url == GURL(chrome::kAboutBlankURL)) &&
        browser::GetIndexOfSingletonTab(&local_params) < 0) {
      local_params.disposition = CURRENT_TAB;
    }
  }

  browser::Navigate(&local_params);
}

void Browser::WindowFullscreenStateChanged() {
  fullscreen_controller_->WindowFullscreenStateChanged();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted browser commands:

TabContents* Browser::GetOrCloneTabForDisposition(
       WindowOpenDisposition disposition) {
  TabContentsWrapper* current_tab = GetSelectedTabContentsWrapper();
  switch (disposition) {
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB: {
      current_tab = current_tab->Clone();
      tab_handler_->GetTabStripModel()->AddTabContents(
          current_tab, -1, content::PAGE_TRANSITION_LINK,
          disposition == NEW_FOREGROUND_TAB ? TabStripModel::ADD_ACTIVE :
                                              TabStripModel::ADD_NONE);
      break;
    }
    case NEW_WINDOW: {
      current_tab = current_tab->Clone();
      Browser* browser = Browser::Create(profile_);
      browser->tabstrip_model()->AddTabContents(
          current_tab, -1, content::PAGE_TRANSITION_LINK,
          TabStripModel::ADD_ACTIVE);
      browser->window()->Show();
      break;
    }
    default:
      break;
  }
  return current_tab->tab_contents();
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

  unsigned int features = FEATURE_INFOBAR | FEATURE_SIDEBAR;

#if !defined(OS_CHROMEOS) || defined(USE_AURA)
  // Chrome OS opens a FileBrowse pop up instead of using download shelf.
  // So FEATURE_DOWNLOADSHELF is only added for non-chromeos platforms.
  features |= FEATURE_DOWNLOADSHELF;
#endif  // !defined(OS_CHROMEOS) || defined(USE_AURA)

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

bool Browser::IsClosingPermitted() {
  TabCloseableStateWatcher* watcher =
      g_browser_process->tab_closeable_state_watcher();
  bool can_close = !watcher || watcher->CanCloseBrowser(this);
  if (!can_close && is_attempting_to_close_browser_)
    CancelWindowClose();
  return can_close;
}

bool Browser::CanGoBack() const {
  return GetSelectedTabContentsWrapper()->controller().CanGoBack();
}

void Browser::GoBack(WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(UserMetricsAction("Back"));

  TabContentsWrapper* current_tab = GetSelectedTabContentsWrapper();
  if (CanGoBack()) {
    TabContents* new_tab = GetOrCloneTabForDisposition(disposition);
    // If we are on an interstitial page and clone the tab, it won't be copied
    // to the new tab, so we don't need to go back.
    if (current_tab->tab_contents()->showing_interstitial_page() &&
        (new_tab != current_tab->tab_contents()))
      return;
    new_tab->controller().GoBack();
  }
}

bool Browser::CanGoForward() const {
  return GetSelectedTabContentsWrapper()->controller().CanGoForward();
}

void Browser::GoForward(WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(UserMetricsAction("Forward"));
  if (CanGoForward())
    GetOrCloneTabForDisposition(disposition)->controller().GoForward();
}

void Browser::Reload(WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(UserMetricsAction("Reload"));
  ReloadInternal(disposition, false);
}

void Browser::ReloadIgnoringCache(WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(UserMetricsAction("ReloadIgnoringCache"));
  ReloadInternal(disposition, true);
}

void Browser::ReloadInternal(WindowOpenDisposition disposition,
                             bool ignore_cache) {
  // If we are showing an interstitial, treat this as an OpenURL.
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab && current_tab->showing_interstitial_page()) {
    NavigationEntry* entry = current_tab->controller().GetActiveEntry();
    DCHECK(entry);  // Should exist if interstitial is showing.
    OpenURL(entry->url(), GURL(), disposition, content::PAGE_TRANSITION_RELOAD);
    return;
  }

  // As this is caused by a user action, give the focus to the page.
  TabContents* tab = GetOrCloneTabForDisposition(disposition);
  if (!tab->FocusLocationBarByDefault())
    tab->Focus();
  if (ignore_cache)
    tab->controller().ReloadIgnoringCache(true);
  else
    tab->controller().Reload(true);
}

void Browser::Home(WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(UserMetricsAction("Home"));
  OpenURL(
      profile_->GetHomePage(), GURL(), disposition,
      content::PageTransitionFromInt(
          content::PAGE_TRANSITION_AUTO_BOOKMARK |
          content::PAGE_TRANSITION_HOME_PAGE));
}

void Browser::OpenCurrentURL() {
  UserMetrics::RecordAction(UserMetricsAction("LoadURL"));
  LocationBar* location_bar = window_->GetLocationBar();
  if (!location_bar)
    return;

  WindowOpenDisposition open_disposition =
      location_bar->GetWindowOpenDisposition();
  if (OpenInstant(open_disposition))
    return;

  GURL url(location_bar->GetInputString());

  if (open_disposition == CURRENT_TAB && TabFinder::IsEnabled()) {
    Browser* existing_browser = NULL;
    TabContents* existing_tab = TabFinder::GetInstance()->FindTab(
        this, url, &existing_browser);
    if (existing_tab) {
      existing_browser->ActivateContents(existing_tab);
      return;
    }
  }

  browser::NavigateParams params(this, url, location_bar->GetPageTransition());
  params.disposition = open_disposition;
  // Use ADD_INHERIT_OPENER so that all pages opened by the omnibox at least
  // inherit the opener. In some cases the tabstrip will determine the group
  // should be inherited, in which case the group is inherited instead of the
  // opener.
  params.tabstrip_add_types =
      TabStripModel::ADD_FORCE_INDEX | TabStripModel::ADD_INHERIT_OPENER;
  browser::Navigate(&params);

  DCHECK(profile_->GetExtensionService());
  if (profile_->GetExtensionService()->IsInstalledApp(url)) {
    UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppLaunchHistogram,
                              extension_misc::APP_LAUNCH_OMNIBOX_LOCATION,
                              extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
  }
}

void Browser::Stop() {
  UserMetrics::RecordAction(UserMetricsAction("Stop"));
  GetSelectedTabContentsWrapper()->tab_contents()->Stop();
}

void Browser::NewWindow() {
  IncognitoModePrefs::Availability incognito_avail =
      IncognitoModePrefs::GetAvailability(profile_->GetPrefs());
  if (browser_defaults::kAlwaysOpenIncognitoWindow &&
      incognito_avail != IncognitoModePrefs::DISABLED &&
      (CommandLine::ForCurrentProcess()->HasSwitch(switches::kIncognito) ||
       incognito_avail == IncognitoModePrefs::FORCED)) {
    NewIncognitoWindow();
    return;
  }
  UserMetrics::RecordAction(UserMetricsAction("NewWindow"));
  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile_->GetOriginalProfile());
  if (!session_service ||
      !session_service->RestoreIfNecessary(std::vector<GURL>())) {
    Browser::OpenEmptyWindow(profile_->GetOriginalProfile());
  }
}

void Browser::NewIncognitoWindow() {
  if (IncognitoModePrefs::GetAvailability(profile_->GetPrefs()) ==
          IncognitoModePrefs::DISABLED) {
    NewWindow();
    return;
  }

  UserMetrics::RecordAction(UserMetricsAction("NewIncognitoWindow"));
  Browser::OpenEmptyWindow(profile_->GetOffTheRecordProfile());
}

void Browser::CloseWindow() {
  UserMetrics::RecordAction(UserMetricsAction("CloseWindow"));
  window_->Close();
}

void Browser::NewTab() {
  UserMetrics::RecordAction(UserMetricsAction("NewTab"));

  if (is_type_tabbed()) {
    AddBlankTab(true);
    GetSelectedTabContentsWrapper()->view()->RestoreFocus();
  } else {
    Browser* b = GetOrCreateTabbedBrowser(profile_);
    b->AddBlankTab(true);
    b->window()->Show();
    // The call to AddBlankTab above did not set the focus to the tab as its
    // window was not active, so we have to do it explicitly.
    // See http://crbug.com/6380.
    b->GetSelectedTabContentsWrapper()->view()->RestoreFocus();
  }
}

void Browser::CloseTab() {
  UserMetrics::RecordAction(UserMetricsAction("CloseTab_Accelerator"));
  if (CanCloseTab())
    tab_handler_->GetTabStripModel()->CloseSelectedTabs();
}

void Browser::SelectNextTab() {
  UserMetrics::RecordAction(UserMetricsAction("SelectNextTab"));
  tab_handler_->GetTabStripModel()->SelectNextTab();
}

void Browser::SelectPreviousTab() {
  UserMetrics::RecordAction(UserMetricsAction("SelectPrevTab"));
  tab_handler_->GetTabStripModel()->SelectPreviousTab();
}

void Browser::OpenTabpose() {
#if defined(OS_MACOSX)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableExposeForTabs)) {
    return;
  }

  UserMetrics::RecordAction(UserMetricsAction("OpenTabpose"));
  window()->OpenTabpose();
#else
  NOTREACHED();
#endif
}

void Browser::MoveTabNext() {
  UserMetrics::RecordAction(UserMetricsAction("MoveTabNext"));
  tab_handler_->GetTabStripModel()->MoveTabNext();
}

void Browser::MoveTabPrevious() {
  UserMetrics::RecordAction(UserMetricsAction("MoveTabPrevious"));
  tab_handler_->GetTabStripModel()->MoveTabPrevious();
}

void Browser::SelectNumberedTab(int index) {
  if (index < tab_count()) {
    UserMetrics::RecordAction(UserMetricsAction("SelectNumberedTab"));
    tab_handler_->GetTabStripModel()->ActivateTabAt(index, true);
  }
}

void Browser::SelectLastTab() {
  UserMetrics::RecordAction(UserMetricsAction("SelectLastTab"));
  tab_handler_->GetTabStripModel()->SelectLastTab();
}

void Browser::DuplicateTab() {
  UserMetrics::RecordAction(UserMetricsAction("Duplicate"));
  DuplicateContentsAt(active_index());
}

void Browser::RestoreTab() {
  UserMetrics::RecordAction(UserMetricsAction("RestoreTab"));
  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile_);
  if (!service)
    return;

  service->RestoreMostRecentEntry(tab_restore_service_delegate());
}

void Browser::WriteCurrentURLToClipboard() {
  // TODO(ericu): There isn't currently a metric for this.  Should there be?
  // We don't appear to track the action when it comes from the
  // RenderContextViewMenu.

  TabContents* contents = GetSelectedTabContents();
  if (!toolbar_model_.ShouldDisplayURL())
    return;

  chrome_browser_net::WriteURLToClipboard(
      contents->GetURL(),
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
      g_browser_process->clipboard());
}

void Browser::ConvertPopupToTabbedBrowser() {
  UserMetrics::RecordAction(UserMetricsAction("ShowAsTab"));
  int tab_strip_index = tab_handler_->GetTabStripModel()->active_index();
  TabContentsWrapper* contents =
      tab_handler_->GetTabStripModel()->DetachTabContentsAt(tab_strip_index);
  Browser* browser = Browser::Create(profile_);
  browser->tabstrip_model()->AppendTabContents(contents, true);
  browser->window()->Show();
}

// TODO(koz): Change |for_tab| to an enum.
void Browser::ToggleFullscreenMode(bool for_tab) {
  fullscreen_controller_->ToggleFullscreenMode(for_tab);
}

#if defined(OS_MACOSX)
void Browser::TogglePresentationMode(bool for_tab) {
  fullscreen_controller_->TogglePresentationMode(for_tab);
}
#endif

#if defined(OS_CHROMEOS)
void Browser::Search() {
  // Exit fullscreen to show omnibox.
  if (window_->IsFullscreen()) {
    ToggleFullscreenMode(false);
    // ToggleFullscreenMode is asynchronous, so we don't have omnibox
    // visible at this point. Wait for next event cycle which toggles
    // the visibility of omnibox before creating new tab.
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&Browser::Search, weak_factory_.GetWeakPtr()));
    return;
  }

  const GURL& url = GetSelectedTabContents()->GetURL();
  if (url.SchemeIs(chrome::kChromeUIScheme) &&
      url.host() == chrome::kChromeUINewTabHost) {
    // If the NTP is showing, focus the omnibox.
    window_->SetFocusToLocationBar(true);
  } else {
    // Otherwise, open the NTP.
    NewTab();
  }
}

void Browser::ShowKeyboardOverlay() {
  window_->ShowKeyboardOverlay(window_->GetNativeHandle());
}
#endif

void Browser::Exit() {
  UserMetrics::RecordAction(UserMetricsAction("Exit"));
  BrowserList::AttemptUserExit(false);
}

void Browser::BookmarkCurrentPage() {
  UserMetrics::RecordAction(UserMetricsAction("Star"));

  BookmarkModel* model = profile()->GetBookmarkModel();
  if (!model || !model->IsLoaded())
    return;  // Ignore requests until bookmarks are loaded.

  GURL url;
  string16 title;
  TabContentsWrapper* tab = GetSelectedTabContentsWrapper();
  bookmark_utils::GetURLAndTitleToBookmark(tab->tab_contents(), &url, &title);
  bool was_bookmarked = model->IsBookmarked(url);
  if (!was_bookmarked && profile_->IsOffTheRecord()) {
    // If we're incognito the favicon may not have been saved. Save it now
    // so that bookmarks have an icon for the page.
    tab->favicon_tab_helper()->SaveFavicon();
  }
  bookmark_utils::AddIfNotBookmarked(model, url, title);
  // Make sure the model actually added a bookmark before showing the star. A
  // bookmark isn't created if the url is invalid.
  if (window_->IsActive() && model->IsBookmarked(url)) {
    // Only show the bubble if the window is active, otherwise we may get into
    // weird situations were the bubble is deleted as soon as it is shown.
    window_->ShowBookmarkBubble(url, was_bookmarked);
  }
}

void Browser::SavePage() {
  UserMetrics::RecordAction(UserMetricsAction("SavePage"));
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab && current_tab->contents_mime_type() == "application/pdf")
    UserMetrics::RecordAction(UserMetricsAction("PDF.SavePage"));
  GetSelectedTabContents()->OnSavePage();
}

void Browser::ViewSelectedSource() {
  ViewSource(GetSelectedTabContentsWrapper());
}

void Browser::ShowFindBar() {
  GetFindBarController()->Show();
}

bool Browser::SupportsWindowFeature(WindowFeature feature) const {
  return SupportsWindowFeatureImpl(feature, true);
}

bool Browser::CanSupportWindowFeature(WindowFeature feature) const {
  return SupportsWindowFeatureImpl(feature, false);
}

void Browser::EmailPageLocation() {
  UserMetrics::RecordAction(UserMetricsAction("EmailPageLocation"));
  TabContents* tc = GetSelectedTabContents();
  DCHECK(tc);

  std::string title = net::EscapeQueryParamValue(
      UTF16ToUTF8(tc->GetTitle()), false);
  std::string page_url = net::EscapeQueryParamValue(tc->GetURL().spec(), false);
  std::string mailto = std::string("mailto:?subject=Fwd:%20") +
      title + "&body=%0A%0A" + page_url;
  platform_util::OpenExternal(GURL(mailto));
}

void Browser::Print() {
  if (switches::IsPrintPreviewEnabled())
    GetSelectedTabContentsWrapper()->print_view_manager()->PrintPreviewNow();
  else
    GetSelectedTabContentsWrapper()->print_view_manager()->PrintNow();
}

void Browser::AdvancedPrint() {
  GetSelectedTabContentsWrapper()->print_view_manager()->AdvancedPrintNow();
}

void Browser::ToggleEncodingAutoDetect() {
  UserMetrics::RecordAction(UserMetricsAction("AutoDetectChange"));
  encoding_auto_detect_.SetValue(!encoding_auto_detect_.GetValue());
  // If "auto detect" is turned on, then any current override encoding
  // is cleared. This also implicitly performs a reload.
  // OTOH, if "auto detect" is turned off, we don't change the currently
  // active encoding.
  if (encoding_auto_detect_.GetValue()) {
    TabContents* contents = GetSelectedTabContents();
    if (contents)
      contents->ResetOverrideEncoding();
  }
}

void Browser::OverrideEncoding(int encoding_id) {
  UserMetrics::RecordAction(UserMetricsAction("OverrideEncoding"));
  const std::string selected_encoding =
      CharacterEncoding::GetCanonicalEncodingNameByCommandId(encoding_id);
  TabContents* contents = GetSelectedTabContents();
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

void Browser::Cut() {
  UserMetrics::RecordAction(UserMetricsAction("Cut"));
  window()->Cut();
}

void Browser::Copy() {
  UserMetrics::RecordAction(UserMetricsAction("Copy"));
  window()->Copy();
}

void Browser::Paste() {
  UserMetrics::RecordAction(UserMetricsAction("Paste"));
  window()->Paste();
}

void Browser::Find() {
  UserMetrics::RecordAction(UserMetricsAction("Find"));
  FindInPage(false, false);
}

void Browser::FindNext() {
  UserMetrics::RecordAction(UserMetricsAction("FindNext"));
  FindInPage(true, true);
}

void Browser::FindPrevious() {
  UserMetrics::RecordAction(UserMetricsAction("FindPrevious"));
  FindInPage(true, false);
}

void Browser::Zoom(content::PageZoom zoom) {
  if (is_devtools())
    return;

  RenderViewHost* host = GetSelectedTabContentsWrapper()->render_view_host();
  if (zoom == content::PAGE_ZOOM_RESET) {
    host->SetZoomLevel(0);
    UserMetrics::RecordAction(UserMetricsAction("ZoomNormal"));
    return;
  }

  double current_zoom_level = GetSelectedTabContents()->GetZoomLevel();
  double default_zoom_level =
      profile_->GetPrefs()->GetDouble(prefs::kDefaultZoomLevel);

  // Generate a vector of zoom levels from an array of known presets along with
  // the default level added if necessary.
  std::vector<double> zoom_levels =
      chrome_page_zoom::PresetZoomLevels(default_zoom_level);

  if (zoom == content::PAGE_ZOOM_OUT) {
    // Iterate through the zoom levels in reverse order to find the next
    // lower level based on the current zoom level for this page.
    for (std::vector<double>::reverse_iterator i = zoom_levels.rbegin();
         i != zoom_levels.rend(); ++i) {
      double zoom_level = *i;
      if (content::ZoomValuesEqual(zoom_level, current_zoom_level))
        continue;
      if (zoom_level < current_zoom_level) {
        host->SetZoomLevel(zoom_level);
        UserMetrics::RecordAction(UserMetricsAction("ZoomMinus"));
        return;
      }
      UserMetrics::RecordAction(UserMetricsAction("ZoomMinus_AtMinimum"));
    }
  } else {
    // Iterate through the zoom levels in normal order to find the next
    // higher level based on the current zoom level for this page.
    for (std::vector<double>::const_iterator i = zoom_levels.begin();
         i != zoom_levels.end(); ++i) {
      double zoom_level = *i;
      if (content::ZoomValuesEqual(zoom_level, current_zoom_level))
        continue;
      if (zoom_level > current_zoom_level) {
        host->SetZoomLevel(zoom_level);
        UserMetrics::RecordAction(UserMetricsAction("ZoomPlus"));
        return;
      }
    }
    UserMetrics::RecordAction(UserMetricsAction("ZoomPlus_AtMaximum"));
  }
}

void Browser::FocusToolbar() {
  UserMetrics::RecordAction(UserMetricsAction("FocusToolbar"));
  window_->FocusToolbar();
}

void Browser::FocusAppMenu() {
  UserMetrics::RecordAction(UserMetricsAction("FocusAppMenu"));
  window_->FocusAppMenu();
}

void Browser::FocusLocationBar() {
  UserMetrics::RecordAction(UserMetricsAction("FocusLocation"));
  window_->SetFocusToLocationBar(true);
}

void Browser::FocusBookmarksToolbar() {
  UserMetrics::RecordAction(UserMetricsAction("FocusBookmarksToolbar"));
  window_->FocusBookmarksToolbar();
}

void Browser::FocusChromeOSStatus() {
  UserMetrics::RecordAction(UserMetricsAction("FocusChromeOSStatus"));
  window_->FocusChromeOSStatus();
}

void Browser::FocusNextPane() {
  UserMetrics::RecordAction(UserMetricsAction("FocusNextPane"));
  window_->RotatePaneFocus(true);
}

void Browser::FocusPreviousPane() {
  UserMetrics::RecordAction(UserMetricsAction("FocusPreviousPane"));
  window_->RotatePaneFocus(false);
}

void Browser::FocusSearch() {
  // TODO(beng): replace this with FocusLocationBar
  UserMetrics::RecordAction(UserMetricsAction("FocusSearch"));
  window_->GetLocationBar()->FocusSearch();
}

void Browser::OpenFile() {
  UserMetrics::RecordAction(UserMetricsAction("OpenFile"));
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  const FilePath directory = profile_->last_selected_directory();

  // TODO(beng): figure out how to juggle this.
  gfx::NativeWindow parent_window = window_->GetNativeHandle();
  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                                  string16(), directory,
                                  NULL, 0, FILE_PATH_LITERAL(""),
                                  GetSelectedTabContents(),
                                  parent_window, NULL);
}

void Browser::OpenCreateShortcutsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("CreateShortcut"));
#if !defined(OS_MACOSX)
  TabContentsWrapper* current_tab = GetSelectedTabContentsWrapper();
  DCHECK(current_tab &&
      web_app::IsValidUrl(current_tab->tab_contents()->GetURL())) <<
          "Menu item should be disabled.";

  NavigationEntry* entry = current_tab->controller().GetLastCommittedEntry();
  if (!entry)
    return;

  // RVH's GetApplicationInfo should not be called before it returns.
  DCHECK(pending_web_app_action_ == NONE);
  pending_web_app_action_ = CREATE_SHORTCUT;

  // Start fetching web app info for CreateApplicationShortcut dialog and show
  // the dialog when the data is available in OnDidGetApplicationInfo.
  current_tab->extension_tab_helper()->GetApplicationInfo(entry->page_id());
#else
  NOTIMPLEMENTED();
#endif
}

void Browser::ToggleDevToolsWindow(DevToolsToggleAction action) {
  if (action == DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE)
    UserMetrics::RecordAction(UserMetricsAction("DevTools_ToggleConsole"));
  else
    UserMetrics::RecordAction(UserMetricsAction("DevTools_ToggleWindow"));

  DevToolsWindow::ToggleDevToolsWindow(
      GetSelectedTabContentsWrapper()->render_view_host(), action);
}

void Browser::OpenTaskManager(bool highlight_background_resources) {
  UserMetrics::RecordAction(UserMetricsAction("TaskManager"));
  if (highlight_background_resources)
    window_->ShowBackgroundPages();
  else
    window_->ShowTaskManager();
}

void Browser::OpenBugReportDialog() {
  UserMetrics::RecordAction(UserMetricsAction("ReportBug"));
  browser::ShowHtmlBugReportView(this, std::string(), 0);
}

void Browser::ToggleBookmarkBar() {
  UserMetrics::RecordAction(UserMetricsAction("ShowBookmarksBar"));
  window_->ToggleBookmarkBar();
}

void Browser::OpenBookmarkManager() {
  UserMetrics::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  UserMetrics::RecordAction(UserMetricsAction("ShowBookmarks"));
  ShowSingletonTabOverwritingNTP(
      GetSingletonTabNavigateParams(GURL(chrome::kChromeUIBookmarksURL)));
}

void Browser::OpenBookmarkManagerWithHash(const std::string& action,
                                          int64 node_id) {
  UserMetrics::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  UserMetrics::RecordAction(UserMetricsAction("ShowBookmarks"));
  browser::NavigateParams params(GetSingletonTabNavigateParams(
      GURL(chrome::kChromeUIBookmarksURL).Resolve(
      StringPrintf("/#%s%s", action.c_str(),
      base::Int64ToString(node_id).c_str()))));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(params);
}

void Browser::OpenBookmarkManagerForNode(int64 node_id) {
  OpenBookmarkManagerWithHash("", node_id);
}

void Browser::OpenBookmarkManagerEditNode(int64 node_id) {
  OpenBookmarkManagerWithHash("e=", node_id);
}

void Browser::ShowAppMenu() {
  // We record the user metric for this event in WrenchMenu::RunMenu.
  window_->ShowAppMenu();
}

void Browser::ShowAvatarMenu() {
  window_->ShowAvatarBubbleFromAvatarButton();
}

void Browser::ShowHistoryTab() {
  UserMetrics::RecordAction(UserMetricsAction("ShowHistory"));
  ShowSingletonTabOverwritingNTP(
      GetSingletonTabNavigateParams(GURL(chrome::kChromeUIHistoryURL)));
}

void Browser::ShowDownloadsTab() {
  UserMetrics::RecordAction(UserMetricsAction("ShowDownloads"));
#if !defined(OS_CHROMEOS)
  // ChromiumOS uses ActiveDownloadsUI instead of of DownloadShelf.
  if (window()) {
    DownloadShelf* shelf = window()->GetDownloadShelf();
    if (shelf->IsShowing())
      shelf->Close();
  }
#endif
  ShowSingletonTabOverwritingNTP(
      GetSingletonTabNavigateParams(GURL(chrome::kChromeUIDownloadsURL)));
}

void Browser::ShowExtensionsTab() {
  UserMetrics::RecordAction(UserMetricsAction("ShowExtensions"));
  ShowOptionsTab(chrome::kExtensionsSubPage);
}

void Browser::ShowAboutConflictsTab() {
  UserMetrics::RecordAction(UserMetricsAction("AboutConflicts"));
  ShowSingletonTab(GURL(chrome::kChromeUIConflictsURL));
}

void Browser::ShowBrokenPageTab(TabContents* contents) {
  UserMetrics::RecordAction(UserMetricsAction("ReportBug"));
  string16 page_title = contents->GetTitle();
  NavigationEntry* entry = contents->controller().GetActiveEntry();
  if (!entry)
    return;
  std::string page_url = entry->url().spec();
  std::vector<std::string> subst;
  subst.push_back(UTF16ToASCII(page_title));
  subst.push_back(page_url);
  std::string report_page_url =
      ReplaceStringPlaceholders(kBrokenPageUrl, subst, NULL);
  ShowSingletonTab(GURL(report_page_url));
}

void Browser::ShowOptionsTab(const std::string& sub_page) {
  browser::NavigateParams params(GetSingletonTabNavigateParams(
      GURL(chrome::kChromeUISettingsURL + sub_page)));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;

  ShowSingletonTabOverwritingNTP(params);
}

void Browser::OpenClearBrowsingDataDialog() {
  UserMetrics::RecordAction(UserMetricsAction("ClearBrowsingData_ShowDlg"));
  ShowOptionsTab(chrome::kClearBrowserDataSubPage);
}

void Browser::OpenOptionsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("ShowOptions"));
  ShowOptionsTab("");
}

void Browser::OpenPasswordManager() {
  UserMetrics::RecordAction(UserMetricsAction("Options_ShowPasswordManager"));
  ShowOptionsTab(chrome::kPasswordManagerSubPage);
}

void Browser::OpenImportSettingsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("Import_ShowDlg"));
  ShowOptionsTab(chrome::kImportDataSubPage);
}

void Browser::OpenInstantConfirmDialog() {
  ShowOptionsTab(chrome::kInstantConfirmPage);
}

void Browser::OpenSyncMyBookmarksDialog() {
  sync_ui_util::OpenSyncMyBookmarksDialog(
      profile_, this, ProfileSyncService::START_FROM_WRENCH);
}

void Browser::OpenAboutChromeDialog() {
  UserMetrics::RecordAction(UserMetricsAction("AboutChrome"));
#if defined(OS_CHROMEOS)
  std::string chrome_settings(chrome::kChromeUISettingsURL);
  ShowSingletonTab(GURL(chrome_settings.append(chrome::kAboutOptionsSubPage)));
#else
  window_->ShowAboutChromeDialog();
#endif
}

void Browser::OpenUpdateChromeDialog() {
  UserMetrics::RecordAction(UserMetricsAction("UpdateChrome"));
  window_->ShowUpdateChromeDialog();
}

void Browser::ShowHelpTab() {
  UserMetrics::RecordAction(UserMetricsAction("ShowHelpTab"));
  GURL help_url(kHelpContentUrl);
  GURL localized_help_url = google_util::AppendGoogleLocaleParam(help_url);
  ShowSingletonTab(localized_help_url);
}

void Browser::OpenPrivacyDashboardTabAndActivate() {
  OpenURL(GURL(kPrivacyDashboardUrl), GURL(),
          NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK);
  window_->Activate();
}

void Browser::OpenAutofillHelpTabAndActivate() {
  GURL help_url = google_util::AppendGoogleLocaleParam(GURL(kAutofillHelpUrl));
  AddSelectedTabWithURL(help_url, content::PAGE_TRANSITION_LINK);
}

void Browser::OpenSearchEngineOptionsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("EditSearchEngines"));
  ShowOptionsTab(chrome::kSearchEnginesSubPage);
}

#if defined(FILE_MANAGER_EXTENSION)
void Browser::OpenFileManager() {
  UserMetrics::RecordAction(UserMetricsAction("OpenFileManager"));
  ShowSingletonTabRespectRef(GURL(chrome::kChromeUIFileManagerURL));
}
#endif

#if defined(OS_CHROMEOS)
void Browser::LockScreen() {
  UserMetrics::RecordAction(UserMetricsAction("LockScreen"));
  chromeos::CrosLibrary::Get()->GetScreenLockLibrary()->
      NotifyScreenLockRequested();
}

void Browser::OpenSystemOptionsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("OpenSystemOptionsDialog"));
  ShowOptionsTab(chrome::kSystemOptionsSubPage);
}

void Browser::OpenInternetOptionsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("OpenInternetOptionsDialog"));
  ShowOptionsTab(chrome::kInternetOptionsSubPage);
}

void Browser::OpenLanguageOptionsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("OpenLanguageOptionsDialog"));
  ShowOptionsTab(chrome::kLanguageOptionsSubPage);
}

void Browser::OpenSystemTabAndActivate() {
  OpenURL(GURL(chrome::kChromeUISystemInfoURL), GURL(),
          NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK);
  window_->Activate();
}

void Browser::OpenMobilePlanTabAndActivate() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMobileSetupDialog)) {
    window_->ShowMobileSetup();
  } else {
    OpenURL(GURL(chrome::kChromeUIMobileSetupURL), GURL(),
            NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK);
    window_->Activate();
  }
}
#endif

void Browser::OpenPluginsTabAndActivate() {
  OpenURL(GURL(chrome::kChromeUIPluginsURL), GURL(),
          NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK);
  window_->Activate();
}

///////////////////////////////////////////////////////////////////////////////

// static
void Browser::SetNewHomePagePrefs(PrefService* prefs) {
  const PrefService::Preference* home_page_pref =
      prefs->FindPreference(prefs::kHomePage);
  if (home_page_pref &&
      !home_page_pref->IsManaged() &&
      !prefs->HasPrefPath(prefs::kHomePage)) {
    prefs->SetString(prefs::kHomePage,
        GoogleURLTracker::kDefaultGoogleHomepage);
  }
  const PrefService::Preference* home_page_is_new_tab_page_pref =
      prefs->FindPreference(prefs::kHomePageIsNewTabPage);
  if (home_page_is_new_tab_page_pref &&
      !home_page_is_new_tab_page_pref->IsManaged() &&
      !prefs->HasPrefPath(prefs::kHomePageIsNewTabPage))
    prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
}

// static
void Browser::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kOptionsWindowLastTabIndex, 0);
  prefs->RegisterIntegerPref(prefs::kExtensionSidebarWidth, -1);
  prefs->RegisterBooleanPref(prefs::kAllowFileSelectionDialogs, true);
  prefs->RegisterBooleanPref(prefs::kShouldShowFirstRunBubble, false);
}

// static
void Browser::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kHomePage,
                            chrome::kChromeUINewTabURL,
                            PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kHomePageChanged,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kHomePageIsNewTabPage,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kShowHomeButton,
                             false,
                             PrefService::SYNCABLE_PREF);
#if defined(OS_MACOSX)
  // This really belongs in platform code, but there's no good place to
  // initialize it between the time when the AppController is created
  // (where there's no profile) and the time the controller gets another
  // crack at the start of the main event loop. By that time, BrowserInit
  // has already created the browser window, and it's too late: we need the
  // pref to be already initialized. Doing it here also saves us from having
  // to hard-code pref registration in the several unit tests that use
  // this preference.
  prefs->RegisterBooleanPref(prefs::kConfirmToQuitEnabled,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kShowUpdatePromotionInfoBar,
                             true,
                             PrefService::UNSYNCABLE_PREF);
#endif
  prefs->RegisterBooleanPref(prefs::kDeleteBrowsingHistory,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteDownloadHistory,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteCache,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteCookies,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeletePasswords,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDeleteFormData,
                             false,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kDeleteTimePeriod,
                             0,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kCheckDefaultBrowser,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kShowOmniboxSearchHint,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateOnDesktop,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateInAppsMenu,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateInQuickLaunchBar,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kEnableTranslate,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kCloudPrintEmail,
                            std::string(),
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kCloudPrintProxyEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kCloudPrintSubmitEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDevToolsDisabled,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kDevToolsSplitLocation,
                             -1,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kBrowserWindowPlacement,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kPreferencesWindowPlacement,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportBookmarks,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportHistory,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportHomepage,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportSearchEngine,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kImportSavedPasswords,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  // The map of timestamps of the last used file browser handlers.
  prefs->RegisterDictionaryPref(prefs::kLastUsedFileBrowserHandlers,
                                PrefService::UNSYNCABLE_PREF);

  // We need to register the type of these preferences in order to query
  // them even though they're only typically controlled via policy.
  prefs->RegisterBooleanPref(prefs::kDisable3DAPIs,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsAllowOutdated,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kPluginsAlwaysAuthorize,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kEnableHyperlinkAuditing,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kEnableReferrers,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kClearPluginLSODataEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);

  // Initialize the disk cache prefs.
  prefs->RegisterFilePathPref(prefs::kDiskCacheDir,
                              FilePath(),
                              PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kDiskCacheSize,
                             0,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kMediaCacheSize,
                             0,
                             PrefService::UNSYNCABLE_PREF);
}

// static
bool Browser::RunUnloadEventsHelper(TabContents* contents) {
  // If the TabContents is not connected yet, then there's no unload
  // handler we can fire even if the TabContents has an unload listener.
  // One case where we hit this is in a tab that has an infinite loop
  // before load.
  if (contents->NeedToFireBeforeUnload()) {
    // If the page has unload listeners, then we tell the renderer to fire
    // them. Once they have fired, we'll get a message back saying whether
    // to proceed closing the page or not, which sends us back to this method
    // with the NeedToFireBeforeUnload bit cleared.
    contents->render_view_host()->FirePageBeforeUnload(false);
    return true;
  }
  return false;
}

// static
Browser* Browser::GetBrowserForController(
    const NavigationController* controller, int* index_result) {
  BrowserList::const_iterator it;
  for (it = BrowserList::begin(); it != BrowserList::end(); ++it) {
    int index = (*it)->tab_handler_->GetTabStripModel()->GetIndexOfController(
        controller);
    if (index != TabStripModel::kNoTab) {
      if (index_result)
        *index_result = index;
      return *it;
    }
  }

  return NULL;
}

// static
void Browser::RunFileChooserHelper(
    TabContents* tab, const content::FileChooserParams& params) {
  Profile* profile =
      Profile::FromBrowserContext(tab->browser_context());
  // FileSelectHelper adds a reference to itself and only releases it after
  // sending the result message. It won't be destroyed when this reference
  // goes out of scope.
  scoped_refptr<FileSelectHelper> file_select_helper(
      new FileSelectHelper(profile));
  file_select_helper->RunFileChooser(tab->render_view_host(), tab, params);
}

// static
void Browser::EnumerateDirectoryHelper(TabContents* tab, int request_id,
                                       const FilePath& path) {
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  if (!policy->CanReadDirectory(tab->render_view_host()->process()->GetID(),
                                path)) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(tab->browser_context());
  // FileSelectHelper adds a reference to itself and only releases it after
  // sending the result message. It won't be destroyed when this reference
  // goes out of scope.
  scoped_refptr<FileSelectHelper> file_select_helper(
      new FileSelectHelper(profile));
  file_select_helper->EnumerateDirectory(request_id,
                                         tab->render_view_host(),
                                         path);
}

// static
void Browser::JSOutOfMemoryHelper(TabContents* tab) {
  TabContentsWrapper* tcw = TabContentsWrapper::GetCurrentWrapperForContents(
      tab);
  if (tcw) {
    InfoBarTabHelper* infobar_helper = tcw->infobar_tab_helper();
    infobar_helper->AddInfoBar(new SimpleAlertInfoBarDelegate(
        infobar_helper,
        NULL,
        l10n_util::GetStringUTF16(IDS_JS_OUT_OF_MEMORY_PROMPT),
        true));
  }
}

// static
void Browser::RegisterProtocolHandlerHelper(TabContents* tab,
                                            const std::string& protocol,
                                            const GURL& url,
                                            const string16& title) {
  TabContentsWrapper* tcw = TabContentsWrapper::GetCurrentWrapperForContents(
      tab);
  if (!tcw || tcw->profile()->IsOffTheRecord())
    return;

  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  if (policy->IsPseudoScheme(protocol) || policy->IsDisabledScheme(protocol))
    return;

  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(protocol, url, title);

  ProtocolHandlerRegistry* registry =
      tcw->profile()->GetProtocolHandlerRegistry();

  if (!registry->SilentlyHandleRegisterHandlerRequest(handler)) {
    UserMetrics::RecordAction(
        UserMetricsAction("RegisterProtocolHandler.InfoBar_Shown"));
    InfoBarTabHelper* infobar_helper = tcw->infobar_tab_helper();
    infobar_helper->AddInfoBar(
        new RegisterProtocolHandlerInfoBarDelegate(infobar_helper,
                                                   registry,
                                                   handler));
  }
}

// static
void Browser::RegisterIntentHandlerHelper(TabContents* tab,
                                          const string16& action,
                                          const string16& type,
                                          const string16& href,
                                          const string16& title,
                                          const string16& disposition) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableWebIntents))
    return;

  TabContentsWrapper* tcw = TabContentsWrapper::GetCurrentWrapperForContents(
      tab);
  if (!tcw || tcw->profile()->IsOffTheRecord())
    return;

  FaviconService* favicon_service =
      tcw->profile()->GetFaviconService(Profile::EXPLICIT_ACCESS);

  // |href| can be relative to originating URL. Resolve if necessary.
  GURL service_url(href);
  if (!service_url.is_valid()) {
    const GURL& url = tab->GetURL();
    service_url = url.Resolve(href);
  }

  webkit_glue::WebIntentServiceData service;
  service.service_url = service_url;
  service.action = action;
  service.type = type;
  service.title = title;
  service.setDisposition(disposition);

  RegisterIntentHandlerInfoBarDelegate::MaybeShowIntentInfoBar(
      tcw->infobar_tab_helper(),
      WebIntentsRegistryFactory::GetForProfile(tcw->profile()),
      service,
      favicon_service,
      tab->GetURL());
}

// static
void Browser::FindReplyHelper(TabContents* tab,
                              int request_id,
                              int number_of_matches,
                              const gfx::Rect& selection_rect,
                              int active_match_ordinal,
                              bool final_update) {
  TabContentsWrapper* tcw = TabContentsWrapper::GetCurrentWrapperForContents(
      tab);
  if (!tcw || !tcw->find_tab_helper())
    return;

  tcw->find_tab_helper()->HandleFindReply(request_id, number_of_matches,
                                          selection_rect, active_match_ordinal,
                                          final_update);
}

// static
void Browser::CrashedPluginHelper(TabContents* tab,
                                  const FilePath& plugin_path) {
  TabContentsWrapper* tcw = TabContentsWrapper::GetCurrentWrapperForContents(
      tab);
  if (!tcw)
    return;

  DCHECK(!plugin_path.value().empty());

  string16 plugin_name = plugin_path.LossyDisplayName();
  webkit::WebPluginInfo plugin_info;
  if (PluginService::GetInstance()->GetPluginInfoByPath(
          plugin_path, &plugin_info) &&
      !plugin_info.name.empty()) {
    plugin_name = plugin_info.name;
#if defined(OS_MACOSX)
    // Many plugins on the Mac have .plugin in the actual name, which looks
    // terrible, so look for that and strip it off if present.
    const std::string kPluginExtension = ".plugin";
    if (EndsWith(plugin_name, ASCIIToUTF16(kPluginExtension), true))
      plugin_name.erase(plugin_name.length() - kPluginExtension.length());
#endif  // OS_MACOSX
  }
  gfx::Image* icon = &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_INFOBAR_PLUGIN_CRASHED);
  InfoBarTabHelper* infobar_helper = tcw->infobar_tab_helper();
  infobar_helper->AddInfoBar(
      new SimpleAlertInfoBarDelegate(
          infobar_helper,
          icon,
          l10n_util::GetStringFUTF16(IDS_PLUGIN_CRASHED_PROMPT, plugin_name),
          true));
}

// static
void Browser::UpdateTargetURLHelper(TabContents* tab, int32 page_id,
                                    const GURL& url) {
  TabContentsWrapper* tcw = TabContentsWrapper::GetCurrentWrapperForContents(
      tab);
  if (!tcw || !tcw->prerender_tab_helper())
    return;
  tcw->prerender_tab_helper()->UpdateTargetURL(page_id, url);
}

void Browser::ExecuteCommandWithDisposition(
  int id, WindowOpenDisposition disposition) {
  // No commands are enabled if there is not yet any selected tab.
  // TODO(pkasting): It seems like we should not need this, because either
  // most/all commands should not have been enabled yet anyway or the ones that
  // are enabled should be global, or safe themselves against having no selected
  // tab.  However, Ben says he tried removing this before and got lots of
  // crashes, e.g. from Windows sending WM_COMMANDs at random times during
  // window construction.  This probably could use closer examination someday.
  if (!GetSelectedTabContentsWrapper())
    return;

  DCHECK(command_updater_.IsCommandEnabled(id)) << "Invalid/disabled command "
                                                << id;

  // If command execution is blocked then just record the command and return.
  if (block_command_execution_) {
    // We actually only allow no more than one blocked command, otherwise some
    // commands maybe lost.
    DCHECK_EQ(last_blocked_command_id_, -1);
    last_blocked_command_id_ = id;
    last_blocked_command_disposition_ = disposition;
    return;
  }

  // The order of commands in this switch statement must match the function
  // declaration order in browser.h!
  switch (id) {
    // Navigation commands
    case IDC_BACK:                  GoBack(disposition);              break;
    case IDC_FORWARD:               GoForward(disposition);           break;
    case IDC_RELOAD:                Reload(disposition);              break;
    case IDC_RELOAD_IGNORING_CACHE: ReloadIgnoringCache(disposition); break;
    case IDC_HOME:                  Home(disposition);                break;
    case IDC_OPEN_CURRENT_URL:      OpenCurrentURL();                 break;
    case IDC_STOP:                  Stop();                           break;

     // Window management commands
    case IDC_NEW_WINDOW:            NewWindow();                      break;
    case IDC_NEW_INCOGNITO_WINDOW:  NewIncognitoWindow();             break;
    case IDC_CLOSE_WINDOW:          CloseWindow();                    break;
    case IDC_NEW_TAB:               NewTab();                         break;
    case IDC_CLOSE_TAB:             CloseTab();                       break;
    case IDC_SELECT_NEXT_TAB:       SelectNextTab();                  break;
    case IDC_SELECT_PREVIOUS_TAB:   SelectPreviousTab();              break;
    case IDC_TABPOSE:               OpenTabpose();                    break;
    case IDC_MOVE_TAB_NEXT:         MoveTabNext();                    break;
    case IDC_MOVE_TAB_PREVIOUS:     MoveTabPrevious();                break;
    case IDC_SELECT_TAB_0:
    case IDC_SELECT_TAB_1:
    case IDC_SELECT_TAB_2:
    case IDC_SELECT_TAB_3:
    case IDC_SELECT_TAB_4:
    case IDC_SELECT_TAB_5:
    case IDC_SELECT_TAB_6:
    case IDC_SELECT_TAB_7:          SelectNumberedTab(id - IDC_SELECT_TAB_0);
                                                                      break;
    case IDC_SELECT_LAST_TAB:       SelectLastTab();                  break;
    case IDC_DUPLICATE_TAB:         DuplicateTab();                   break;
    case IDC_RESTORE_TAB:           RestoreTab();                     break;
    case IDC_COPY_URL:              WriteCurrentURLToClipboard();     break;
    case IDC_SHOW_AS_TAB:           ConvertPopupToTabbedBrowser();    break;
    case IDC_FULLSCREEN:            ToggleFullscreenMode(false);      break;
#if defined(OS_MACOSX)
    case IDC_PRESENTATION_MODE:     TogglePresentationMode(false);    break;
#endif
    case IDC_EXIT:                  Exit();                           break;
#if defined(OS_CHROMEOS)
    case IDC_SEARCH:                Search();                         break;
    case IDC_SHOW_KEYBOARD_OVERLAY: ShowKeyboardOverlay();            break;
#endif

    // Page-related commands
    case IDC_SAVE_PAGE:             SavePage();                       break;
    case IDC_BOOKMARK_PAGE:         BookmarkCurrentPage();            break;
    case IDC_BOOKMARK_ALL_TABS:     BookmarkAllTabs();                break;
    case IDC_VIEW_SOURCE:           ViewSelectedSource();             break;
    case IDC_EMAIL_PAGE_LOCATION:   EmailPageLocation();              break;
    case IDC_PRINT:                 Print();                          break;
    case IDC_ADVANCED_PRINT:        AdvancedPrint();                  break;
    case IDC_ENCODING_AUTO_DETECT:  ToggleEncodingAutoDetect();       break;
    case IDC_ENCODING_UTF8:
    case IDC_ENCODING_UTF16LE:
    case IDC_ENCODING_ISO88591:
    case IDC_ENCODING_WINDOWS1252:
    case IDC_ENCODING_GBK:
    case IDC_ENCODING_GB18030:
    case IDC_ENCODING_BIG5HKSCS:
    case IDC_ENCODING_BIG5:
    case IDC_ENCODING_KOREAN:
    case IDC_ENCODING_SHIFTJIS:
    case IDC_ENCODING_ISO2022JP:
    case IDC_ENCODING_EUCJP:
    case IDC_ENCODING_THAI:
    case IDC_ENCODING_ISO885915:
    case IDC_ENCODING_MACINTOSH:
    case IDC_ENCODING_ISO88592:
    case IDC_ENCODING_WINDOWS1250:
    case IDC_ENCODING_ISO88595:
    case IDC_ENCODING_WINDOWS1251:
    case IDC_ENCODING_KOI8R:
    case IDC_ENCODING_KOI8U:
    case IDC_ENCODING_ISO88597:
    case IDC_ENCODING_WINDOWS1253:
    case IDC_ENCODING_ISO88594:
    case IDC_ENCODING_ISO885913:
    case IDC_ENCODING_WINDOWS1257:
    case IDC_ENCODING_ISO88593:
    case IDC_ENCODING_ISO885910:
    case IDC_ENCODING_ISO885914:
    case IDC_ENCODING_ISO885916:
    case IDC_ENCODING_WINDOWS1254:
    case IDC_ENCODING_ISO88596:
    case IDC_ENCODING_WINDOWS1256:
    case IDC_ENCODING_ISO88598:
    case IDC_ENCODING_ISO88598I:
    case IDC_ENCODING_WINDOWS1255:
    case IDC_ENCODING_WINDOWS1258:  OverrideEncoding(id);             break;

    // Clipboard commands
    case IDC_CUT:                   Cut();                            break;
    case IDC_COPY:                  Copy();                           break;
    case IDC_PASTE:                 Paste();                          break;

    // Find-in-page
    case IDC_FIND:                  Find();                           break;
    case IDC_FIND_NEXT:             FindNext();                       break;
    case IDC_FIND_PREVIOUS:         FindPrevious();                   break;

    // Zoom
    case IDC_ZOOM_PLUS:             Zoom(content::PAGE_ZOOM_IN);      break;
    case IDC_ZOOM_NORMAL:           Zoom(content::PAGE_ZOOM_RESET);   break;
    case IDC_ZOOM_MINUS:            Zoom(content::PAGE_ZOOM_OUT);     break;

    // Focus various bits of UI
    case IDC_FOCUS_TOOLBAR:         FocusToolbar();                   break;
    case IDC_FOCUS_LOCATION:        FocusLocationBar();               break;
    case IDC_FOCUS_SEARCH:          FocusSearch();                    break;
    case IDC_FOCUS_MENU_BAR:        FocusAppMenu();                   break;
    case IDC_FOCUS_BOOKMARKS:       FocusBookmarksToolbar();          break;
    case IDC_FOCUS_CHROMEOS_STATUS: FocusChromeOSStatus();            break;
    case IDC_FOCUS_NEXT_PANE:       FocusNextPane();                  break;
    case IDC_FOCUS_PREVIOUS_PANE:   FocusPreviousPane();              break;

    // Show various bits of UI
    case IDC_OPEN_FILE:             OpenFile();                       break;
    case IDC_CREATE_SHORTCUTS:      OpenCreateShortcutsDialog();      break;
    case IDC_DEV_TOOLS:             ToggleDevToolsWindow(
                                        DEVTOOLS_TOGGLE_ACTION_NONE);
                                    break;
    case IDC_DEV_TOOLS_CONSOLE:     ToggleDevToolsWindow(
                                        DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE);
                                    break;
    case IDC_DEV_TOOLS_INSPECT:     ToggleDevToolsWindow(
                                        DEVTOOLS_TOGGLE_ACTION_INSPECT);
                                    break;
    case IDC_TASK_MANAGER:          OpenTaskManager(false);           break;
    case IDC_VIEW_BACKGROUND_PAGES: OpenTaskManager(true);            break;
    case IDC_FEEDBACK:              OpenBugReportDialog();            break;

    case IDC_SHOW_BOOKMARK_BAR:     ToggleBookmarkBar();              break;
    case IDC_PROFILING_ENABLED:     Profiling::Toggle();              break;

    case IDC_SHOW_BOOKMARK_MANAGER: OpenBookmarkManager();            break;
    case IDC_SHOW_APP_MENU:         ShowAppMenu();                    break;
    case IDC_SHOW_AVATAR_MENU:      ShowAvatarMenu();                 break;
    case IDC_SHOW_HISTORY:          ShowHistoryTab();                 break;
    case IDC_SHOW_DOWNLOADS:        ShowDownloadsTab();               break;
    case IDC_MANAGE_EXTENSIONS:     ShowExtensionsTab();              break;
    case IDC_SYNC_BOOKMARKS:        OpenSyncMyBookmarksDialog();      break;
    case IDC_OPTIONS:               OpenOptionsDialog();              break;
    case IDC_EDIT_SEARCH_ENGINES:   OpenSearchEngineOptionsDialog();  break;
    case IDC_VIEW_PASSWORDS:        OpenPasswordManager();            break;
    case IDC_CLEAR_BROWSING_DATA:   OpenClearBrowsingDataDialog();    break;
    case IDC_IMPORT_SETTINGS:       OpenImportSettingsDialog();       break;
    case IDC_ABOUT:                 OpenAboutChromeDialog();          break;
    case IDC_UPGRADE_DIALOG:        OpenUpdateChromeDialog();         break;
    case IDC_VIEW_INCOMPATIBILITIES: ShowAboutConflictsTab();         break;
    case IDC_HELP_PAGE:             ShowHelpTab();                    break;
#if defined(OS_CHROMEOS)
    case IDC_LOCK_SCREEN:           LockScreen();                     break;
    case IDC_FILE_MANAGER:          OpenFileManager();                break;
    case IDC_SYSTEM_OPTIONS:        OpenSystemOptionsDialog();        break;
    case IDC_INTERNET_OPTIONS:      OpenInternetOptionsDialog();      break;
    case IDC_LANGUAGE_OPTIONS:      OpenLanguageOptionsDialog();      break;
#endif
    case IDC_SHOW_SYNC_SETUP:       ShowSyncSetup();                  break;
    case IDC_TOGGLE_SPEECH_INPUT:   ToggleSpeechInput();              break;

    default:
      LOG(WARNING) << "Received Unimplemented Command: " << id;
      break;
  }
}

void Browser::ExecuteCommand(int id, int event_flags) {
  ExecuteCommandWithDisposition(
      id, browser::DispositionFromEventFlags(event_flags));
}

bool Browser::ExecuteCommandIfEnabled(int id) {
  if (command_updater_.SupportsCommand(id) &&
      command_updater_.IsCommandEnabled(id)) {
    ExecuteCommand(id);
    return true;
  }
  return false;
}

bool Browser::IsReservedCommandOrKey(int command_id,
                                     const NativeWebKeyboardEvent& event) {
#if defined(OS_CHROMEOS)
  // Chrome OS's top row of keys produces F1-10.  Make sure that web pages
  // aren't able to block Chrome from performing the standard actions for F1-F4
  // (F5-7 are grabbed by other X clients and hence don't need this protection,
  // and F8-10 are handled separately in Chrome via a GDK event filter, but
  // let's future-proof this).
  ui::KeyboardCode key_code =
      static_cast<ui::KeyboardCode>(event.windowsKeyCode);
  if (key_code == ui::VKEY_F1 ||
      key_code == ui::VKEY_F2 ||
      key_code == ui::VKEY_F3 ||
      key_code == ui::VKEY_F4 ||
      key_code == ui::VKEY_F5 ||
      key_code == ui::VKEY_F6 ||
      key_code == ui::VKEY_F7 ||
      key_code == ui::VKEY_F8 ||
      key_code == ui::VKEY_F9 ||
      key_code == ui::VKEY_F10) {
    return true;
  }
#endif

  if (window_->IsFullscreen() && command_id == IDC_FULLSCREEN)
    return true;
  return command_id == IDC_CLOSE_TAB ||
         command_id == IDC_CLOSE_WINDOW ||
         command_id == IDC_NEW_INCOGNITO_WINDOW ||
         command_id == IDC_NEW_TAB ||
         command_id == IDC_NEW_WINDOW ||
         command_id == IDC_RESTORE_TAB ||
         command_id == IDC_SELECT_NEXT_TAB ||
         command_id == IDC_SELECT_PREVIOUS_TAB ||
         command_id == IDC_TABPOSE ||
         command_id == IDC_EXIT ||
         command_id == IDC_SEARCH;
}

void Browser::SetBlockCommandExecution(bool block) {
  block_command_execution_ = block;
  if (block) {
    last_blocked_command_id_ = -1;
    last_blocked_command_disposition_ = CURRENT_TAB;
  }
}

int Browser::GetLastBlockedCommand(WindowOpenDisposition* disposition) {
  if (disposition)
    *disposition = last_blocked_command_disposition_;
  return last_blocked_command_id_;
}

void Browser::UpdateUIForNavigationInTab(TabContentsWrapper* contents,
                                         content::PageTransition transition,
                                         bool user_initiated) {
  tabstrip_model()->TabNavigating(contents, transition);

  bool contents_is_selected = contents == GetSelectedTabContentsWrapper();
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
  ScheduleUIUpdate(contents->tab_contents(), TabContents::INVALIDATE_URL);

  if (contents_is_selected)
    contents->tab_contents()->Focus();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, PageNavigator implementation:

// TODO(adriansc): Remove this method once refactoring changed all call sites.
TabContents* Browser::OpenURL(const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              content::PageTransition transition) {
  // For specifying a referrer, use the version of OpenURL taking OpenURLParams.
  DCHECK(referrer.is_empty());
  return OpenURLFromTab(NULL, OpenURLParams(
      url, content::Referrer(), disposition, transition, false));
}

TabContents* Browser::OpenURL(const OpenURLParams& params) {
  return OpenURLFromTab(NULL, params);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, CommandUpdater::CommandUpdaterDelegate implementation:

void Browser::ExecuteCommand(int id) {
  ExecuteCommandWithDisposition(id, CURRENT_TAB);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabHandlerDelegate implementation:

Profile* Browser::GetProfile() const {
  return profile();
}

Browser* Browser::AsBrowser() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModelDelegate implementation:

TabContentsWrapper* Browser::AddBlankTab(bool foreground) {
  return AddBlankTabAt(-1, foreground);
}

TabContentsWrapper* Browser::AddBlankTabAt(int index, bool foreground) {
  // Time new tab page creation time.  We keep track of the timing data in
  // TabContents, but we want to include the time it takes to create the
  // TabContents object too.
  base::TimeTicks new_tab_start_time = base::TimeTicks::Now();
  browser::NavigateParams params(this, GURL(chrome::kChromeUINewTabURL),
                                 content::PAGE_TRANSITION_TYPED);
  params.disposition = foreground ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  params.tabstrip_index = index;
  browser::Navigate(&params);
  params.target_contents->tab_contents()->set_new_tab_start_time(
      new_tab_start_time);
  return params.target_contents;
}

Browser* Browser::CreateNewStripWithContents(
    TabContentsWrapper* detached_contents,
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
  browser->set_show_state(
      maximize ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_NORMAL);
  browser->InitBrowserWindow();
  browser->tabstrip_model()->AppendTabContents(detached_contents, true);
  // Make sure the loading state is updated correctly, otherwise the throbber
  // won't start if the page is loading.
  browser->LoadingStateChanged(detached_contents->tab_contents());
  return browser;
}

int Browser::GetDragActions() const {
  return TabStripModelDelegate::TAB_TEAROFF_ACTION | (tab_count() > 1 ?
      TabStripModelDelegate::TAB_MOVE_ACTION : 0);
}

TabContentsWrapper* Browser::CreateTabContentsForURL(
    const GURL& url, const GURL& referrer, Profile* profile,
    content::PageTransition transition, bool defer_load,
    SiteInstance* instance) const {
  TabContentsWrapper* contents = TabContentsFactory(profile, instance,
      MSG_ROUTING_NONE,
      GetSelectedTabContents(), NULL);
  if (!defer_load) {
    // Load the initial URL before adding the new tab contents to the tab strip
    // so that the tab contents has navigation state.
    contents->controller().LoadURL(
        url,
        content::Referrer(referrer, WebKit::WebReferrerPolicyDefault),
        transition,
        std::string());
  }

  return contents;
}

bool Browser::CanDuplicateContentsAt(int index) {
  NavigationController& nc = GetTabContentsAt(index)->controller();
  return nc.tab_contents() && nc.GetLastCommittedEntry();
}

void Browser::DuplicateContentsAt(int index) {
  TabContentsWrapper* contents = GetTabContentsWrapperAt(index);
  CHECK(contents);
  TabContentsWrapper* contents_dupe = contents->Clone();

  bool pinned = false;
  if (CanSupportWindowFeature(FEATURE_TABSTRIP)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    int index = tab_handler_->GetTabStripModel()->
        GetIndexOfTabContents(contents);
    pinned = tab_handler_->GetTabStripModel()->IsTabPinned(index);
    int add_types = TabStripModel::ADD_ACTIVE |
        TabStripModel::ADD_INHERIT_GROUP |
        (pinned ? TabStripModel::ADD_PINNED : 0);
    tab_handler_->GetTabStripModel()->InsertTabContentsAt(index + 1,
                                                          contents_dupe,
                                                          add_types);
  } else {
    Browser* browser = NULL;
    if (is_app()) {
      CHECK(!is_type_popup());
      CHECK(!is_type_panel());
      browser = Browser::CreateForApp(TYPE_POPUP, app_name_, gfx::Rect(),
                                      profile_);
    } else if (is_type_popup()) {
      browser = Browser::CreateForType(TYPE_POPUP, profile_);
    }

    // Preserve the size of the original window. The new window has already
    // been given an offset by the OS, so we shouldn't copy the old bounds.
    BrowserWindow* new_window = browser->window();
    new_window->SetBounds(gfx::Rect(new_window->GetRestoredBounds().origin(),
                          window()->GetRestoredBounds().size()));

    // We need to show the browser now.  Otherwise ContainerWin assumes the
    // TabContents is invisible and won't size it.
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

void Browser::CreateHistoricalTab(TabContentsWrapper* contents) {
  // We don't create historical tabs for incognito windows or windows without
  // profiles.
  if (!profile() || profile()->IsOffTheRecord())
    return;

  // We don't create historical tabs for print preview tabs.
  if (contents->tab_contents()->GetURL() == GURL(chrome::kChromeUIPrintURL))
    return;

  TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile());

  // We only create historical tab entries for tabbed browser windows.
  if (service && CanSupportWindowFeature(FEATURE_TABSTRIP)) {
    service->CreateHistoricalTab(&contents->controller(),
        tab_handler_->GetTabStripModel()->GetIndexOfTabContents(contents));
  }
}

bool Browser::RunUnloadListenerBeforeClosing(TabContentsWrapper* contents) {
  return Browser::RunUnloadEventsHelper(contents->tab_contents());
}

bool Browser::CanReloadContents(TabContents* source) const {
  return !is_devtools();
}

bool Browser::CanCloseContents(std::vector<int>* indices) {
  DCHECK(!indices->empty());
  TabCloseableStateWatcher* watcher =
      g_browser_process->tab_closeable_state_watcher();
  bool can_close_all = !watcher || watcher->CanCloseTabs(this, indices);
  if (indices->empty())  // Cannot close any tab.
    return false;
  // Now, handle cases where at least one tab can be closed.
  // If we are closing all the tabs for this browser, make sure to check for
  // in-progress downloads.
  // Note that the next call when it returns false will ask the user for
  // confirmation before closing the browser if the user decides so.
  if (tab_handler_->GetTabStripModel()->count() ==
          static_cast<int>(indices->size()) &&
      !CanCloseWithInProgressDownloads()) {
    indices->clear();
    can_close_all = false;
  }
  return can_close_all;
}

bool Browser::CanBookmarkAllTabs() const {
  BookmarkModel* model = profile()->GetBookmarkModel();
  return (model && model->IsLoaded()) &&
         tab_count() > 1 &&
         profile()->GetPrefs()->GetBoolean(prefs::kEditBookmarksEnabled);
}

void Browser::BookmarkAllTabs() {
  BookmarkEditor::ShowBookmarkAllTabsDialog(this);
}

bool Browser::CanCloseTab() const {
  TabCloseableStateWatcher* watcher =
      g_browser_process->tab_closeable_state_watcher();
  return !watcher || watcher->CanCloseTab(this);
}

bool Browser::LargeIconsPermitted() const {
  // We don't show the big icons in tabs for TYPE_EXTENSION_APP windows because
  // for those windows, we already have a big icon in the top-left outside any
  // tab. Having big tab icons too looks kinda redonk.
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModelObserver implementation:

void Browser::TabInsertedAt(TabContentsWrapper* contents,
                            int index,
                            bool foreground) {
  SetAsDelegate(contents, this);
  contents->restore_tab_helper()->SetWindowID(session_id());

  SyncHistoryWithTabs(index);

  // Make sure the loading state is updated correctly, otherwise the throbber
  // won't start if the page is loading.
  LoadingStateChanged(contents->tab_contents());

  // If the tab crashes in the beforeunload or unload handler, it won't be
  // able to ack. But we know we can close it.
  registrar_.Add(this, content::NOTIFICATION_TAB_CONTENTS_DISCONNECTED,
                 content::Source<TabContents>(contents->tab_contents()));

  registrar_.Add(this, content::NOTIFICATION_INTERSTITIAL_ATTACHED,
                 content::Source<TabContents>(contents->tab_contents()));
}

void Browser::TabClosingAt(TabStripModel* tab_strip_model,
                           TabContentsWrapper* contents,
                           int index) {
  fullscreen_controller_->OnTabClosing(contents->tab_contents());
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_TAB_CLOSING,
      content::Source<NavigationController>(&contents->controller()),
      content::NotificationService::NoDetails());

  // Sever the TabContents' connection back to us.
  SetAsDelegate(contents, NULL);
}

void Browser::TabDetachedAt(TabContentsWrapper* contents, int index) {
  TabDetachedAtImpl(contents, index, DETACH_TYPE_DETACH);
}

void Browser::TabDeactivated(TabContentsWrapper* contents) {
  fullscreen_controller_->OnTabDeactivated(contents);
  if (instant())
    instant()->Hide();

  // Save what the user's currently typing, so it can be restored when we
  // switch back to this tab.
  window_->GetLocationBar()->SaveStateToContents(contents->tab_contents());
}

void Browser::ActiveTabChanged(TabContentsWrapper* old_contents,
                               TabContentsWrapper* new_contents,
                               int index,
                               bool user_gesture) {
  // On some platforms we want to automatically reload tabs that are
  // killed when the user selects them.
  if (user_gesture && new_contents->tab_contents()->crashed_status() ==
        base::TERMINATION_STATUS_PROCESS_WAS_KILLED) {
    const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
    if (parsed_command_line.HasSwitch(switches::kReloadKilledTabs)) {
      Reload(CURRENT_TAB);
      return;
    }
  }

  // Discarded tabs always get reloaded.
  if (IsTabDiscarded(index)) {
    Reload(CURRENT_TAB);
    return;
  }

  // If we have any update pending, do it now.
  if (chrome_updater_factory_.HasWeakPtrs() && old_contents)
    ProcessPendingUIUpdates();

  // Propagate the profile to the location bar.
  UpdateToolbar(true);

  // Update reload/stop state.
  UpdateReloadStopState(new_contents->tab_contents()->IsLoading(), true);

  // Update commands to reflect current state.
  UpdateCommandsForTabState();

  // Reset the status bubble.
  StatusBubble* status_bubble = GetStatusBubble();
  if (status_bubble) {
    status_bubble->Hide();

    // Show the loading state (if any).
    status_bubble->SetStatus(GetSelectedTabContentsWrapper()->GetStatusText());
  }

  if (HasFindBarController()) {
    find_bar_controller_->ChangeTabContents(new_contents);
    find_bar_controller_->find_bar()->MoveWindowIfNecessary(gfx::Rect(), true);
  }

  // Update sessions. Don't force creation of sessions. If sessions doesn't
  // exist, the change will be picked up by sessions when created.
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile_);
  if (session_service && !tab_handler_->GetTabStripModel()->closing_all()) {
    session_service->SetSelectedTabInWindow(
        session_id(), tab_handler_->GetTabStripModel()->active_index());
  }

  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_SWITCH);
}

void Browser::TabMoved(TabContentsWrapper* contents,
                       int from_index,
                       int to_index) {
  DCHECK(from_index >= 0 && to_index >= 0);
  // Notify the history service.
  SyncHistoryWithTabs(std::min(from_index, to_index));
}

void Browser::TabReplacedAt(TabStripModel* tab_strip_model,
                            TabContentsWrapper* old_contents,
                            TabContentsWrapper* new_contents,
                            int index) {
  TabDetachedAtImpl(old_contents, index, DETACH_TYPE_REPLACE);
  TabInsertedAt(new_contents, index,
                (index == tab_handler_->GetTabStripModel()->active_index()));

  int entry_count = new_contents->controller().entry_count();
  if (entry_count > 0) {
    // Send out notification so that observers are updated appropriately.
    new_contents->controller().NotifyEntryChanged(
        new_contents->controller().GetEntryAtIndex(entry_count - 1),
        entry_count - 1);
  }

  SessionService* session_service =
      SessionServiceFactory::GetForProfile(profile());
  if (session_service) {
    // The new_contents may end up with a different navigation stack. Force
    // the session service to update itself.
    session_service->TabRestored(
        new_contents, tab_handler_->GetTabStripModel()->IsTabPinned(index));
  }

  content::DevToolsManager::GetInstance()->TabReplaced(
      old_contents->tab_contents(), new_contents->tab_contents());
}

void Browser::TabPinnedStateChanged(TabContentsWrapper* contents, int index) {
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile());
  if (session_service) {
    session_service->SetPinnedState(
        session_id(),
        GetTabContentsWrapperAt(index)->restore_tab_helper()->session_id(),
        tab_handler_->GetTabStripModel()->IsTabPinned(index));
  }
}

void Browser::TabStripEmpty() {
  // Close the frame after we return to the message loop (not immediately,
  // otherwise it will destroy this object before the stack has a chance to
  // cleanly unwind.)
  // Note: This will be called several times if TabStripEmpty is called several
  //       times. This is because it does not close the window if tabs are
  //       still present.
  // NOTE: If you change to be immediate (no invokeLater) then you'll need to
  //       update BrowserList::CloseAllBrowsers.
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&Browser::CloseFrame, weak_factory_.GetWeakPtr()));
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabContentsDelegate implementation:

TabContents* Browser::OpenURLFromTab(TabContents* source,
                                     const OpenURLParams& params) {
  browser::NavigateParams nav_params(this, params.url, params.transition);
  nav_params.source_contents =
      tabstrip_model()->GetTabContentsAt(
          tabstrip_model()->GetWrapperIndex(source));
  nav_params.referrer = params.referrer.url;
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
      nav_params.target_contents->tab_contents() : NULL;
}

void Browser::NavigationStateChanged(const TabContents* source,
                                     unsigned changed_flags) {
  // Only update the UI when something visible has changed.
  if (changed_flags)
    ScheduleUIUpdate(source, changed_flags);

  // We can synchronously update commands since they will only change once per
  // navigation, so we don't have to worry about flickering. We do, however,
  // need to update the command state early on load to always present usable
  // actions in the face of slow-to-commit pages.
  if (changed_flags & (TabContents::INVALIDATE_URL |
                       TabContents::INVALIDATE_LOAD))
    UpdateCommandsForTabState();
}

void Browser::AddNewContents(TabContents* source,
                             TabContents* new_contents,
                             WindowOpenDisposition disposition,
                             const gfx::Rect& initial_pos,
                             bool user_gesture) {
  // No code for this yet.
  DCHECK(disposition != SAVE_TO_DISK);
  // Can't create a new contents for the current tab - invalid case.
  DCHECK(disposition != CURRENT_TAB);

  TabContentsWrapper* source_wrapper = NULL;
  BlockedContentTabHelper* source_blocked_content = NULL;
  TabContentsWrapper* new_wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(new_contents);
  if (!new_wrapper)
    new_wrapper = new TabContentsWrapper(new_contents);
  if (source) {
    source_wrapper = TabContentsWrapper::GetCurrentWrapperForContents(source);
    source_blocked_content = source_wrapper->blocked_content_tab_helper();
  }

  if (source_wrapper) {
    // Handle blocking of all contents.
    if (source_blocked_content->all_contents_blocked()) {
      source_blocked_content->AddTabContents(new_wrapper,
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
      GetConstrainingContentsWrapper(source_wrapper)->
          blocked_content_tab_helper()->
              AddPopup(new_wrapper, initial_pos, user_gesture);
      return;
    }

    new_contents->render_view_host()->DisassociateFromPopupCount();
  }

  browser::NavigateParams params(this, new_wrapper);
  params.source_contents =
      source ? tabstrip_model()->GetTabContentsAt(
                   tabstrip_model()->GetWrapperIndex(source))
             : NULL;
  params.disposition = disposition;
  params.window_bounds = initial_pos;
  params.window_action = browser::NavigateParams::SHOW_WINDOW;
  params.user_gesture = user_gesture;
  browser::Navigate(&params);
}

void Browser::ActivateContents(TabContents* contents) {
  tab_handler_->GetTabStripModel()->ActivateTabAt(
      tab_handler_->GetTabStripModel()->GetWrapperIndex(contents), false);
  window_->Activate();
}

void Browser::DeactivateContents(TabContents* contents) {
  window_->Deactivate();
}

void Browser::LoadingStateChanged(TabContents* source) {
  window_->UpdateLoadingAnimations(
      tab_handler_->GetTabStripModel()->TabsAreLoading());
  window_->UpdateTitleBar();

  TabContents* selected_contents = GetSelectedTabContents();
  if (source == selected_contents) {
    bool is_loading = source->IsLoading();
    UpdateReloadStopState(is_loading, false);
    if (GetStatusBubble()) {
      GetStatusBubble()->SetStatus(
          GetSelectedTabContentsWrapper()->GetStatusText());
    }

    if (!is_loading && pending_web_app_action_ == UPDATE_SHORTCUT) {
      // Schedule a shortcut update when web application info is available if
      // last committed entry is not NULL. Last committed entry could be NULL
      // when an interstitial page is injected (e.g. bad https certificate,
      // malware site etc). When this happens, we abort the shortcut update.
      NavigationEntry* entry = source->controller().GetLastCommittedEntry();
      if (entry) {
        TabContentsWrapper::GetCurrentWrapperForContents(source)->
            extension_tab_helper()->GetApplicationInfo(entry->page_id());
      } else {
        pending_web_app_action_ = NONE;
      }
    }
  }
}

void Browser::CloseContents(TabContents* source) {
  if (is_attempting_to_close_browser_) {
    // If we're trying to close the browser, just clear the state related to
    // waiting for unload to fire. Don't actually try to close the tab as it
    // will go down the slow shutdown path instead of the fast path of killing
    // all the renderer processes.
    ClearUnloadState(source, true);
    return;
  }

  int index = tab_handler_->GetTabStripModel()->GetWrapperIndex(source);
  if (index == TabStripModel::kNoTab) {
    NOTREACHED() << "CloseContents called for tab not in our strip";
    return;
  }
  tab_handler_->GetTabStripModel()->CloseTabContentsAt(
      index,
      TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
}

void Browser::MoveContents(TabContents* source, const gfx::Rect& pos) {
  if (!IsPopupOrPanel(source)) {
    NOTREACHED() << "moving invalid browser type";
    return;
  }
  window_->SetBounds(pos);
}

void Browser::DetachContents(TabContents* source) {
  int index = tab_handler_->GetTabStripModel()->GetWrapperIndex(source);
  if (index >= 0)
    tab_handler_->GetTabStripModel()->DetachTabContentsAt(index);
}

bool Browser::IsPopupOrPanel(const TabContents* source) const {
  // A non-tabbed BROWSER is an unconstrained popup.
  return is_type_popup() || is_type_panel();
}

void Browser::ContentsMouseEvent(
    TabContents* source, const gfx::Point& location, bool motion) {
  if (!GetStatusBubble())
    return;

  if (source == GetSelectedTabContents()) {
    GetStatusBubble()->MouseMoved(location, !motion);
    if (!motion)
      GetStatusBubble()->SetURL(GURL(), std::string());
  }
}

void Browser::UpdateTargetURL(TabContents* source, int32 page_id,
                              const GURL& url) {
  Browser::UpdateTargetURLHelper(source, page_id, url);

  if (!GetStatusBubble())
    return;

  if (source == GetSelectedTabContents()) {
    PrefService* prefs = profile_->GetPrefs();
    GetStatusBubble()->SetURL(url, prefs->GetString(prefs::kAcceptLanguages));
  }
}

void Browser::UpdateDownloadShelfVisibility(bool visible) {
  if (GetStatusBubble())
    GetStatusBubble()->UpdateDownloadShelfVisibility(visible);
}

void Browser::ContentsZoomChange(bool zoom_in) {
  ExecuteCommand(zoom_in ? IDC_ZOOM_PLUS : IDC_ZOOM_MINUS);
}

void Browser::TabContentsFocused(TabContents* tab_content) {
  window_->TabContentsFocused(tab_content);
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

void Browser::ConvertContentsToApplication(TabContents* contents) {
  const GURL& url = contents->controller().GetActiveEntry()->url();
  std::string app_name = web_app::GenerateApplicationNameFromURL(url);

  DetachContents(contents);
  Browser* app_browser = Browser::CreateForApp(
      TYPE_POPUP, app_name, gfx::Rect(), profile_);
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(contents);
  if (!wrapper)
    wrapper = new TabContentsWrapper(contents);
  app_browser->tabstrip_model()->AppendTabContents(wrapper, true);

  contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  contents->render_view_host()->SyncRendererPrefs();
  app_browser->window()->Show();
}

void Browser::BeforeUnloadFired(TabContents* tab,
                                bool proceed,
                                bool* proceed_to_fire_unload) {
  if (!is_attempting_to_close_browser_) {
    *proceed_to_fire_unload = proceed;
    if (!proceed)
      tab->set_closed_by_user_gesture(false);
    return;
  }

  if (!proceed) {
    CancelWindowClose();
    *proceed_to_fire_unload = false;
    tab->set_closed_by_user_gesture(false);
    return;
  }

  if (RemoveFromSet(&tabs_needing_before_unload_fired_, tab)) {
    // Now that beforeunload has fired, put the tab on the queue to fire
    // unload.
    tabs_needing_unload_fired_.insert(tab);
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

namespace {

bool DisplayOldDownloadsUI() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDownloadsNewUI);
}

}  // anonymous namespace

void Browser::OnStartDownload(TabContents* source, DownloadItem* download) {
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(source);
  TabContentsWrapper* constrained = GetConstrainingContentsWrapper(wrapper);
  if (constrained != wrapper) {
    // Download in a constrained popup is shown in the tab that opened it.
    TabContents* constrained_tab = constrained->tab_contents();
    constrained_tab->delegate()->OnStartDownload(constrained_tab, download);
    return;
  }

  if (!window())
    return;

  if (DisplayOldDownloadsUI()) {
#if defined(OS_CHROMEOS) && !defined(USE_AURA)
    // Don't show content browser for extension/theme downloads from gallery.
    ExtensionService* service = profile_->GetExtensionService();
    if (!ChromeDownloadManagerDelegate::IsExtensionDownload(download) ||
        service == NULL ||
        !service->IsDownloadFromGallery(download->GetURL(),
                                        download->GetReferrerUrl())) {
      // Open the Active Downloads ui for chromeos.
      ActiveDownloadsUI::OpenPopup(profile_);
    }
#else
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
    TabContents* shelf_tab = shelf->browser()->GetSelectedTabContents();
    if ((download->GetTotalBytes() > 0) &&
        !ChromeDownloadManagerDelegate::IsExtensionDownload(download) &&
        platform_util::IsVisible(shelf_tab->GetNativeView()) &&
        ui::Animation::ShouldRenderRichAnimation()) {
      DownloadStartedAnimation::Show(shelf_tab);
    }
#endif
  }

  // If the download occurs in a new tab, close it.
  if (source->controller().IsInitialNavigation() && tab_count() > 1)
    CloseContents(source);
}

void Browser::ShowPageInfo(content::BrowserContext* browser_context,
                           const GURL& url,
                           const NavigationEntry::SSLStatus& ssl,
                           bool show_history) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  window()->ShowPageInfo(profile, url, ssl, show_history);
}

void Browser::ViewSourceForTab(TabContents* source, const GURL& page_url) {
  DCHECK(source);
  int index = tabstrip_model()->GetWrapperIndex(source);
  TabContentsWrapper* wrapper = tabstrip_model()->GetTabContentsAt(index);
  ViewSource(wrapper);
}

void Browser::ViewSourceForFrame(TabContents* source,
                                 const GURL& frame_url,
                                 const std::string& frame_content_state) {
  DCHECK(source);
  int index = tabstrip_model()->GetWrapperIndex(source);
  TabContentsWrapper* wrapper = tabstrip_model()->GetTabContentsAt(index);
  ViewSource(wrapper, frame_url, frame_content_state);
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

void Browser::ShowRepostFormWarningDialog(TabContents *tab_contents) {
  window()->ShowRepostFormWarningDialog(tab_contents);
}

void Browser::ShowContentSettingsPage(ContentSettingsType content_type) {
  ShowOptionsTab(
      chrome::kContentSettingsExceptionsSubPage + std::string(kHashMark) +
      ContentSettingsHandler::ContentSettingsTypeToGroupName(content_type));
}

void Browser::ShowCollectedCookiesDialog(TabContentsWrapper* wrapper) {
  window()->ShowCollectedCookiesDialog(wrapper);
}

bool Browser::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    content::NavigationType navigation_type) {
  // Don't update history if running as app.
  return !IsApplication();
}

void Browser::TabContentsCreated(TabContents* new_contents) {
  // Create a TabContentsWrapper now, so all observers are in place, as the
  // network requests for its initial navigation will start immediately. The
  // TabContents will later be inserted into this browser using
  // Browser::Navigate via AddNewContents. The latter will retrieve the newly
  // created TabContentsWrapper from TabContents object.
  new TabContentsWrapper(new_contents);
}

void Browser::ContentRestrictionsChanged(TabContents* source) {
  UpdateCommandsForContentRestrictionState();
}

void Browser::RendererUnresponsive(TabContents* source) {
  // Ignore hangs if print preview is open.
  TabContentsWrapper* source_wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(source);
  if (source_wrapper) {
    printing::PrintPreviewTabController* controller =
        printing::PrintPreviewTabController::GetInstance();
    if (controller) {
      TabContentsWrapper* preview_tab =
          controller->GetPrintPreviewForTab(source_wrapper);
      if (preview_tab && preview_tab != source_wrapper) {
        return;
      }
    }
  }

  browser::ShowHungRendererDialog(source);
}

void Browser::RendererResponsive(TabContents* source) {
  browser::HideHungRendererDialog(source);
}

void Browser::WorkerCrashed(TabContents* source) {
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(source);
  InfoBarTabHelper* infobar_helper = wrapper->infobar_tab_helper();
  infobar_helper->AddInfoBar(new SimpleAlertInfoBarDelegate(
      infobar_helper,
      NULL,
      l10n_util::GetStringUTF16(IDS_WEBWORKER_CRASHED_PROMPT),
      true));
}

void Browser::DidNavigateMainFramePostCommit(TabContents* tab) {
  if (tab == GetSelectedTabContents())
    UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
}

void Browser::DidNavigateToPendingEntry(TabContents* tab) {
  if (tab == GetSelectedTabContents())
    UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
}

content::JavaScriptDialogCreator* Browser::GetJavaScriptDialogCreator() {
  return GetJavaScriptDialogCreatorInstance();
}

void Browser::RunFileChooser(TabContents* tab,
                             const content::FileChooserParams& params) {
  RunFileChooserHelper(tab, params);
}

void Browser::EnumerateDirectory(TabContents* tab, int request_id,
                                 const FilePath& path) {
  EnumerateDirectoryHelper(tab, request_id, path);
}

void Browser::ToggleFullscreenModeForTab(TabContents* tab,
                                         bool enter_fullscreen) {
  fullscreen_controller_->ToggleFullscreenModeForTab(tab, enter_fullscreen);
}

bool Browser::IsFullscreenForTab(const TabContents* tab) const {
  return fullscreen_controller_->IsFullscreenForTab(tab);
}

void Browser::JSOutOfMemory(TabContents* tab) {
  JSOutOfMemoryHelper(tab);
}

void Browser::RegisterProtocolHandler(TabContents* tab,
                                      const std::string& protocol,
                                      const GURL& url,
                                      const string16& title) {
  RegisterProtocolHandlerHelper(tab, protocol, url, title);
}

void Browser::RegisterIntentHandler(TabContents* tab,
                                    const string16& action,
                                    const string16& type,
                                    const string16& href,
                                    const string16& title,
                                    const string16& disposition) {
  RegisterIntentHandlerHelper(tab, action, type, href, title, disposition);
}

void Browser::WebIntentDispatch(TabContents* tab,
                                content::IntentsHost* intents_host) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableWebIntents))
    return;

  TabContentsWrapper* tcw =
      TabContentsWrapper::GetCurrentWrapperForContents(tab);
  tcw->web_intent_picker_controller()->SetIntentsHost(intents_host);
  tcw->web_intent_picker_controller()->ShowDialog(
      this, intents_host->GetIntent().action, intents_host->GetIntent().type);
}

void Browser::FindReply(TabContents* tab,
                        int request_id,
                        int number_of_matches,
                        const gfx::Rect& selection_rect,
                        int active_match_ordinal,
                        bool final_update) {
  FindReplyHelper(tab, request_id, number_of_matches, selection_rect,
                  active_match_ordinal, final_update);
}

void Browser::CrashedPlugin(TabContents* tab, const FilePath& plugin_path) {
  CrashedPluginHelper(tab, plugin_path);
}

void Browser::UpdatePreferredSize(TabContents* source,
                                  const gfx::Size& pref_size) {
  window_->UpdatePreferredSize(source, pref_size);
}

void Browser::RequestToLockMouse(TabContents* tab) {
  fullscreen_controller_->RequestToLockMouse(tab);
}

void Browser::LostMouseLock() {
  fullscreen_controller_->LostMouseLock();
}

void Browser::OnAcceptFullscreenPermission(
    const GURL& url,
    FullscreenExitBubbleType bubble_type) {
  fullscreen_controller_->OnAcceptFullscreenPermission(url, bubble_type);
}

void Browser::OnDenyFullscreenPermission(FullscreenExitBubbleType bubble_type) {
  fullscreen_controller_->OnDenyFullscreenPermission(bubble_type);
}


///////////////////////////////////////////////////////////////////////////////
// Browser, TabContentsWrapperDelegate implementation:

void Browser::OnDidGetApplicationInfo(TabContentsWrapper* source,
                                      int32 page_id) {
  if (GetSelectedTabContentsWrapper() != source)
    return;

  NavigationEntry* entry = source->controller().GetLastCommittedEntry();
  if (!entry || (entry->page_id() != page_id))
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

void Browser::OnInstallApplication(TabContentsWrapper* source,
                                   const WebApplicationInfo& web_app) {
  ExtensionService* extension_service = profile()->GetExtensionService();
  if (!extension_service)
    return;

  scoped_refptr<CrxInstaller> installer(CrxInstaller::Create(
      extension_service,
      extension_service->show_extensions_prompts() ?
      new ExtensionInstallUI(profile()) : NULL));
  installer->InstallWebApp(web_app);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, SearchEngineTabHelperDelegate implementation:

void Browser::ConfirmSetDefaultSearchProvider(TabContents* tab_contents,
                                              TemplateURL* template_url,
                                              Profile* profile) {
  window()->ConfirmSetDefaultSearchProvider(tab_contents, template_url,
                                            profile);
}

void Browser::ConfirmAddSearchProvider(const TemplateURL* template_url,
                                       Profile* profile) {
#if defined(OS_CHROMEOS) || defined(USE_AURA)
  // Use a WebUI implementation of the dialog.
  browser::ConfirmAddSearchProvider(template_url, profile);
#else
  // TODO(rbyers): Remove the IsMoreWebUI check and (ideally) all #ifdefs once
  // we can select exactly one version of this dialog to use for each platform
  // at build time.
  if (ChromeWebUI::IsMoreWebUI())
    browser::ConfirmAddSearchProvider(template_url, profile);
  else
    // Platform-specific implementation of the dialog.
    window()->ConfirmAddSearchProvider(template_url, profile);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Browser, BlockedContentTabHelperDelegate implementation:

TabContentsWrapper* Browser::GetConstrainingContentsWrapper(
  TabContentsWrapper* source) {
  return source;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, BookmarkTabHelperDelegate implementation:

void Browser::URLStarredChanged(TabContentsWrapper* source, bool starred) {
  if (source == GetSelectedTabContentsWrapper())
    window_->SetStarredState(starred);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, SelectFileDialog::Listener implementation:

void Browser::FileSelected(const FilePath& path, int index, void* params) {
  profile_->set_last_selected_directory(path.DirName());
  GURL file_url = net::FilePathToFileURL(path);
  if (!file_url.is_empty())
    OpenURL(file_url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, content::NotificationObserver implementation:

void Browser::Observe(int type,
                      const content::NotificationSource& source,
                      const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_TAB_CONTENTS_DISCONNECTED:
      if (is_attempting_to_close_browser_) {
        // Pass in false so that we delay processing. We need to delay the
        // processing as it may close the tab, which is currently on the call
        // stack above us.
        ClearUnloadState(content::Source<TabContents>(source).ptr(), false);
      }
      break;

    case content::NOTIFICATION_SSL_VISIBLE_STATE_CHANGED:
      // When the current tab's SSL state changes, we need to update the URL
      // bar to reflect the new state. Note that it's possible for the selected
      // tab contents to be NULL. This is because we listen for all sources
      // (NavigationControllers) for convenience, so the notification could
      // actually be for a different window while we're doing asynchronous
      // closing of this one.
      if (GetSelectedTabContents() &&
          &GetSelectedTabContents()->controller() ==
          content::Source<NavigationController>(source).ptr())
        UpdateToolbar(false);
      break;

    case chrome::NOTIFICATION_EXTENSION_UPDATE_DISABLED: {
      // Show the UI if the extension was disabled for escalated permissions.
      Profile* profile = content::Source<Profile>(source).ptr();
      if (profile_->IsSameProfile(profile)) {
        ExtensionService* service = profile->GetExtensionService();
        DCHECK(service);
        const Extension* extension =
            content::Details<const Extension>(details).ptr();
        if (service->extension_prefs()->DidExtensionEscalatePermissions(
                extension->id()))
          ShowExtensionDisabledUI(service, profile_, extension);
      }
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      if (window()->GetLocationBar())
        window()->GetLocationBar()->UpdatePageActions();

      // Close any tabs from the unloaded extension, unless it's terminated,
      // in which case let the sad tabs remain.
      if (content::Details<UnloadedExtensionInfo>(details)->reason !=
          extension_misc::UNLOAD_REASON_TERMINATE) {
        const Extension* extension =
            content::Details<UnloadedExtensionInfo>(details)->extension;
        TabStripModel* model = tab_handler_->GetTabStripModel();
        for (int i = model->count() - 1; i >= 0; --i) {
          TabContents* tc = model->GetTabContentsAt(i)->tab_contents();
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

    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      window()->UserChangedTheme();
      break;

    case chrome::NOTIFICATION_PREF_CHANGED: {
      const std::string& pref_name =
          *content::Details<std::string>(details).ptr();
      if (pref_name == prefs::kPrintingEnabled) {
        UpdatePrintingState(GetContentRestrictionsForSelectedTab());
      } else if (pref_name == prefs::kInstantEnabled ||
                 pref_name == prefs::kMetricsReportingEnabled ||
                 pref_name == prefs::kSearchSuggestEnabled) {
        if (!InstantController::IsEnabled(profile())) {
          if (instant()) {
            instant()->DestroyPreviewContents();
            instant_.reset();
            instant_unload_handler_.reset();
          }
        } else {
          CreateInstantIfNecessary();
        }
      } else if (pref_name == prefs::kIncognitoModeAvailability) {
        UpdateCommandsForIncognitoAvailability();
      } else if (pref_name == prefs::kDevToolsDisabled) {
        UpdateCommandsForDevTools();
        if (profile_->GetPrefs()->GetBoolean(prefs::kDevToolsDisabled))
          content::DevToolsManager::GetInstance()->CloseAllClientHosts();
      } else if (pref_name == prefs::kEditBookmarksEnabled) {
        UpdateCommandsForBookmarkEditing();
      } else if (pref_name == prefs::kShowBookmarkBar) {
        UpdateCommandsForBookmarkBar();
        UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE);
      } else if (pref_name == prefs::kHomePage) {
        PrefService* pref_service = content::Source<PrefService>(source).ptr();
        MarkHomePageAsChanged(pref_service);
      } else if (pref_name == prefs::kAllowFileSelectionDialogs) {
        UpdateSaveAsState(GetContentRestrictionsForSelectedTab());
        UpdateOpenFileState();
      } else {
        NOTREACHED();
      }
      break;
    }

    case chrome::NOTIFICATION_TAB_CONTENT_SETTINGS_CHANGED: {
      TabContents* tab_contents = content::Source<TabContents>(source).ptr();
      if (tab_contents == GetSelectedTabContents()) {
        LocationBar* location_bar = window()->GetLocationBar();
        if (location_bar)
          location_bar->UpdateContentSettingsIcons();
      }
      break;
    }

    case content::NOTIFICATION_INTERSTITIAL_ATTACHED:
      UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
      break;

    case chrome::NOTIFICATION_FULLSCREEN_CHANGED:
      UpdateCommandsForFullscreenMode(window_->IsFullscreen());
      UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TOGGLE_FULLSCREEN);
      break;

    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ProfileSyncServiceObserver implementation:

void Browser::OnStateChanged() {
  DCHECK(profile_->GetProfileSyncService());

  const bool show_main_ui = IsShowingMainUI(window_->IsFullscreen());
  command_updater_.UpdateCommandEnabled(IDC_SYNC_BOOKMARKS,
      show_main_ui && profile_->GetOriginalProfile()->IsSyncAccessible());
}

///////////////////////////////////////////////////////////////////////////////
// Browser, InstantDelegate implementation:

void Browser::ShowInstant(TabContentsWrapper* preview_contents) {
  DCHECK(instant_->tab_contents() == GetSelectedTabContentsWrapper());
  window_->ShowInstant(preview_contents);

  GetSelectedTabContents()->HideContents();
  preview_contents->tab_contents()->ShowContents();
}

void Browser::HideInstant() {
  window_->HideInstant();
  if (GetSelectedTabContents())
    GetSelectedTabContents()->ShowContents();
  if (instant_->GetPreviewContents())
    instant_->GetPreviewContents()->tab_contents()->HideContents();
}

void Browser::CommitInstant(TabContentsWrapper* preview_contents) {
  TabContentsWrapper* tab_contents = instant_->tab_contents();
  int index =
      tab_handler_->GetTabStripModel()->GetIndexOfTabContents(tab_contents);
  DCHECK_NE(TabStripModel::kNoTab, index);
  // TabStripModel takes ownership of preview_contents.
  tab_handler_->GetTabStripModel()->ReplaceTabContentsAt(
      index, preview_contents);
  // InstantUnloadHandler takes ownership of tab_contents.
  instant_unload_handler_->RunUnloadListenersOrDestroy(tab_contents, index);

  GURL url = preview_contents->tab_contents()->GetURL();
  DCHECK(profile_->GetExtensionService());
  if (profile_->GetExtensionService()->IsInstalledApp(url)) {
    UMA_HISTOGRAM_ENUMERATION(extension_misc::kAppLaunchHistogram,
                              extension_misc::APP_LAUNCH_OMNIBOX_INSTANT,
                              extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);
  }
}

void Browser::SwapTabContents(TabContentsWrapper* old_tab_contents,
                              TabContentsWrapper* new_tab_contents) {
  int index =
      tab_handler_->GetTabStripModel()->GetIndexOfTabContents(old_tab_contents);
  DCHECK_NE(TabStripModel::kNoTab, index);
  tab_handler_->GetTabStripModel()->ReplaceTabContentsAt(index,
                                                         new_tab_contents);
}

void Browser::SetTabContentBlocked(TabContentsWrapper* wrapper, bool blocked) {
  int index = tabstrip_model()->GetIndexOfTabContents(wrapper);
  if (index == TabStripModel::kNoTab) {
    NOTREACHED();
    return;
  }
  tabstrip_model()->SetTabBlocked(index, blocked);
  UpdatePrintingState(wrapper->tab_contents()->content_restrictions());
}

void Browser::SetSuggestedText(const string16& text,
                               InstantCompleteBehavior behavior) {
  if (window()->GetLocationBar())
    window()->GetLocationBar()->SetSuggestedText(text, behavior);
}

gfx::Rect Browser::GetInstantBounds() {
  return window()->GetInstantBounds();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, protected:

BrowserWindow* Browser::CreateBrowserWindow() {
#if !defined(OS_CHROMEOS) || defined(USE_AURA)
  if (type_ == TYPE_PANEL) {
    DCHECK(!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisablePanels));
    return PanelManager::GetInstance()->CreatePanel(this);
  }
#endif

  return BrowserWindow::CreateBrowserWindow(this);
}


///////////////////////////////////////////////////////////////////////////////
// Browser, Command and state updating (private):

void Browser::InitCommandState() {
  // All browser commands whose state isn't set automagically some other way
  // (like Back & Forward with initial page load) must have their state
  // initialized here, otherwise they will be forever disabled.

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_RELOAD, true);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_IGNORING_CACHE, true);

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_WINDOW, true);
  command_updater_.UpdateCommandEnabled(IDC_NEW_TAB, true);
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_TAB, true);
  command_updater_.UpdateCommandEnabled(IDC_DUPLICATE_TAB, true);
  command_updater_.UpdateCommandEnabled(IDC_RESTORE_TAB, false);
  command_updater_.UpdateCommandEnabled(IDC_EXIT, true);
  command_updater_.UpdateCommandEnabled(IDC_DEBUG_FRAME_TOGGLE, true);

  // Page-related commands
  command_updater_.UpdateCommandEnabled(IDC_EMAIL_PAGE_LOCATION, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_AUTO_DETECT, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_UTF8, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_UTF16LE, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88591, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1252, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_GBK, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_GB18030, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_BIG5HKSCS, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_BIG5, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_THAI, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_KOREAN, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_SHIFTJIS, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO2022JP, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_EUCJP, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885915, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_MACINTOSH, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88592, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1250, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88595, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1251, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_KOI8R, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_KOI8U, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88597, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1253, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88594, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885913, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1257, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88593, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885910, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885914, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO885916, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1254, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88596, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1256, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88598, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_ISO88598I, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1255, true);
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_WINDOWS1258, true);

  // Zoom
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_PLUS, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_NORMAL, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MINUS, true);

  // Show various bits of UI
  UpdateOpenFileState();
  command_updater_.UpdateCommandEnabled(IDC_CREATE_SHORTCUTS, false);
  UpdateCommandsForDevTools();
  command_updater_.UpdateCommandEnabled(IDC_TASK_MANAGER, true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_HISTORY, true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_PAGE, true);
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARKS_MENU, true);

#if defined(OS_CHROMEOS)
  command_updater_.UpdateCommandEnabled(IDC_LOCK_SCREEN, true);
  command_updater_.UpdateCommandEnabled(IDC_FILE_MANAGER, true);
  command_updater_.UpdateCommandEnabled(IDC_SEARCH, true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_KEYBOARD_OVERLAY, true);
  command_updater_.UpdateCommandEnabled(IDC_SYSTEM_OPTIONS, true);
  command_updater_.UpdateCommandEnabled(IDC_INTERNET_OPTIONS, true);
#endif
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_SYNC_SETUP, profile_->GetOriginalProfile()->IsSyncAccessible());

  // Initialize other commands based on the window type.
  bool normal_window = is_type_tabbed();

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_HOME, normal_window);

  // Window management commands
  // TODO(rohitrao): Disable fullscreen on non-Lion?
  command_updater_.UpdateCommandEnabled(IDC_FULLSCREEN,
      !(is_type_panel() && is_app()));
  command_updater_.UpdateCommandEnabled(IDC_SELECT_NEXT_TAB, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_PREVIOUS_TAB,
                                        normal_window);
  command_updater_.UpdateCommandEnabled(IDC_MOVE_TAB_NEXT, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_MOVE_TAB_PREVIOUS, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_0, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_1, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_2, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_3, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_4, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_5, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_6, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_TAB_7, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_LAST_TAB, normal_window);
#if defined(OS_MACOSX)
  command_updater_.UpdateCommandEnabled(IDC_TABPOSE, normal_window);
  command_updater_.UpdateCommandEnabled(IDC_PRESENTATION_MODE,
      !(is_type_panel() && is_app()));
#endif

  // Clipboard commands
  command_updater_.UpdateCommandEnabled(IDC_COPY_URL, !is_devtools());

  // Find-in-page
  command_updater_.UpdateCommandEnabled(IDC_FIND, !is_devtools());
  command_updater_.UpdateCommandEnabled(IDC_FIND_NEXT, !is_devtools());
  command_updater_.UpdateCommandEnabled(IDC_FIND_PREVIOUS, !is_devtools());

  // Show various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_CLEAR_BROWSING_DATA, normal_window);

  // The upgrade entry and the view incompatibility entry should always be
  // enabled. Whether they are visible is a separate matter determined on menu
  // show.
  command_updater_.UpdateCommandEnabled(IDC_UPGRADE_DIALOG, true);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_INCOMPATIBILITIES, true);

  // View Background Pages entry is always enabled, but is hidden if there are
  // no background pages.
  command_updater_.UpdateCommandEnabled(IDC_VIEW_BACKGROUND_PAGES, true);

  // Toggle speech input
  command_updater_.UpdateCommandEnabled(IDC_TOGGLE_SPEECH_INPUT, true);

  // Initialize other commands whose state changes based on fullscreen mode.
  UpdateCommandsForFullscreenMode(false);

  UpdateCommandsForContentRestrictionState();

  UpdateCommandsForBookmarkEditing();

  UpdateCommandsForIncognitoAvailability();
}

bool Browser::IsShowingMainUI(bool is_fullscreen) {
#if !defined(OS_MACOSX)
  return is_type_tabbed() && !is_fullscreen;
#else
  return is_type_tabbed();
#endif
}

void Browser::UpdateCommandsForIncognitoAvailability() {
  IncognitoModePrefs::Availability incognito_availability =
      IncognitoModePrefs::GetAvailability(profile_->GetPrefs());
  command_updater_.UpdateCommandEnabled(
      IDC_NEW_WINDOW,
      incognito_availability != IncognitoModePrefs::FORCED);
  command_updater_.UpdateCommandEnabled(
      IDC_NEW_INCOGNITO_WINDOW,
      incognito_availability != IncognitoModePrefs::DISABLED);

  // Bookmark manager and settings page/subpages are forced to open in normal
  // mode. For this reason we disable these commands when incognito is forced.
  const bool command_enabled =
      incognito_availability != IncognitoModePrefs::FORCED;
  command_updater_.UpdateCommandEnabled(
      IDC_SHOW_BOOKMARK_MANAGER,
      browser_defaults::bookmarks_enabled && command_enabled);
  ExtensionService* extension_service = profile()->GetExtensionService();
  bool enable_extensions =
      extension_service && extension_service->extensions_enabled();
  command_updater_.UpdateCommandEnabled(IDC_MANAGE_EXTENSIONS,
                                        enable_extensions && command_enabled);

  const bool show_main_ui = IsShowingMainUI(window_ && window_->IsFullscreen());
  command_updater_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS,
                                        show_main_ui && command_enabled);
  command_updater_.UpdateCommandEnabled(IDC_OPTIONS,
                                        show_main_ui && command_enabled);
}

void Browser::UpdateCommandsForFullscreenMode(bool is_fullscreen) {
  const bool show_main_ui = IsShowingMainUI(is_fullscreen);
  bool main_not_fullscreen = show_main_ui && !is_fullscreen;

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_OPEN_CURRENT_URL, show_main_ui);

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_SHOW_AS_TAB,
      type_ != TYPE_TABBED && !is_fullscreen);

  // Focus various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_TOOLBAR, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_LOCATION, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_SEARCH, show_main_ui);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_MENU_BAR, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_NEXT_PANE, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_PREVIOUS_PANE, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_BOOKMARKS, main_not_fullscreen);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_CHROMEOS_STATUS, main_not_fullscreen);

  // Show various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_DEVELOPER_MENU, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FEEDBACK, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SYNC_BOOKMARKS,
      show_main_ui && profile_->GetOriginalProfile()->IsSyncAccessible());

  // Settings page/subpages are forced to open in normal mode. We disable these
  // commands when incognito is forced.
  const bool options_enabled = show_main_ui &&
      IncognitoModePrefs::GetAvailability(
          profile_->GetPrefs()) != IncognitoModePrefs::FORCED;
  command_updater_.UpdateCommandEnabled(IDC_OPTIONS, options_enabled);
  command_updater_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS, options_enabled);

  command_updater_.UpdateCommandEnabled(IDC_EDIT_SEARCH_ENGINES, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_PASSWORDS, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_ABOUT, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_APP_MENU, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_AVATAR_MENU,
      show_main_ui && !profile()->IsOffTheRecord());
#if defined (ENABLE_PROFILING) && !defined(NO_TCMALLOC)
  command_updater_.UpdateCommandEnabled(IDC_PROFILING_ENABLED, show_main_ui);
#endif

  UpdateCommandsForBookmarkBar();
}

void Browser::UpdateCommandsForTabState() {
  TabContents* current_tab = GetSelectedTabContents();
  TabContentsWrapper* current_tab_wrapper = GetSelectedTabContentsWrapper();
  if (!current_tab || !current_tab_wrapper)  // May be NULL during tab restore.
    return;

  // Navigation commands
  NavigationController& nc = current_tab->controller();
  command_updater_.UpdateCommandEnabled(IDC_BACK, nc.CanGoBack());
  command_updater_.UpdateCommandEnabled(IDC_FORWARD, nc.CanGoForward());
  command_updater_.UpdateCommandEnabled(IDC_RELOAD,
                                        CanReloadContents(current_tab));
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_IGNORING_CACHE,
                                        CanReloadContents(current_tab));

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_DUPLICATE_TAB,
      !is_app() && CanDuplicateContentsAt(active_index()));

  // Page-related commands
  window_->SetStarredState(
      current_tab_wrapper->bookmark_tab_helper()->is_starred());
  command_updater_.UpdateCommandEnabled(IDC_VIEW_SOURCE,
      current_tab->controller().CanViewSource());
  command_updater_.UpdateCommandEnabled(IDC_EMAIL_PAGE_LOCATION,
      toolbar_model_.ShouldDisplayURL() && current_tab->GetURL().is_valid());
  if (is_devtools())
      command_updater_.UpdateCommandEnabled(IDC_OPEN_FILE, false);

  // Changing the encoding is not possible on Chrome-internal webpages.
  bool is_chrome_internal = HasInternalURL(nc.GetActiveEntry());
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_MENU,
      !is_chrome_internal && SavePackage::IsSavableContents(
          current_tab->contents_mime_type()));

  // Show various bits of UI
  // TODO(pinkerton): Disable app-mode in the model until we implement it
  // on the Mac. Be sure to remove both ifdefs. http://crbug.com/13148
#if !defined(OS_MACOSX)
  command_updater_.UpdateCommandEnabled(IDC_CREATE_SHORTCUTS,
      web_app::IsValidUrl(current_tab->GetURL()));
#endif

  UpdateCommandsForContentRestrictionState();
  UpdateCommandsForBookmarkEditing();
}

void Browser::UpdateCommandsForContentRestrictionState() {
  int restrictions = GetContentRestrictionsForSelectedTab();

  command_updater_.UpdateCommandEnabled(
      IDC_COPY, !(restrictions & content::CONTENT_RESTRICTION_COPY));
  command_updater_.UpdateCommandEnabled(
      IDC_CUT, !(restrictions & content::CONTENT_RESTRICTION_CUT));
  command_updater_.UpdateCommandEnabled(
      IDC_PASTE, !(restrictions & content::CONTENT_RESTRICTION_PASTE));
  UpdateSaveAsState(restrictions);
  UpdatePrintingState(restrictions);
}

void Browser::UpdatePrintingState(int content_restrictions) {
  bool print_enabled = true;
  bool advanced_print_enabled = true;
  if (g_browser_process->local_state()) {
    print_enabled =
        g_browser_process->local_state()->GetBoolean(prefs::kPrintingEnabled);
    advanced_print_enabled = print_enabled;
  }
  if (print_enabled) {
    // Do not print when a constrained window is showing. It's confusing.
    TabContentsWrapper* wrapper = GetSelectedTabContentsWrapper();
    bool has_constrained_window = (wrapper &&
        wrapper->constrained_window_tab_helper()->constrained_window_count());
    if (has_constrained_window ||
        content_restrictions & content::CONTENT_RESTRICTION_PRINT) {
      print_enabled = false;
      advanced_print_enabled = false;
    }

    // The exception is print preview,
    // where advanced printing is always enabled.
    printing::PrintPreviewTabController* controller =
        printing::PrintPreviewTabController::GetInstance();
    if (controller && (controller->GetPrintPreviewForTab(wrapper) ||
                       controller->is_creating_print_preview_tab())) {
      advanced_print_enabled = true;
    }
  }
  command_updater_.UpdateCommandEnabled(IDC_PRINT, print_enabled);
  command_updater_.UpdateCommandEnabled(IDC_ADVANCED_PRINT,
                                        advanced_print_enabled);
}

void Browser::UpdateReloadStopState(bool is_loading, bool force) {
  window_->UpdateReloadStopState(is_loading, force);
  command_updater_.UpdateCommandEnabled(IDC_STOP, is_loading);
}

void Browser::UpdateCommandsForDevTools() {
  bool dev_tools_enabled =
      !profile_->GetPrefs()->GetBoolean(prefs::kDevToolsDisabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_CONSOLE,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_INSPECT,
                                        dev_tools_enabled);
}

void Browser::UpdateCommandsForBookmarkEditing() {
  bool enabled =
      profile_->GetPrefs()->GetBoolean(prefs::kEditBookmarksEnabled) &&
      browser_defaults::bookmarks_enabled;

  command_updater_.UpdateCommandEnabled(IDC_BOOKMARK_PAGE,
      enabled && is_type_tabbed());
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARK_ALL_TABS,
      enabled && CanBookmarkAllTabs());
}

void Browser::UpdateCommandsForBookmarkBar() {
  const bool show_main_ui = IsShowingMainUI(window_ && window_->IsFullscreen());
  command_updater_.UpdateCommandEnabled(IDC_SHOW_BOOKMARK_BAR,
      browser_defaults::bookmarks_enabled &&
      !profile_->GetPrefs()->IsManagedPreference(prefs::kShowBookmarkBar) &&
      show_main_ui);
}

void Browser::MarkHomePageAsChanged(PrefService* pref_service) {
  pref_service->SetBoolean(prefs::kHomePageChanged, true);
  pref_service->ScheduleSavePersistentPrefs();
}

void Browser::UpdateSaveAsState(int content_restrictions) {
  bool enabled = !(content_restrictions & content::CONTENT_RESTRICTION_SAVE);
  PrefService* state = g_browser_process->local_state();
  if (state)
    enabled = enabled && state->GetBoolean(prefs::kAllowFileSelectionDialogs);

  command_updater_.UpdateCommandEnabled(IDC_SAVE_PAGE, enabled);
}

void Browser::UpdateOpenFileState() {
  bool enabled = true;
  PrefService* local_state = g_browser_process->local_state();
  if (local_state)
    enabled = local_state->GetBoolean(prefs::kAllowFileSelectionDialogs);

  command_updater_.UpdateCommandEnabled(IDC_OPEN_FILE, enabled);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, UI update coalescing and handling (private):

void Browser::UpdateToolbar(bool should_restore_state) {
  window_->UpdateToolbar(GetSelectedTabContentsWrapper(), should_restore_state);
}

void Browser::ScheduleUIUpdate(const TabContents* source,
                               unsigned changed_flags) {
  if (!source)
    return;

  // Do some synchronous updates.
  if (changed_flags & TabContents::INVALIDATE_URL &&
      source == GetSelectedTabContents()) {
    // Only update the URL for the current tab. Note that we do not update
    // the navigation commands since those would have already been updated
    // synchronously by NavigationStateChanged.
    UpdateToolbar(false);
    changed_flags &= ~TabContents::INVALIDATE_URL;
  }
  if (changed_flags & TabContents::INVALIDATE_LOAD) {
    // Update the loading state synchronously. This is so the throbber will
    // immediately start/stop, which gives a more snappy feel. We want to do
    // this for any tab so they start & stop quickly.
    tab_handler_->GetTabStripModel()->UpdateTabContentsStateAt(
        tab_handler_->GetTabStripModel()->GetIndexOfController(
            &source->controller()),
        TabStripModelObserver::LOADING_ONLY);
    // The status bubble needs to be updated during INVALIDATE_LOAD too, but
    // we do that asynchronously by not stripping INVALIDATE_LOAD from
    // changed_flags.
  }

  if (changed_flags & TabContents::INVALIDATE_TITLE && !source->IsLoading()) {
    // To correctly calculate whether the title changed while not loading
    // we need to process the update synchronously. This state only matters for
    // the TabStripModel, so we notify the TabStripModel now and notify others
    // asynchronously.
    tab_handler_->GetTabStripModel()->UpdateTabContentsStateAt(
        tab_handler_->GetTabStripModel()->GetIndexOfController(
            &source->controller()),
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
        kUIUpdateCoalescingTimeMS);
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
      if (GetTabContentsAt(tab) == i->first) {
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
    const TabContents* contents = i->first;
    unsigned flags = i->second;

    if (contents == GetSelectedTabContents()) {
      // Updates that only matter when the tab is selected go here.

      if (flags & TabContents::INVALIDATE_PAGE_ACTIONS) {
        LocationBar* location_bar = window()->GetLocationBar();
        if (location_bar)
          location_bar->UpdatePageActions();
      }
      // Updating the URL happens synchronously in ScheduleUIUpdate.
      if (flags & TabContents::INVALIDATE_LOAD && GetStatusBubble()) {
        GetStatusBubble()->SetStatus(
            GetSelectedTabContentsWrapper()->GetStatusText());
      }

      if (flags & (TabContents::INVALIDATE_TAB |
                   TabContents::INVALIDATE_TITLE)) {
// TODO(pinkerton): Disable app-mode in the model until we implement it
// on the Mac. Be sure to remove both ifdefs. http://crbug.com/13148
#if !defined(OS_MACOSX)
        command_updater_.UpdateCommandEnabled(IDC_CREATE_SHORTCUTS,
            web_app::IsValidUrl(contents->GetURL()));
#endif
        window_->UpdateTitleBar();
      }
    }

    // Updates that don't depend upon the selected state go here.
    if (flags & (TabContents::INVALIDATE_TAB | TabContents::INVALIDATE_TITLE)) {
      tab_handler_->GetTabStripModel()->UpdateTabContentsStateAt(
          tab_handler_->GetTabStripModel()->GetWrapperIndex(contents),
          TabStripModelObserver::ALL);
    }

    // We don't need to process INVALIDATE_STATE, since that's not visible.
  }

  scheduled_updates_.clear();
}

void Browser::RemoveScheduledUpdatesFor(TabContents* contents) {
  if (!contents)
    return;

  UpdateMap::iterator i = scheduled_updates_.find(contents);
  if (i != scheduled_updates_.end())
    scheduled_updates_.erase(i);
}


///////////////////////////////////////////////////////////////////////////////
// Browser, Getters for UI (private):

StatusBubble* Browser::GetStatusBubble() {
#if !defined(OS_MACOSX)
  // In kiosk mode, we want to always hide the status bubble.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return NULL;
#endif
  return window_ ? window_->GetStatusBubble() : NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Session restore functions (private):

void Browser::SyncHistoryWithTabs(int index) {
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile());
  if (session_service) {
    for (int i = index; i < tab_count(); ++i) {
      TabContentsWrapper* tab = GetTabContentsWrapperAt(i);
      if (tab) {
        session_service->SetTabIndexInWindow(
            session_id(), tab->restore_tab_helper()->session_id(), i);
        session_service->SetPinnedState(
            session_id(),
            tab->restore_tab_helper()->session_id(),
            tab_handler_->GetTabStripModel()->IsTabPinned(i));
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
    TabContents* tab = *(tabs_needing_before_unload_fired_.begin());
    // Null check render_view_host here as this gets called on a PostTask and
    // the tab's render_view_host may have been nulled out.
    if (tab->render_view_host()) {
      tab->render_view_host()->FirePageBeforeUnload(false);
    } else {
      ClearUnloadState(tab, true);
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
    TabContents* tab = *(tabs_needing_unload_fired_.begin());
    // Null check render_view_host here as this gets called on a PostTask and
    // the tab's render_view_host may have been nulled out.
    if (tab->render_view_host()) {
      tab->render_view_host()->ClosePage();
    } else {
      ClearUnloadState(tab, true);
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
  // Closing of window can be canceled from:
  // - canceling beforeunload
  // - disallowing closing from IsClosingPermitted.
  DCHECK(is_attempting_to_close_browser_);
  tabs_needing_before_unload_fired_.clear();
  tabs_needing_unload_fired_.clear();
  is_attempting_to_close_browser_ = false;

  // Inform TabCloseableStateWatcher that closing of window has been canceled.
  TabCloseableStateWatcher* watcher =
      g_browser_process->tab_closeable_state_watcher();
  if (watcher)
    watcher->OnWindowCloseCanceled(this);
}

bool Browser::RemoveFromSet(UnloadListenerSet* set, TabContents* tab) {
  DCHECK(is_attempting_to_close_browser_);

  UnloadListenerSet::iterator iter = std::find(set->begin(), set->end(), tab);
  if (iter != set->end()) {
    set->erase(iter);
    return true;
  }
  return false;
}

void Browser::ClearUnloadState(TabContents* tab, bool process_now) {
  // Closing of browser could be canceled (via IsClosingPermitted) between the
  // time when request was initiated and when this method is called, so check
  // for is_attempting_to_close_browser_ flag before proceeding.
  if (is_attempting_to_close_browser_) {
    RemoveFromSet(&tabs_needing_before_unload_fired_, tab);
    RemoveFromSet(&tabs_needing_unload_fired_, tab);
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

// static
Browser* Browser::GetTabbedBrowser(Profile* profile, bool match_incognito) {
  return BrowserList::FindTabbedBrowser(profile, match_incognito);
}

// static
Browser* Browser::GetOrCreateTabbedBrowser(Profile* profile) {
  Browser* browser = GetTabbedBrowser(profile, false);
  if (!browser)
    browser = Browser::Create(profile);
  return browser;
}

void Browser::SetAsDelegate(TabContentsWrapper* tab, Browser* delegate) {
  // TabContents...
  tab->tab_contents()->set_delegate(delegate);
  tab->set_delegate(delegate);

  // ...and all the helpers.
  tab->blocked_content_tab_helper()->set_delegate(delegate);
  tab->bookmark_tab_helper()->set_delegate(delegate);
  tab->constrained_window_tab_helper()->set_delegate(delegate);
  tab->extension_tab_helper()->set_delegate(delegate);
  tab->search_engine_tab_helper()->set_delegate(delegate);
}

void Browser::FindInPage(bool find_next, bool forward_direction) {
  ShowFindBar();
  if (find_next) {
    string16 find_text;
#if defined(OS_MACOSX)
    // We always want to search for the contents of the find pasteboard on OS X.
    find_text = GetFindPboardText();
#endif
    GetSelectedTabContentsWrapper()->
        find_tab_helper()->StartFinding(find_text,
                                        forward_direction,
                                        false);  // Not case sensitive.
  }
}

void Browser::CloseFrame() {
  window_->Close();
}

void Browser::TabDetachedAtImpl(TabContentsWrapper* contents, int index,
                                DetachType type) {
  if (type == DETACH_TYPE_DETACH) {
    // Save the current location bar state, but only if the tab being detached
    // is the selected tab.  Because saving state can conditionally revert the
    // location bar, saving the current tab's location bar state to a
    // non-selected tab can corrupt both tabs.
    if (contents == GetSelectedTabContentsWrapper()) {
      LocationBar* location_bar = window()->GetLocationBar();
      if (location_bar)
        location_bar->SaveStateToContents(contents->tab_contents());
    }

    if (!tab_handler_->GetTabStripModel()->closing_all())
      SyncHistoryWithTabs(0);
  }

  SetAsDelegate(contents, NULL);
  RemoveScheduledUpdatesFor(contents->tab_contents());

  if (find_bar_controller_.get() &&
      index == tab_handler_->GetTabStripModel()->active_index()) {
    find_bar_controller_->ChangeTabContents(NULL);
  }

  if (is_attempting_to_close_browser_) {
    // If this is the last tab with unload handlers, then ProcessPendingTabs
    // would call back into the TabStripModel (which is invoking this method on
    // us). Avoid that by passing in false so that the call to
    // ProcessPendingTabs is delayed.
    ClearUnloadState(contents->tab_contents(), false);
  }

  registrar_.Remove(this, content::NOTIFICATION_INTERSTITIAL_ATTACHED,
                    content::Source<TabContents>(contents->tab_contents()));
  registrar_.Remove(this, content::NOTIFICATION_TAB_CONTENTS_DISCONNECTED,
                    content::Source<TabContents>(contents->tab_contents()));
}

// static
void Browser::RegisterAppPrefs(const std::string& app_name, Profile* profile) {
  // We need to register the window position pref.
  std::string window_pref(prefs::kBrowserWindowPlacement);
  window_pref.append("_");
  window_pref.append(app_name);
  PrefService* prefs = profile->GetPrefs();
  if (!prefs->FindPreference(window_pref.c_str())) {
    prefs->RegisterDictionaryPref(window_pref.c_str(),
                                  PrefService::UNSYNCABLE_PREF);
  }
}

void Browser::TabRestoreServiceChanged(TabRestoreService* service) {
  command_updater_.UpdateCommandEnabled(IDC_RESTORE_TAB,
                                        !service->entries().empty());
}

void Browser::TabRestoreServiceDestroyed(TabRestoreService* service) {
  if (!tab_restore_service_)
    return;

  DCHECK_EQ(tab_restore_service_, service);
  tab_restore_service_->RemoveObserver(this);
  tab_restore_service_ = NULL;
}

// Centralized method for creating a TabContents, configuring and installing
// all its supporting objects and observers.
TabContentsWrapper* Browser::TabContentsFactory(
    Profile* profile,
    SiteInstance* site_instance,
    int routing_id,
    const TabContents* base_tab_contents,
    SessionStorageNamespace* session_storage_namespace) {
  TabContents* new_contents = new TabContents(profile, site_instance,
                                              routing_id, base_tab_contents,
                                              session_storage_namespace);
  TabContentsWrapper* wrapper = new TabContentsWrapper(new_contents);
  return wrapper;
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
        content::Source<TabContentsWrapper>(instant()->CommitCurrentPreview(
            INSTANT_COMMIT_PRESSED_ENTER)),
        content::NotificationService::NoDetails());
    return true;
  }
  if (disposition == NEW_FOREGROUND_TAB) {
    TabContentsWrapper* preview_contents = instant()->ReleasePreviewContents(
        INSTANT_COMMIT_PRESSED_ENTER);
    // HideInstant is invoked after release so that InstantController is not
    // active when HideInstant asks it for its state.
    HideInstant();
    preview_contents->controller().PruneAllButActive();
    tab_handler_->GetTabStripModel()->AddTabContents(
        preview_contents,
        -1,
        instant()->last_transition_type(),
        TabStripModel::ADD_ACTIVE);
    instant()->CompleteRelease(preview_contents);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_INSTANT_COMMITTED,
        content::Source<TabContentsWrapper>(preview_contents),
        content::NotificationService::NoDetails());
    return true;
  }
  // The omnibox currently doesn't use other dispositions, so we don't attempt
  // to handle them. If you hit this NOTREACHED file a bug and I'll (sky) add
  // support for the new disposition.
  NOTREACHED();
  return false;
}

void Browser::CreateInstantIfNecessary() {
  if (is_type_tabbed() && InstantController::IsEnabled(profile()) &&
      !profile()->IsOffTheRecord()) {
    instant_.reset(new InstantController(profile_, this));
    instant_unload_handler_.reset(new InstantUnloadHandler(this));
  }
}

void Browser::ViewSource(TabContentsWrapper* contents) {
  DCHECK(contents);

  NavigationEntry* active_entry = contents->controller().GetActiveEntry();
  if (!active_entry)
    return;

  ViewSource(contents, active_entry->url(), active_entry->content_state());
}

void Browser::ViewSource(TabContentsWrapper* contents,
                         const GURL& url,
                         const std::string& content_state) {
  UserMetrics::RecordAction(UserMetricsAction("ViewSource"));
  DCHECK(contents);

  TabContentsWrapper* view_source_contents = contents->Clone();
  view_source_contents->controller().PruneAllButActive();
  NavigationEntry* active_entry =
      view_source_contents->controller().GetActiveEntry();
  if (!active_entry)
    return;

  GURL view_source_url = GURL(chrome::kViewSourceScheme + std::string(":") +
      url.spec());
  active_entry->set_virtual_url(view_source_url);

  // Do not restore scroller position.
  active_entry->set_content_state(
      webkit_glue::RemoveScrollOffsetFromHistoryState(content_state));

  // Do not restore title, derive it from the url.
  active_entry->set_title(string16());

  // Now show view-source entry.
  if (CanSupportWindowFeature(FEATURE_TABSTRIP)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    int index = tab_handler_->GetTabStripModel()->
        GetIndexOfTabContents(contents);
    int add_types = TabStripModel::ADD_ACTIVE |
        TabStripModel::ADD_INHERIT_GROUP;
    tab_handler_->GetTabStripModel()->InsertTabContentsAt(index + 1,
                                                          view_source_contents,
                                                          add_types);
  } else {
    Browser* browser = Browser::CreateForType(TYPE_TABBED, profile_);

    // Preserve the size of the original window. The new window has already
    // been given an offset by the OS, so we shouldn't copy the old bounds.
    BrowserWindow* new_window = browser->window();
    new_window->SetBounds(gfx::Rect(new_window->GetRestoredBounds().origin(),
                          window()->GetRestoredBounds().size()));

    // We need to show the browser now. Otherwise ContainerWin assumes the
    // TabContents is invisible and won't size it.
    browser->window()->Show();

    // The page transition below is only for the purpose of inserting the tab.
    browser->AddTab(view_source_contents, content::PAGE_TRANSITION_LINK);
  }

  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile_);
  if (session_service)
    session_service->TabRestored(view_source_contents, false);
}

int Browser::GetContentRestrictionsForSelectedTab() {
  int content_restrictions = 0;
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab) {
    content_restrictions = current_tab->content_restrictions();
    NavigationEntry* active_entry = current_tab->controller().GetActiveEntry();
    // See comment in UpdateCommandsForTabState about why we call url().
    if (!SavePackage::IsSavableURL(active_entry ? active_entry->url() : GURL()))
      content_restrictions |= content::CONTENT_RESTRICTION_SAVE;
  }
  return content_restrictions;
}

void Browser::UpdateBookmarkBarState(BookmarkBarStateChangeReason reason) {
  BookmarkBar::State state;
  // The bookmark bar is hidden in fullscreen mode, unless on the new tab page.
  if (browser_defaults::bookmarks_enabled &&
      profile_->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar) &&
      (!window_ || !window_->IsFullscreen())) {
    state = BookmarkBar::SHOW;
  } else {
    TabContentsWrapper* tab = GetSelectedTabContentsWrapper();
    if (tab && tab->bookmark_tab_helper()->ShouldShowBookmarkBar())
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

  BookmarkBar::AnimateChangeType animate_type =
      (reason == BOOKMARK_BAR_STATE_CHANGE_PREF_CHANGE) ?
      BookmarkBar::ANIMATE_STATE_CHANGE :
      BookmarkBar::DONT_ANIMATE_STATE_CHANGE;
  window_->BookmarkBarStateChanged(animate_type);
}

void Browser::ShowSyncSetup() {
  ProfileSyncService* service =
      profile()->GetOriginalProfile()->GetProfileSyncService();
  if (service->HasSyncSetupCompleted())
    ShowOptionsTab(chrome::kPersonalOptionsSubPage);
  else
    service->ShowLoginDialog();
}

void Browser::ToggleSpeechInput() {
  GetSelectedTabContentsWrapper()->render_view_host()->ToggleSpeechInput();
}

void Browser::OnWindowDidShow() {
  if (window_has_shown_)
    return;
  window_has_shown_ = true;

  // Nothing to do for non-tabbed windows.
  if (!is_type_tabbed())
    return;

  // Suppress the first run bubble if we're showing the sync promo.
  TabContents* contents = GetSelectedTabContents();
  bool is_showing_promo = contents &&
      contents->GetURL().SchemeIs(chrome::kChromeUIScheme) &&
      contents->GetURL().host() == chrome::kChromeUISyncPromoHost;

  // Show the First Run information bubble if we've been told to.
  PrefService* local_state = g_browser_process->local_state();
  if (!is_showing_promo && local_state &&
      local_state->GetBoolean(prefs::kShouldShowFirstRunBubble)) {
    FirstRun::BubbleType bubble_type = FirstRun::MINIMAL_BUBBLE;
    if (local_state->
        FindPreference(prefs::kShouldUseOEMFirstRunBubble) &&
        local_state->GetBoolean(prefs::kShouldUseOEMFirstRunBubble)) {
      bubble_type = FirstRun::OEM_BUBBLE;
    } else if (local_state->
        FindPreference(prefs::kShouldUseMinimalFirstRunBubble) &&
        local_state->GetBoolean(prefs::kShouldUseMinimalFirstRunBubble)) {
      bubble_type = FirstRun::MINIMAL_BUBBLE;
    }
    // Reset the preference so we don't show the bubble for subsequent
    // windows.
    local_state->SetBoolean(prefs::kShouldShowFirstRunBubble, false);
    window_->GetLocationBar()->ShowFirstRunBubble(bubble_type);
  } else {
    GlobalErrorService* service =
        GlobalErrorServiceFactory::GetForProfile(profile());
    GlobalError* error = service->GetFirstGlobalErrorWithBubbleView();
    if (error) {
      error->ShowBubbleView(this);
    }
  }
}
