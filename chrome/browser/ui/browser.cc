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
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_started_animation.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/api/app/app_api.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_unload_handler.h"
#include "chrome/browser/intents/register_intent_handler_infobar_delegate.h"
#include "chrome/browser/intents/web_intents_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
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
#include "chrome/browser/tab_closeable_state_watcher.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/app_modal_dialogs/message_box_handler.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tab_restore_service_delegate.h"
#include "chrome/browser/ui/browser_toolbar_model_delegate.h"
#include "chrome/browser/ui/browser_window.h"
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
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/sync/browser_synced_window_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "chrome/browser/ui/tabs/tab_finder.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/ui/webui/feedback_ui.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/ntp/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/options2/content_settings_handler2.h"
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
#include "chrome/common/net/url_util.h"
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
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/plugins/webplugininfo.h"

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
#include "ui/base/cocoa/find_pasteboard.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#endif

#if defined(USE_ASH)
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "chrome/browser/ui/views/ash/panel_view_aura.h"
#endif

using base::TimeDelta;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::PluginService;
using content::Referrer;
using content::SiteInstance;
using content::SSLStatus;
using content::UserMetricsAction;
using content::WebContents;

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

const char kHashMark[] = "#";

// Returns |true| if entry has an internal chrome:// URL, |false| otherwise.
bool HasInternalURL(const NavigationEntry* entry) {
  if (!entry)
    return false;

  // Check the |virtual_url()| first. This catches regular chrome:// URLs
  // including URLs that were rewritten (such as chrome://bookmarks).
  if (entry->GetVirtualURL().SchemeIs(chrome::kChromeUIScheme))
    return true;

  // If the |virtual_url()| isn't a chrome:// URL, check if it's actually
  // view-source: of a chrome:// URL.
  if (entry->GetVirtualURL().SchemeIs(chrome::kViewSourceScheme))
    return entry->GetURL().SchemeIs(chrome::kChromeUIScheme);

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
    if (!url.is_valid())
      url = GURL(chrome::kChromeUIExtensionsURL);
  }

  return url;
}

// Parse two comma-separated integers from str. Return true on success.
bool ParseCommaSeparatedIntegers(const std::string& str,
                                 int* ret_num1,
                                 int* ret_num2) {
  size_t num1_size = str.find_first_of(',');
  if (num1_size == std::string::npos)
    return false;

  size_t num2_pos = num1_size + 1;
  size_t num2_size = str.size() - num2_pos;
  int num1, num2;
  if (!base::StringToInt(str.substr(0, num1_size), &num1) ||
      !base::StringToInt(str.substr(num2_pos, num2_size), &num2))
    return false;

  *ret_num1 = num1;
  *ret_num2 = num2;
  return true;
}

bool AllowPanels(const std::string& app_name) {
  return PanelManager::ShouldUsePanels(
      web_app::GetExtensionIdFromApplicationName(app_name));
}

bool DisplayOldDownloadsUI() {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDownloadsNewUI);
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
      command_updater_(this),
      app_type_(APP_TYPE_HOST),
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
      window_has_shown_(false) {
  tab_strip_model_->AddObserver(this);

  toolbar_model_.reset(new ToolbarModel(toolbar_model_delegate_.get()));

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

  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    local_pref_registrar_.Init(local_state);
    local_pref_registrar_.Add(prefs::kPrintingEnabled, this);
    local_pref_registrar_.Add(prefs::kAllowFileSelectionDialogs, this);
    local_pref_registrar_.Add(prefs::kMetricsReportingEnabled, this);
    local_pref_registrar_.Add(prefs::kInManagedMode, this);
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

  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  if (service)
    service->AddObserver(this);

  CreateInstantIfNecessary();

  // Make sure TabFinder has been created. This does nothing if TabFinder is
  // not enabled.
  TabFinder::GetInstance();

  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_INIT);

  FilePath profile_path = profile->GetPath();
  ProfileMetrics::LogProfileLaunch(profile_path);
}

Browser::~Browser() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  if (service)
    service->RemoveObserver(this);

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

  profile_pref_registrar_.RemoveAll();
  local_pref_registrar_.RemoveAll();

  encoding_auto_detect_.Destroy();

  if (profile_->IsOffTheRecord() &&
      !BrowserList::IsOffTheRecordSessionActiveForProfile(profile_)) {
    // An incognito profile is no longer needed, this indirectly frees
    // its cache and cookies once it gets destroyed at the appropriate time.
    ProfileDestroyer::DestroyOffTheRecordProfile(profile_);
  }

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  TabRestoreServiceDestroyed(tab_restore_service_);
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
    RegisterAppPrefs(params.app_name, params.profile);

  Browser* browser = new Browser(params.type, params.profile);
  browser->app_name_ = params.app_name;
  browser->app_type_ = params.app_type;
  browser->set_override_bounds(params.initial_bounds);
  browser->set_show_state(params.initial_show_state);
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
      ShellIntegration::GetAppId(UTF8ToWide(app_name_), profile_->GetPath()) :
      ShellIntegration::GetChromiumAppId(profile_->GetPath()),
      window()->GetNativeHandle());

  if (is_type_panel()) {
    ui::win::SetAppIconForWindow(ShellIntegration::GetChromiumIconPath(),
                                 window()->GetNativeHandle());
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

  // Permanently dismiss ntp4 bubble for new users.
  if (first_run::IsChromeFirstRun())
    NewTabPageHandler::DismissIntroMessage(local_state);
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
void Browser::NewEmptyWindow(Profile* profile) {
  bool incognito = profile->IsOffTheRecord();
  PrefService* prefs = profile->GetPrefs();
  if (incognito) {
    if (IncognitoModePrefs::GetAvailability(prefs) ==
          IncognitoModePrefs::DISABLED) {
      incognito = false;
    }
  } else {
    if (browser_defaults::kAlwaysOpenIncognitoWindow &&
        IncognitoModePrefs::ShouldLaunchIncognito(
            *CommandLine::ForCurrentProcess(), prefs)) {
      incognito = true;
    }
  }

  if (incognito) {
    content::RecordAction(UserMetricsAction("NewIncognitoWindow"));
    OpenEmptyWindow(profile->GetOffTheRecordProfile());
  } else {
    content::RecordAction(UserMetricsAction("NewWindow"));
    SessionService* session_service =
        SessionServiceFactory::GetForProfile(profile->GetOriginalProfile());
    if (!session_service ||
        !session_service->RestoreIfNecessary(std::vector<GURL>())) {
      OpenEmptyWindow(profile->GetOriginalProfile());
    }
  }
}

// static
Browser* Browser::OpenEmptyWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->AddBlankTab(true);
  browser->window()->Show();
  return browser;
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
WebContents* Browser::OpenApplication(
    Profile* profile,
    const Extension* extension,
    extension_misc::LaunchContainer container,
    const GURL& override_url,
    WindowOpenDisposition disposition) {
  WebContents* tab = NULL;
  ExtensionPrefs* prefs = profile->GetExtensionService()->extension_prefs();
  prefs->SetActiveBit(extension->id(), true);

  UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunchContainer", container, 100);

  if (extension->is_platform_app()) {
    extensions::AppEventRouter::DispatchOnLaunchedEvent(profile, extension);
    return NULL;
  }

  switch (container) {
    case extension_misc::LAUNCH_NONE: {
      NOTREACHED();
      break;
    }
    case extension_misc::LAUNCH_PANEL:
#if defined(USE_ASH)
      if (extension &&
          CommandLine::ForCurrentProcess()->HasSwitch(
              ash::switches::kAuraPanelManager)) {
        tab = OpenApplicationPanel(profile, extension, override_url);
        break;
      }
      // else fall through to LAUNCH_WINDOW
#endif
    case extension_misc::LAUNCH_WINDOW:
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

#if defined(USE_ASH)
// static
WebContents* Browser::OpenApplicationPanel(
    Profile* profile,
    const Extension* extension,
    const GURL& url_input) {
  GURL url = UrlForExtension(extension, url_input);
  std::string app_name =
      web_app::GenerateApplicationNameFromExtensionId(extension->id());
  gfx::Rect panel_bounds;
  panel_bounds.set_width(extension->launch_width());
  panel_bounds.set_height(extension->launch_height());
  PanelViewAura* panel_view = new PanelViewAura(app_name);
  panel_view->Init(profile, url, panel_bounds);
  return panel_view->WebContents();
}
#endif

// static
WebContents* Browser::OpenApplicationWindow(
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
  if (extension &&
      container == extension_misc::LAUNCH_PANEL &&
      AllowPanels(app_name)) {
    type = TYPE_PANEL;
  }

  gfx::Rect window_bounds;
  if (extension) {
    window_bounds.set_width(extension->launch_width());
    window_bounds.set_height(extension->launch_height());
  }

  CreateParams params(type, profile);
  params.app_name = app_name;
  params.initial_bounds = window_bounds;

#if defined(USE_ASH)
  if (extension &&
      container == extension_misc::LAUNCH_WINDOW) {
    // In ash, LAUNCH_FULLSCREEN launches in a maximized app window and
    // LAUNCH_WINDOW launches in a normal app window.
    ExtensionPrefs::LaunchType launch_type =
        profile->GetExtensionService()->extension_prefs()->GetLaunchType(
            extension->id(), ExtensionPrefs::LAUNCH_DEFAULT);
    if (launch_type == ExtensionPrefs::LAUNCH_FULLSCREEN)
      params.initial_show_state = ui::SHOW_STATE_MAXIMIZED;
    else if (launch_type == ExtensionPrefs::LAUNCH_WINDOW)
      params.initial_show_state = ui::SHOW_STATE_NORMAL;
  }
#endif

  Browser* browser = Browser::CreateWithParams(params);

  if (app_browser)
    *app_browser = browser;

  TabContentsWrapper* wrapper =
      browser->AddSelectedTabWithURL(url, content::PAGE_TRANSITION_START_PAGE);
  WebContents* contents = wrapper->web_contents();
  contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  contents->GetRenderViewHost()->SyncRendererPrefs();
  // TODO(stevenjb): Find the right centralized place to do this. Currently it
  // is only done for app tabs in normal browsers through SetExtensionAppById.
  if (extension && type == TYPE_PANEL)
    wrapper->extension_tab_helper()->SetExtensionAppIconById(extension->id());

  browser->window()->Show();

  // TODO(jcampan): http://crbug.com/8123 we should not need to set the initial
  //                focus explicitly.
  contents->GetView()->SetInitialFocus();
  return contents;
}

WebContents* Browser::OpenAppShortcutWindow(Profile* profile,
                                            const GURL& url,
                                            bool update_shortcut) {
  Browser* app_browser;
  WebContents* tab = OpenApplicationWindow(
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
    // the web app info is available, ExtensionTabHelper notifies Browser via
    // OnDidGetApplicationInfo, which calls
    // web_app::UpdateShortcutForTabContents when it sees UPDATE_SHORTCUT as
    // pending web app action.
    app_browser->pending_web_app_action_ = UPDATE_SHORTCUT;
  }
  return tab;
}

// static
WebContents* Browser::OpenApplicationTab(Profile* profile,
                                         const Extension* extension,
                                         const GURL& override_url,
                                         WindowOpenDisposition disposition) {
  Browser* browser = BrowserList::FindTabbedBrowser(profile, false);
  WebContents* contents = NULL;
  if (!browser) {
    // No browser for this profile, need to open a new one.
    browser = Browser::Create(profile);
    browser->window()->Show();
    // There's no current tab in this browser window, so add a new one.
    disposition = NEW_FOREGROUND_TAB;
  } else {
    // For existing browser, ensure its window is activated.
    browser->window()->Activate();
  }

  // Check the prefs for overridden mode.
  ExtensionService* extension_service = profile->GetExtensionService();
  DCHECK(extension_service);

  ExtensionPrefs::LaunchType launch_type =
      extension_service->extension_prefs()->GetLaunchType(
          extension->id(), ExtensionPrefs::LAUNCH_DEFAULT);
  UMA_HISTOGRAM_ENUMERATION("Extensions.AppTabLaunchType", launch_type, 100);

  static bool default_apps_trial_exists =
      base::FieldTrialList::TrialExists(kDefaultAppsTrialName);
  if (default_apps_trial_exists) {
    UMA_HISTOGRAM_ENUMERATION(
        base::FieldTrial::MakeName("Extensions.AppTabLaunchType",
                                   kDefaultAppsTrialName),
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
    WebContents* existing_tab = browser->GetSelectedWebContents();
    TabStripModel* model = browser->tabstrip_model();
    int tab_index = model->GetWrapperIndex(existing_tab);

    existing_tab->OpenURL(OpenURLParams(
          extension_url,
          content::Referrer(existing_tab->GetURL(),
                            WebKit::WebReferrerPolicyDefault),
          disposition, content::PAGE_TRANSITION_LINK, false));
    // Reset existing_tab as OpenURL() may have clobbered it.
    existing_tab = browser->GetSelectedWebContents();
    if (params.tabstrip_add_types & TabStripModel::ADD_PINNED) {
      model->SetTabPinned(tab_index, true);
      // Pinning may have moved the tab.
      tab_index = model->GetWrapperIndex(existing_tab);
    }
    if (params.tabstrip_add_types & TabStripModel::ADD_ACTIVE)
      model->ActivateTabAt(tab_index, true);

    contents = existing_tab;
  } else {
    browser::Navigate(&params);
    contents = params.target_contents->web_contents();
  }

#if defined(USE_ASH)
  // In ash, LAUNCH_FULLSCREEN launches in a maximized app window and it should
  // not reach here.
  DCHECK(launch_type != ExtensionPrefs::LAUNCH_FULLSCREEN);
#else
  // TODO(skerner):  If we are already in full screen mode, and the user
  // set the app to open as a regular or pinned tab, what should happen?
  // Today we open the tab, but stay in full screen mode.  Should we leave
  // full screen mode in this case?
  if (launch_type == ExtensionPrefs::LAUNCH_FULLSCREEN &&
      !browser->window()->IsFullscreen()) {
    browser->ToggleFullscreenMode();
  }
#endif

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
void Browser::OpenAboutWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->OpenAboutChromeDialog();
  browser->window()->Show();
}

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
void Browser::OpenSyncSetupWindow(Profile* profile,
                                  SyncPromoUI::Source source) {
  Browser* browser = Browser::Create(profile);
  browser->ShowSyncSetup(source);
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
#if defined USE_AURA
      if (is_app())
        return true;
#endif
      return browser_defaults::kRestorePopups || is_devtools();
    case TYPE_PANEL:
      // Do not save the window placement of panels.
      return false;
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
  gfx::Rect restored_bounds = override_bounds_;
  WindowSizer::GetBrowserWindowBounds(app_name_, restored_bounds, this,
                                      &restored_bounds);

  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  bool record_mode = parsed_command_line.HasSwitch(switches::kRecordMode);
  bool playback_mode = parsed_command_line.HasSwitch(switches::kPlaybackMode);
  if (record_mode || playback_mode) {
    // In playback/record mode we always fix the size of the browser and
    // move it to (0,0).  The reason for this is two reasons:  First we want
    // resize/moves in the playback to still work, and Second we want
    // playbacks to work (as much as possible) on machines w/ different
    // screen sizes.
    restored_bounds = gfx::Rect(0, 0, 800, 600);
  }

  // The following options override playback/record.
  if (parsed_command_line.HasSwitch(switches::kWindowSize)) {
    std::string str =
        parsed_command_line.GetSwitchValueASCII(switches::kWindowSize);
    int width, height;
    if (ParseCommaSeparatedIntegers(str, &width, &height))
      restored_bounds.set_size(gfx::Size(width, height));
  }
  if (parsed_command_line.HasSwitch(switches::kWindowPosition)) {
    std::string str =
        parsed_command_line.GetSwitchValueASCII(switches::kWindowPosition);
    int x, y;
    if (ParseCommaSeparatedIntegers(str, &x, &y))
      restored_bounds.set_origin(gfx::Point(x, y));
  }

  return restored_bounds;
}

ui::WindowShowState Browser::GetSavedWindowShowState() const {
  // Only tabbed browsers use the command line or preference state, with the
  // exception of devtools.
  bool show_state = !is_type_tabbed() && !is_devtools();

#if defined(USE_AURA)
  // Apps save state on aura.
  show_state &= !is_app();
#endif

  if (show_state)
    return show_state_;

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kStartMaximized))
    return ui::SHOW_STATE_MAXIMIZED;

  if (show_state_ != ui::SHOW_STATE_DEFAULT)
    return show_state_;

  const DictionaryValue* window_pref =
      profile()->GetPrefs()->GetDictionary(GetWindowPlacementKey().c_str());
  bool maximized = false;
  window_pref->GetBoolean("maximized", &maximized);

  return maximized ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_DEFAULT;
}

SkBitmap Browser::GetCurrentPageIcon() const {
  TabContentsWrapper* contents = GetSelectedTabContentsWrapper();
  // |contents| can be NULL since GetCurrentPageIcon() is called by the window
  // during the window's creation (before tabs have been added).
  return contents ? contents->favicon_tab_helper()->GetFavicon() : SkBitmap();
}

string16 Browser::GetWindowTitleForCurrentTab() const {
  WebContents* contents = GetSelectedWebContents();
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
  WebContents* contents = GetSelectedWebContents();
  if (contents && contents->GetCrashedStatus() ==
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
  return tab_strip_model_->count();
}

int Browser::active_index() const {
  return tab_strip_model_->active_index();
}

int Browser::GetIndexOfController(
    const NavigationController* controller) const {
  return tab_strip_model_->GetIndexOfController(controller);
}

TabContentsWrapper* Browser::GetSelectedTabContentsWrapper() const {
  return tab_strip_model_->GetActiveTabContents();
}

WebContents* Browser::GetSelectedWebContents() const {
  TabContentsWrapper* wrapper = GetSelectedTabContentsWrapper();
  return wrapper ? wrapper->web_contents() : NULL;
}

TabContentsWrapper* Browser::GetTabContentsWrapperAt(int index) const {
  return tab_strip_model_->GetTabContentsAt(index);
}

WebContents* Browser::GetWebContentsAt(int index) const {
  TabContentsWrapper* wrapper = GetTabContentsWrapperAt(index);
  if (wrapper)
    return wrapper->web_contents();
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

TabContentsWrapper* Browser::AddSelectedTabWithURL(
    const GURL& url,
    content::PageTransition transition) {
  browser::NavigateParams params(this, url, transition);
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
  return params.target_contents;
}

WebContents* Browser::AddTab(TabContentsWrapper* tab_contents,
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
  TabContentsWrapper* wrapper = TabContentsFactory(
      profile(),
      tab_util::GetSiteInstanceForNewTab(profile_, restore_url),
      MSG_ROUTING_NONE,
      GetSelectedWebContents(),
      session_storage_namespace);
  WebContents* new_tab = wrapper->web_contents();
  wrapper->extension_tab_helper()->SetExtensionAppById(extension_app_id);
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
  tab_strip_model_->InsertTabContentsAt(tab_index, wrapper, add_types);
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
    new_tab->HideContents();
  }
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile_);
  if (session_service)
    session_service->TabRestored(wrapper, pin);
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

gfx::NativeWindow Browser::BrowserShowWebDialog(
    WebDialogDelegate* delegate,
    gfx::NativeWindow parent_window) {
  if (!parent_window)
    parent_window = window_->GetNativeHandle();

  return browser::ShowWebDialog(parent_window, profile_, this, delegate);
}

void Browser::BrowserRenderWidgetShowing() {
  RenderWidgetShowing();
}

void Browser::BookmarkBarSizeChanged(bool is_animating) {
  window_->ToolbarSizeChanged(is_animating);
}

void Browser::ReplaceRestoredTab(
    const std::vector<TabNavigation>& navigations,
    int selected_navigation,
    bool from_last_session,
    const std::string& extension_app_id,
    content::SessionStorageNamespace* session_storage_namespace) {
  GURL restore_url = navigations.at(selected_navigation).virtual_url();
  TabContentsWrapper* wrapper = TabContentsFactory(
      profile(),
      tab_util::GetSiteInstanceForNewTab(profile_, restore_url),
      MSG_ROUTING_NONE,
      GetSelectedWebContents(),
      session_storage_namespace);
  wrapper->extension_tab_helper()->SetExtensionAppById(extension_app_id);
  WebContents* replacement = wrapper->web_contents();
  std::vector<NavigationEntry*> entries;
  TabNavigation::CreateNavigationEntriesFromTabNavigations(
      profile_, navigations, &entries);
  replacement->GetController().Restore(
      selected_navigation, from_last_session, &entries);
  DCHECK_EQ(0u, entries.size());

  tab_strip_model_->ReplaceNavigationControllerAt(active_index(), wrapper);
}

bool Browser::NavigateToIndexWithDisposition(int index,
                                             WindowOpenDisposition disp) {
  NavigationController& controller =
      GetOrCloneTabForDisposition(disp)->GetController();
  if (index < 0 || index >= controller.GetEntryCount())
    return false;
  controller.GoToIndex(index);
  return true;
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
  WebContents* contents = GetSelectedWebContents();
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

browser::NavigateParams Browser::GetSingletonTabNavigateParams(
    const GURL& url) {
  browser::NavigateParams params(
      this, url, content::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = SINGLETON_TAB;
  params.window_action = browser::NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;
  return params;
}

void Browser::WindowFullscreenStateChanged() {
  fullscreen_controller_->WindowFullscreenStateChanged();
  UpdateCommandsForFullscreenMode(window_->IsFullscreen());
  UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TOGGLE_FULLSCREEN);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted browser commands:

bool Browser::CanGoBack() const {
  return GetSelectedWebContents()->GetController().CanGoBack();
}

void Browser::GoBack(WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("Back"));

  TabContentsWrapper* current_tab = GetSelectedTabContentsWrapper();
  if (CanGoBack()) {
    WebContents* new_tab = GetOrCloneTabForDisposition(disposition);
    // If we are on an interstitial page and clone the tab, it won't be copied
    // to the new tab, so we don't need to go back.
    if (current_tab->web_contents()->ShowingInterstitialPage() &&
        (new_tab != current_tab->web_contents()))
      return;
    new_tab->GetController().GoBack();
  }
}

bool Browser::CanGoForward() const {
  return GetSelectedWebContents()->GetController().CanGoForward();
}

void Browser::GoForward(WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("Forward"));
  if (CanGoForward())
    GetOrCloneTabForDisposition(disposition)->GetController().GoForward();
}

void Browser::Reload(WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("Reload"));
  ReloadInternal(disposition, false);
}

void Browser::ReloadIgnoringCache(WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("ReloadIgnoringCache"));
  ReloadInternal(disposition, true);
}

void Browser::Home(WindowOpenDisposition disposition) {
  content::RecordAction(UserMetricsAction("Home"));
  OpenURL(OpenURLParams(
      profile_->GetHomePage(), Referrer(), disposition,
      content::PageTransitionFromInt(
          content::PAGE_TRANSITION_AUTO_BOOKMARK |
          content::PAGE_TRANSITION_HOME_PAGE),
      false));
}

void Browser::OpenCurrentURL() {
  content::RecordAction(UserMetricsAction("LoadURL"));
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
    WebContents* existing_tab = TabFinder::GetInstance()->FindTab(
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
    AppLauncherHandler::RecordAppLaunchType(
        extension_misc::APP_LAUNCH_OMNIBOX_LOCATION);
  }
}

void Browser::Stop() {
  content::RecordAction(UserMetricsAction("Stop"));
  GetSelectedWebContents()->Stop();
}

void Browser::NewWindow() {
  NewEmptyWindow(profile_->GetOriginalProfile());
}

void Browser::NewIncognitoWindow() {
  NewEmptyWindow(profile_->GetOffTheRecordProfile());
}

void Browser::CloseWindow() {
  content::RecordAction(UserMetricsAction("CloseWindow"));
  window_->Close();
}

void Browser::NewTab() {
  content::RecordAction(UserMetricsAction("NewTab"));

  if (is_type_tabbed()) {
    AddBlankTab(true);
    GetSelectedWebContents()->GetView()->RestoreFocus();
  } else {
    Browser* b = GetOrCreateTabbedBrowser(profile_);
    b->AddBlankTab(true);
    b->window()->Show();
    // The call to AddBlankTab above did not set the focus to the tab as its
    // window was not active, so we have to do it explicitly.
    // See http://crbug.com/6380.
    b->GetSelectedWebContents()->GetView()->RestoreFocus();
  }
}

void Browser::CloseTab() {
  content::RecordAction(UserMetricsAction("CloseTab_Accelerator"));
  if (CanCloseTab())
    tab_strip_model_->CloseSelectedTabs();
}

void Browser::SelectNextTab() {
  content::RecordAction(UserMetricsAction("SelectNextTab"));
  tab_strip_model_->SelectNextTab();
}

void Browser::SelectPreviousTab() {
  content::RecordAction(UserMetricsAction("SelectPrevTab"));
  tab_strip_model_->SelectPreviousTab();
}

void Browser::OpenTabpose() {
#if defined(OS_MACOSX)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableExposeForTabs)) {
    return;
  }

  content::RecordAction(UserMetricsAction("OpenTabpose"));
  window()->OpenTabpose();
#else
  NOTREACHED();
#endif
}

void Browser::MoveTabNext() {
  content::RecordAction(UserMetricsAction("MoveTabNext"));
  tab_strip_model_->MoveTabNext();
}

void Browser::MoveTabPrevious() {
  content::RecordAction(UserMetricsAction("MoveTabPrevious"));
  tab_strip_model_->MoveTabPrevious();
}

void Browser::SelectNumberedTab(int index) {
  if (index < tab_count()) {
    content::RecordAction(UserMetricsAction("SelectNumberedTab"));
    ActivateTabAt(index, true);
  }
}

void Browser::SelectLastTab() {
  content::RecordAction(UserMetricsAction("SelectLastTab"));
  tab_strip_model_->SelectLastTab();
}

void Browser::DuplicateTab() {
  content::RecordAction(UserMetricsAction("Duplicate"));
  DuplicateContentsAt(active_index());
}

void Browser::WriteCurrentURLToClipboard() {
  // TODO(ericu): There isn't currently a metric for this.  Should there be?
  // We don't appear to track the action when it comes from the
  // RenderContextViewMenu.

  WebContents* contents = GetSelectedWebContents();
  if (!toolbar_model_->ShouldDisplayURL())
    return;

  chrome_common_net::WriteURLToClipboard(
      contents->GetURL(),
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
      g_browser_process->clipboard());
}

void Browser::ConvertPopupToTabbedBrowser() {
  content::RecordAction(UserMetricsAction("ShowAsTab"));
  TabContentsWrapper* contents = tab_strip_model_->DetachTabContentsAt(
      active_index());
  Browser* browser = Browser::Create(profile_);
  browser->tabstrip_model()->AppendTabContents(contents, true);
  browser->window()->Show();
}

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

void Browser::Exit() {
  content::RecordAction(UserMetricsAction("Exit"));
  BrowserList::AttemptUserExit();
}

#if defined(OS_CHROMEOS)
void Browser::ShowKeyboardOverlay() {
  window_->ShowKeyboardOverlay(window_->GetNativeHandle());
}
#endif

void Browser::BookmarkCurrentPage() {
  content::RecordAction(UserMetricsAction("Star"));

  BookmarkModel* model = profile()->GetBookmarkModel();
  if (!model || !model->IsLoaded())
    return;  // Ignore requests until bookmarks are loaded.

  GURL url;
  string16 title;
  TabContentsWrapper* tab = GetSelectedTabContentsWrapper();
  bookmark_utils::GetURLAndTitleToBookmark(tab->web_contents(), &url, &title);
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
    // weird situations where the bubble is deleted as soon as it is shown.
    window_->ShowBookmarkBubble(url, was_bookmarked);
  }
}

void Browser::SavePage() {
  content::RecordAction(UserMetricsAction("SavePage"));
  WebContents* current_tab = GetSelectedWebContents();
  if (current_tab && current_tab->GetContentsMimeType() == "application/pdf")
    content::RecordAction(UserMetricsAction("PDF.SavePage"));
  GetSelectedWebContents()->OnSavePage();
}

void Browser::ViewSelectedSource() {
  ViewSource(GetSelectedTabContentsWrapper());
}

void Browser::ShowFindBar() {
  GetFindBarController()->Show();
}

void Browser::ShowPageInfo(content::WebContents* web_contents,
                           const GURL& url,
                           const SSLStatus& ssl,
                           bool show_history) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents->GetBrowserContext());
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebsiteSettings)) {
    window()->ShowWebsiteSettings(profile, wrapper, url, ssl, show_history);
  } else {
    window()->ShowPageInfo(profile, url, ssl, show_history);
  }
}

void Browser::ShowChromeToMobileBubble() {
  // Only show the bubble if the window is active, otherwise we may get into
  // weird situations where the bubble is deleted as soon as it is shown.
  if (window_->IsActive())
    window_->ShowChromeToMobileBubble();
}

bool Browser::SupportsWindowFeature(WindowFeature feature) const {
  return SupportsWindowFeatureImpl(feature, true);
}

bool Browser::CanSupportWindowFeature(WindowFeature feature) const {
  return SupportsWindowFeatureImpl(feature, false);
}

void Browser::Print() {
  if (g_browser_process->local_state()->GetBoolean(
          prefs::kPrintPreviewDisabled)) {
    GetSelectedTabContentsWrapper()->print_view_manager()->PrintNow();
  } else {
    GetSelectedTabContentsWrapper()->print_view_manager()->PrintPreviewNow();
  }
}

void Browser::AdvancedPrint() {
  GetSelectedTabContentsWrapper()->print_view_manager()->AdvancedPrintNow();
}

void Browser::EmailPageLocation() {
  content::RecordAction(UserMetricsAction("EmailPageLocation"));
  WebContents* wc = GetSelectedWebContents();
  DCHECK(wc);

  std::string title = net::EscapeQueryParamValue(
      UTF16ToUTF8(wc->GetTitle()), false);
  std::string page_url = net::EscapeQueryParamValue(wc->GetURL().spec(), false);
  std::string mailto = std::string("mailto:?subject=Fwd:%20") +
      title + "&body=%0A%0A" + page_url;
  platform_util::OpenExternal(GURL(mailto));
}

void Browser::ToggleEncodingAutoDetect() {
  content::RecordAction(UserMetricsAction("AutoDetectChange"));
  encoding_auto_detect_.SetValue(!encoding_auto_detect_.GetValue());
  // If "auto detect" is turned on, then any current override encoding
  // is cleared. This also implicitly performs a reload.
  // OTOH, if "auto detect" is turned off, we don't change the currently
  // active encoding.
  if (encoding_auto_detect_.GetValue()) {
    WebContents* contents = GetSelectedWebContents();
    if (contents)
      contents->ResetOverrideEncoding();
  }
}

void Browser::OverrideEncoding(int encoding_id) {
  content::RecordAction(UserMetricsAction("OverrideEncoding"));
  const std::string selected_encoding =
      CharacterEncoding::GetCanonicalEncodingNameByCommandId(encoding_id);
  WebContents* contents = GetSelectedWebContents();
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
  content::RecordAction(UserMetricsAction("Cut"));
  window()->Cut();
}

void Browser::Copy() {
  content::RecordAction(UserMetricsAction("Copy"));
  window()->Copy();
}

void Browser::Paste() {
  content::RecordAction(UserMetricsAction("Paste"));
  window()->Paste();
}

void Browser::Find() {
  content::RecordAction(UserMetricsAction("Find"));
  FindInPage(false, false);
}

void Browser::FindNext() {
  content::RecordAction(UserMetricsAction("FindNext"));
  FindInPage(true, true);
}

void Browser::FindPrevious() {
  content::RecordAction(UserMetricsAction("FindPrevious"));
  FindInPage(true, false);
}

void Browser::Zoom(content::PageZoom zoom) {
  if (is_devtools())
    return;

  content::RenderViewHost* host = GetSelectedWebContents()->GetRenderViewHost();
  if (zoom == content::PAGE_ZOOM_RESET) {
    host->SetZoomLevel(0);
    content::RecordAction(UserMetricsAction("ZoomNormal"));
    return;
  }

  double current_zoom_level = GetSelectedWebContents()->GetZoomLevel();
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
        content::RecordAction(UserMetricsAction("ZoomMinus"));
        return;
      }
      content::RecordAction(UserMetricsAction("ZoomMinus_AtMinimum"));
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
        content::RecordAction(UserMetricsAction("ZoomPlus"));
        return;
      }
    }
    content::RecordAction(UserMetricsAction("ZoomPlus_AtMaximum"));
  }
}

void Browser::FocusToolbar() {
  content::RecordAction(UserMetricsAction("FocusToolbar"));
  window_->FocusToolbar();
}

void Browser::FocusLocationBar() {
  content::RecordAction(UserMetricsAction("FocusLocation"));
  window_->SetFocusToLocationBar(true);
}

void Browser::FocusSearch() {
  // TODO(beng): replace this with FocusLocationBar
  content::RecordAction(UserMetricsAction("FocusSearch"));
  window_->GetLocationBar()->FocusSearch();
}

void Browser::FocusAppMenu() {
  content::RecordAction(UserMetricsAction("FocusAppMenu"));
  window_->FocusAppMenu();
}

void Browser::FocusBookmarksToolbar() {
  content::RecordAction(UserMetricsAction("FocusBookmarksToolbar"));
  window_->FocusBookmarksToolbar();
}

void Browser::FocusNextPane() {
  content::RecordAction(UserMetricsAction("FocusNextPane"));
  window_->RotatePaneFocus(true);
}

void Browser::FocusPreviousPane() {
  content::RecordAction(UserMetricsAction("FocusPreviousPane"));
  window_->RotatePaneFocus(false);
}

void Browser::OpenFile() {
  content::RecordAction(UserMetricsAction("OpenFile"));
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  const FilePath directory = profile_->last_selected_directory();

  // TODO(beng): figure out how to juggle this.
  gfx::NativeWindow parent_window = window_->GetNativeHandle();
  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                                  string16(), directory,
                                  NULL, 0, FILE_PATH_LITERAL(""),
                                  GetSelectedWebContents(),
                                  parent_window, NULL);
}

void Browser::OpenCreateShortcutsDialog() {
  content::RecordAction(UserMetricsAction("CreateShortcut"));
#if !defined(OS_MACOSX)
  TabContentsWrapper* current_tab = GetSelectedTabContentsWrapper();
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

void Browser::ToggleDevToolsWindow(DevToolsToggleAction action) {
  if (action == DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE)
    content::RecordAction(UserMetricsAction("DevTools_ToggleConsole"));
  else
    content::RecordAction(UserMetricsAction("DevTools_ToggleWindow"));

  DevToolsWindow::ToggleDevToolsWindow(
      GetSelectedWebContents()->GetRenderViewHost(),
      action);
}

void Browser::OpenTaskManager(bool highlight_background_resources) {
  content::RecordAction(UserMetricsAction("TaskManager"));
  if (highlight_background_resources)
    window_->ShowBackgroundPages();
  else
    window_->ShowTaskManager();
}

void Browser::OpenFeedbackDialog() {
  content::RecordAction(UserMetricsAction("Feedback"));
  browser::ShowWebFeedbackView(this, std::string(), std::string());
}

void Browser::ToggleBookmarkBar() {
  content::RecordAction(UserMetricsAction("ShowBookmarksBar"));
  window_->ToggleBookmarkBar();
}

void Browser::OpenBookmarkManager() {
  content::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  content::RecordAction(UserMetricsAction("ShowBookmarks"));
  ShowSingletonTabOverwritingNTP(
      GetSingletonTabNavigateParams(GURL(chrome::kChromeUIBookmarksURL)));
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
  content::RecordAction(UserMetricsAction("ShowHistory"));
  browser::NavigateParams params(
      GetSingletonTabNavigateParams(GURL(chrome::kChromeUIHistoryURL)));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(params);
}

void Browser::ShowDownloadsTab() {
  content::RecordAction(UserMetricsAction("ShowDownloads"));
  if (window()) {
    DownloadShelf* shelf = window()->GetDownloadShelf();
    if (shelf->IsShowing())
      shelf->Close();
  }
  ShowSingletonTabOverwritingNTP(
      GetSingletonTabNavigateParams(GURL(chrome::kChromeUIDownloadsURL)));
}

void Browser::ShowExtensionsTab() {
  content::RecordAction(UserMetricsAction("ShowExtensions"));
  browser::NavigateParams params(
      GetSingletonTabNavigateParams(GURL(chrome::kChromeUIExtensionsURL)));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(params);
}

void Browser::ShowAboutConflictsTab() {
  content::RecordAction(UserMetricsAction("AboutConflicts"));
  ShowSingletonTab(GURL(chrome::kChromeUIConflictsURL));
}

void Browser::ShowBrokenPageTab(WebContents* contents) {
  content::RecordAction(UserMetricsAction("Feedback"));
  string16 page_title = contents->GetTitle();
  NavigationEntry* entry = contents->GetController().GetActiveEntry();
  if (!entry)
    return;
  std::string page_url = entry->GetURL().spec();
  std::vector<std::string> subst;
  subst.push_back(UTF16ToASCII(page_title));
  subst.push_back(page_url);
  std::string report_page_url =
      ReplaceStringPlaceholders(kBrokenPageUrl, subst, NULL);
  ShowSingletonTab(GURL(report_page_url));
}

void Browser::ShowOptionsTab(const std::string& sub_page) {
  std::string url = std::string(chrome::kChromeUISettingsURL) + sub_page;
#if defined(OS_CHROMEOS)
  if (sub_page.find(chrome::kInternetOptionsSubPage, 0) != std::string::npos) {
    std::string::size_type loc = sub_page.find("?", 0);
    std::string network_page = loc != std::string::npos ?
        sub_page.substr(loc) : std::string();
    url = std::string(chrome::kChromeUISettingsURL) + network_page;
  }
#endif
  browser::NavigateParams params(GetSingletonTabNavigateParams(GURL(url)));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(params);
}

void Browser::ShowContentSettingsPage(ContentSettingsType content_type) {
  ShowOptionsTab(
      chrome::kContentSettingsExceptionsSubPage + std::string(kHashMark) +
      options2::ContentSettingsHandler::ContentSettingsTypeToGroupName(
          content_type));
}

void Browser::OpenClearBrowsingDataDialog() {
  content::RecordAction(UserMetricsAction("ClearBrowsingData_ShowDlg"));
  ShowOptionsTab(chrome::kClearBrowserDataSubPage);
}

void Browser::OpenOptionsDialog() {
  content::RecordAction(UserMetricsAction("ShowOptions"));
  ShowOptionsTab(std::string());
}

void Browser::OpenPasswordManager() {
  content::RecordAction(UserMetricsAction("Options_ShowPasswordManager"));
  ShowOptionsTab(chrome::kPasswordManagerSubPage);
}

void Browser::OpenImportSettingsDialog() {
  content::RecordAction(UserMetricsAction("Import_ShowDlg"));
  ShowOptionsTab(chrome::kImportDataSubPage);
}

void Browser::OpenInstantConfirmDialog() {
  ShowOptionsTab(chrome::kInstantConfirmPage);
}

void Browser::OpenAboutChromeDialog() {
  content::RecordAction(UserMetricsAction("AboutChrome"));
#if !defined(OS_WIN)
  browser::NavigateParams params(
      GetSingletonTabNavigateParams(GURL(chrome::kChromeUIUberURL)));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(params);
#else
  // crbug.com/115123.
  window_->ShowAboutChromeDialog();
#endif
}

void Browser::OpenUpdateChromeDialog() {
  content::RecordAction(UserMetricsAction("UpdateChrome"));
  window_->ShowUpdateChromeDialog();
}

void Browser::ShowHelpTab() {
  content::RecordAction(UserMetricsAction("ShowHelpTab"));
  ShowSingletonTab(GURL(chrome::kChromeHelpURL));
}

void Browser::OpenAutofillHelpTabAndActivate() {
  AddSelectedTabWithURL(GURL(chrome::kAutofillHelpURL),
                        content::PAGE_TRANSITION_LINK);
}

void Browser::OpenPrivacyDashboardTabAndActivate() {
  OpenURL(OpenURLParams(
      GURL(kPrivacyDashboardUrl), Referrer(),
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK, false));
  window_->Activate();
}

void Browser::OpenSearchEngineOptionsDialog() {
  content::RecordAction(UserMetricsAction("EditSearchEngines"));
  ShowOptionsTab(chrome::kSearchEnginesSubPage);
}

void Browser::OpenPluginsTabAndActivate() {
  OpenURL(OpenURLParams(
      GURL(chrome::kChromeUIPluginsURL), Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false));
  window_->Activate();
}

void Browser::ShowSyncSetup(SyncPromoUI::Source source) {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(
          profile()->GetOriginalProfile());
  LoginUIService* login_service =
      LoginUIServiceFactory::GetForProfile(profile()->GetOriginalProfile());
  if (service->HasSyncSetupCompleted()) {
    ShowOptionsTab(std::string());
  } else if (SyncPromoUI::ShouldShowSyncPromo(profile()) &&
             login_service->current_login_ui() == NULL) {
    // There is no currently active login UI, so display a new promo page.
    GURL url(SyncPromoUI::GetSyncPromoURL(GURL(), source));
    browser::NavigateParams params(GetSingletonTabNavigateParams(GURL(url)));
    params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
    ShowSingletonTabOverwritingNTP(params);
  } else {
    LoginUIServiceFactory::GetForProfile(
        profile()->GetOriginalProfile())->ShowLoginUI();
  }
}

void Browser::ToggleSpeechInput() {
  GetSelectedWebContents()->GetRenderViewHost()->ToggleSpeechInput();
}

void Browser::UpdateDownloadShelfVisibility(bool visible) {
  if (GetStatusBubble())
    GetStatusBubble()->UpdateDownloadShelfVisibility(visible);
}

///////////////////////////////////////////////////////////////////////////////

// static
void Browser::SetNewHomePagePrefs(PrefService* prefs) {
  const PrefService::Preference* home_page_pref =
      prefs->FindPreference(prefs::kHomePage);
  if (home_page_pref &&
      !home_page_pref->IsManaged() &&
      !prefs->HasPrefPath(prefs::kHomePage)) {
    prefs->SetString(prefs::kHomePage, std::string());
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
  prefs->RegisterBooleanPref(prefs::kAllowFileSelectionDialogs, true);
  prefs->RegisterBooleanPref(prefs::kShouldShowFirstRunBubble, false);
  prefs->RegisterBooleanPref(prefs::kPrintPreviewDisabled,
#if defined(GOOGLE_CHROME_BUILD)
                             false
#else
                             true
#endif
                             );
}

// static
void Browser::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kHomePage,
                            std::string(),
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
  // crack at the start of the main event loop. By that time,
  // StartupBrowserCreator has already created the browser window, and it's too
  // late: we need the pref to be already initialized. Doing it here also saves
  // us from having to hard-code pref registration in the several unit tests
  // that use this preference.
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
  prefs->RegisterIntegerPref(prefs::kDevToolsHSplitLocation,
                             -1,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(prefs::kDevToolsVSplitLocation,
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
  prefs->RegisterBooleanPref(prefs::kEnableMemoryInfo,
                             false,
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
Browser* Browser::GetBrowserForController(
    const NavigationController* controller, int* index_result) {
  BrowserList::const_iterator it;
  for (it = BrowserList::begin(); it != BrowserList::end(); ++it) {
    int index = (*it)->GetIndexOfController(controller);
    if (index != TabStripModel::kNoTab) {
      if (index_result)
        *index_result = index;
      return *it;
    }
  }

  return NULL;
}

// static
Browser* Browser::GetTabbedBrowser(Profile* profile,
                                   bool match_original_profiles) {
  return BrowserList::FindTabbedBrowser(profile, match_original_profiles);
}

// static
Browser* Browser::GetOrCreateTabbedBrowser(Profile* profile) {
  Browser* browser = GetTabbedBrowser(profile, false);
  if (!browser)
    browser = Browser::Create(profile);
  return browser;
}

// static
void Browser::RunFileChooserHelper(
    WebContents* tab, const content::FileChooserParams& params) {
  Profile* profile =
      Profile::FromBrowserContext(tab->GetBrowserContext());
  // FileSelectHelper adds a reference to itself and only releases it after
  // sending the result message. It won't be destroyed when this reference
  // goes out of scope.
  scoped_refptr<FileSelectHelper> file_select_helper(
      new FileSelectHelper(profile));
  file_select_helper->RunFileChooser(tab->GetRenderViewHost(), tab, params);
}

// static
void Browser::EnumerateDirectoryHelper(WebContents* tab, int request_id,
                                       const FilePath& path) {
  Profile* profile =
      Profile::FromBrowserContext(tab->GetBrowserContext());
  // FileSelectHelper adds a reference to itself and only releases it after
  // sending the result message. It won't be destroyed when this reference
  // goes out of scope.
  scoped_refptr<FileSelectHelper> file_select_helper(
      new FileSelectHelper(profile));
  file_select_helper->EnumerateDirectory(request_id,
                                         tab->GetRenderViewHost(),
                                         path);
}

// static
void Browser::JSOutOfMemoryHelper(WebContents* tab) {
  TabContentsWrapper* tcw = TabContentsWrapper::GetCurrentWrapperForContents(
      tab);
  if (!tcw)
    return;

  InfoBarTabHelper* infobar_helper = tcw->infobar_tab_helper();
  infobar_helper->AddInfoBar(new SimpleAlertInfoBarDelegate(
      infobar_helper,
      NULL,
      l10n_util::GetStringUTF16(IDS_JS_OUT_OF_MEMORY_PROMPT),
      true));
}

// static
void Browser::RegisterProtocolHandlerHelper(WebContents* tab,
                                            const std::string& protocol,
                                            const GURL& url,
                                            const string16& title) {
  TabContentsWrapper* tcw = TabContentsWrapper::GetCurrentWrapperForContents(
      tab);
  if (!tcw || tcw->profile()->IsOffTheRecord())
    return;

  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(protocol, url, title);

  ProtocolHandlerRegistry* registry =
      tcw->profile()->GetProtocolHandlerRegistry();

  if (!registry->SilentlyHandleRegisterHandlerRequest(handler)) {
    content::RecordAction(
        UserMetricsAction("RegisterProtocolHandler.InfoBar_Shown"));
    InfoBarTabHelper* infobar_helper = tcw->infobar_tab_helper();

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
void Browser::FindReplyHelper(WebContents* tab,
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

void Browser::ExecuteCommand(int id) {
  ExecuteCommandWithDisposition(id, CURRENT_TAB);
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

  // In Apps mode, no keys are reserved.
  if (is_app())
    return false;

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
  tab_strip_model_->TabNavigating(contents, transition);

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
  ScheduleUIUpdate(contents->web_contents(), content::INVALIDATE_TYPE_URL);

  if (contents_is_selected)
    contents->web_contents()->Focus();
}

void Browser::ShowCollectedCookiesDialog(TabContentsWrapper* wrapper) {
  browser::ShowCollectedCookiesDialog(window()->GetNativeHandle(), wrapper);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, PageNavigator implementation:

WebContents* Browser::OpenURL(const OpenURLParams& params) {
  return OpenURLFromTab(NULL, params);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, CommandUpdater::CommandUpdaterDelegate implementation:

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
    case IDC_FULLSCREEN:            ToggleFullscreenMode();      break;
#if defined(OS_MACOSX)
    case IDC_PRESENTATION_MODE:     TogglePresentationMode();    break;
#endif
    case IDC_EXIT:                  Exit();                           break;
#if defined(OS_CHROMEOS)
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
    case IDC_CHROME_TO_MOBILE_PAGE: ShowChromeToMobileBubble();       break;
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
    case IDC_FEEDBACK:              OpenFeedbackDialog();            break;

    case IDC_SHOW_BOOKMARK_BAR:     ToggleBookmarkBar();              break;
    case IDC_PROFILING_ENABLED:     Profiling::Toggle();              break;

    case IDC_SHOW_BOOKMARK_MANAGER: OpenBookmarkManager();            break;
    case IDC_SHOW_APP_MENU:         ShowAppMenu();                    break;
    case IDC_SHOW_AVATAR_MENU:      ShowAvatarMenu();                 break;
    case IDC_SHOW_HISTORY:          ShowHistoryTab();                 break;
    case IDC_SHOW_DOWNLOADS:        ShowDownloadsTab();               break;
    case IDC_MANAGE_EXTENSIONS:     ShowExtensionsTab();              break;
    case IDC_OPTIONS:               OpenOptionsDialog();              break;
    case IDC_EDIT_SEARCH_ENGINES:   OpenSearchEngineOptionsDialog();  break;
    case IDC_VIEW_PASSWORDS:        OpenPasswordManager();            break;
    case IDC_CLEAR_BROWSING_DATA:   OpenClearBrowsingDataDialog();    break;
    case IDC_IMPORT_SETTINGS:       OpenImportSettingsDialog();       break;
    case IDC_ABOUT:                 OpenAboutChromeDialog();          break;
    case IDC_UPGRADE_DIALOG:        OpenUpdateChromeDialog();         break;
    case IDC_VIEW_INCOMPATIBILITIES: ShowAboutConflictsTab();         break;
    case IDC_HELP_PAGE:             ShowHelpTab();                    break;
    case IDC_SHOW_SYNC_SETUP:       ShowSyncSetup(SyncPromoUI::SOURCE_MENU);
                                    break;
    case IDC_TOGGLE_SPEECH_INPUT:   ToggleSpeechInput();              break;

    default:
      LOG(WARNING) << "Received Unimplemented Command: " << id;
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Browser, TabRestoreServiceObserver:

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

// Centralized method for creating a TabContentsWrapper, configuring and
// installing all its supporting objects and observers.
TabContentsWrapper* Browser::TabContentsFactory(
    Profile* profile,
    SiteInstance* site_instance,
    int routing_id,
    const WebContents* base_web_contents,
    content::SessionStorageNamespace* session_storage_namespace) {
  WebContents* new_contents = WebContents::Create(
      profile, site_instance, routing_id, base_web_contents,
      session_storage_namespace);
  TabContentsWrapper* wrapper = new TabContentsWrapper(new_contents);
  return wrapper;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModelDelegate implementation:

TabContentsWrapper* Browser::AddBlankTab(bool foreground) {
  return AddBlankTabAt(-1, foreground);
}

TabContentsWrapper* Browser::AddBlankTabAt(int index, bool foreground) {
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
  browser->LoadingStateChanged(detached_contents->web_contents());
  return browser;
}

int Browser::GetDragActions() const {
  return TabStripModelDelegate::TAB_TEAROFF_ACTION | (tab_count() > 1 ?
      TabStripModelDelegate::TAB_MOVE_ACTION : 0);
}

TabContentsWrapper* Browser::CreateTabContentsForURL(
    const GURL& url, const content::Referrer& referrer, Profile* profile,
    content::PageTransition transition, bool defer_load,
    SiteInstance* instance) const {
  TabContentsWrapper* contents = TabContentsFactory(profile, instance,
      MSG_ROUTING_NONE,
      GetSelectedWebContents(), NULL);
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
  TabContentsWrapper* contents = GetTabContentsWrapperAt(index);
  CHECK(contents);
  TabContentsWrapper* contents_dupe = contents->Clone();

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

void Browser::CreateHistoricalTab(TabContentsWrapper* contents) {
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
    service->CreateHistoricalTab(&contents->web_contents()->GetController(),
        tab_strip_model_->GetIndexOfTabContents(contents));
  }
}

bool Browser::RunUnloadListenerBeforeClosing(TabContentsWrapper* contents) {
  return Browser::RunUnloadEventsHelper(contents->web_contents());
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
  if (tab_count() == static_cast<int>(indices->size()) &&
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

bool Browser::CanRestoreTab() {
  return command_updater_.IsCommandEnabled(IDC_RESTORE_TAB);
}

void Browser::RestoreTab() {
  content::RecordAction(UserMetricsAction("RestoreTab"));
  TabRestoreService* service =
    TabRestoreServiceFactory::GetForProfile(profile_);
  if (!service)
    return;

  service->RestoreMostRecentEntry(tab_restore_service_delegate());
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
                           TabContentsWrapper* contents,
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

void Browser::TabDetachedAt(TabContentsWrapper* contents, int index) {
  TabDetachedAtImpl(contents, index, DETACH_TYPE_DETACH);
}

void Browser::TabDeactivated(TabContentsWrapper* contents) {
  fullscreen_controller_->OnTabDeactivated(contents);
  if (instant())
    instant()->Hide();

  // Save what the user's currently typing, so it can be restored when we
  // switch back to this tab.
  window_->GetLocationBar()->SaveStateToContents(contents->web_contents());
}

void Browser::ActiveTabChanged(TabContentsWrapper* old_contents,
                               TabContentsWrapper* new_contents,
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
      Reload(CURRENT_TAB);
      did_reload = true;
    }
  }

  // Discarded tabs always get reloaded.
  if (!did_reload && IsTabDiscarded(index)) {
    LOG(WARNING) << "Reloading discarded tab at " << index;
    static int reload_count = 0;
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Tabs.Discard.ReloadCount", ++reload_count, 1, 1000, 50);
    Reload(CURRENT_TAB);
  }

  // If we have any update pending, do it now.
  if (chrome_updater_factory_.HasWeakPtrs() && old_contents)
    ProcessPendingUIUpdates();

  // Propagate the profile to the location bar.
  UpdateToolbar(true);

  // Update reload/stop state.
  UpdateReloadStopState(new_contents->web_contents()->IsLoading(), true);

  // Update commands to reflect current state.
  UpdateCommandsForTabState();

  // Reset the status bubble.
  StatusBubble* status_bubble = GetStatusBubble();
  if (status_bubble) {
    status_bubble->Hide();

    // Show the loading state (if any).
    status_bubble->SetStatus(
        GetSelectedTabContentsWrapper()->core_tab_helper()->GetStatusText());
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

void Browser::TabPinnedStateChanged(TabContentsWrapper* contents, int index) {
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(profile());
  if (session_service) {
    session_service->SetPinnedState(
        session_id(),
        GetTabContentsWrapperAt(index)->restore_tab_helper()->session_id(),
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
  // NOTE: If you change to be immediate (no invokeLater) then you'll need to
  //       update BrowserList::CloseAllBrowsers.
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&Browser::CloseFrame, weak_factory_.GetWeakPtr()));
  // Set is_attempting_to_close_browser_ here, so that extensions, etc, do not
  // attempt to add tabs to the browser before it closes.
  is_attempting_to_close_browser_ = true;
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
      WebContents* contents = GetTabContentsWrapperAt(i)->web_contents();
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
    return PanelManager::GetInstance()->CreatePanel(this);

  return BrowserWindow::CreateBrowserWindow(this);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, content::WebContentsDelegate implementation:

WebContents* Browser::OpenURLFromTab(WebContents* source,
                                     const OpenURLParams& params) {
  browser::NavigateParams nav_params(this, params.url, params.transition);
  nav_params.source_contents = GetTabContentsWrapperAt(
      tab_strip_model_->GetWrapperIndex(source));
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
    UpdateCommandsForTabState();
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

  TabContentsWrapper* source_wrapper = NULL;
  BlockedContentTabHelper* source_blocked_content = NULL;
  TabContentsWrapper* new_wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(new_contents);
  if (!new_wrapper) {
    new_wrapper = new TabContentsWrapper(new_contents);
  }
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

    new_contents->GetRenderViewHost()->DisassociateFromPopupCount();
  }

  browser::NavigateParams params(this, new_wrapper);
  params.source_contents = source ?
      GetTabContentsWrapperAt(tab_strip_model_->GetWrapperIndex(source)): NULL;
  params.disposition = disposition;
  params.window_bounds = initial_pos;
  params.window_action = browser::NavigateParams::SHOW_WINDOW;
  params.user_gesture = user_gesture;
  browser::Navigate(&params);
}

void Browser::ActivateContents(WebContents* contents) {
  ActivateTabAt(tab_strip_model_->GetWrapperIndex(contents), false);
  window_->Activate();
}

void Browser::DeactivateContents(WebContents* contents) {
  window_->Deactivate();
}

void Browser::LoadingStateChanged(WebContents* source) {
  window_->UpdateLoadingAnimations(tab_strip_model_->TabsAreLoading());
  window_->UpdateTitleBar();

  WebContents* selected_contents = GetSelectedWebContents();
  if (source == selected_contents) {
    bool is_loading = source->IsLoading();
    UpdateReloadStopState(is_loading, false);
    if (GetStatusBubble()) {
      GetStatusBubble()->SetStatus(
          GetSelectedTabContentsWrapper()->core_tab_helper()->GetStatusText());
    }

    if (!is_loading && pending_web_app_action_ == UPDATE_SHORTCUT) {
      // Schedule a shortcut update when web application info is available if
      // last committed entry is not NULL. Last committed entry could be NULL
      // when an interstitial page is injected (e.g. bad https certificate,
      // malware site etc). When this happens, we abort the shortcut update.
      NavigationEntry* entry = source->GetController().GetLastCommittedEntry();
      if (entry) {
        TabContentsWrapper::GetCurrentWrapperForContents(source)->
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

  int index = tab_strip_model_->GetWrapperIndex(source);
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
  int index = tab_strip_model_->GetWrapperIndex(source);
  if (index >= 0)
    tab_strip_model_->DetachTabContentsAt(index);
}

bool Browser::IsPopupOrPanel(const WebContents* source) const {
  // A non-tabbed BROWSER is an unconstrained popup.
  return is_type_popup() || is_type_panel();
}

bool Browser::CanReloadContents(WebContents* source) const {
  return !is_devtools();
}

void Browser::UpdateTargetURL(WebContents* source, int32 page_id,
                              const GURL& url) {
  if (!GetStatusBubble())
    return;

  if (source == GetSelectedWebContents()) {
    PrefService* prefs = profile_->GetPrefs();
    GetStatusBubble()->SetURL(url, prefs->GetString(prefs::kAcceptLanguages));
  }
}

void Browser::ContentsMouseEvent(
    WebContents* source, const gfx::Point& location, bool motion) {
  if (!GetStatusBubble())
    return;

  if (source == GetSelectedWebContents()) {
    GetStatusBubble()->MouseMoved(location, !motion);
    if (!motion)
      GetStatusBubble()->SetURL(GURL(), std::string());
  }
}

void Browser::ContentsZoomChange(bool zoom_in) {
  ExecuteCommand(zoom_in ? IDC_ZOOM_PLUS : IDC_ZOOM_MINUS);
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
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(contents);
  if (!wrapper)
    wrapper = new TabContentsWrapper(contents);
  app_browser->tabstrip_model()->AppendTabContents(wrapper, true);

  contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  contents->GetRenderViewHost()->SyncRendererPrefs();
  app_browser->window()->Show();
}

gfx::Rect Browser::GetRootWindowResizerRect() const {
  return window_->GetRootWindowResizerRect();
}

void Browser::BeforeUnloadFired(WebContents* tab,
                                bool proceed,
                                bool* proceed_to_fire_unload) {
  if (!is_attempting_to_close_browser_) {
    *proceed_to_fire_unload = proceed;
    if (!proceed)
      tab->SetClosedByUserGesture(false);
    return;
  }

  if (!proceed) {
    CancelWindowClose();
    *proceed_to_fire_unload = false;
    tab->SetClosedByUserGesture(false);
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

void Browser::OnStartDownload(WebContents* source,
                              content::DownloadItem* download) {
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(source);
  TabContentsWrapper* constrained = GetConstrainingContentsWrapper(wrapper);
  if (constrained != wrapper) {
    // Download in a constrained popup is shown in the tab that opened it.
    WebContents* constrained_tab = constrained->web_contents();
    constrained_tab->GetDelegate()->OnStartDownload(constrained_tab, download);
    return;
  }

  if (!window())
    return;

  if (DisplayOldDownloadsUI()) {
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
    WebContents* shelf_tab = shelf->browser()->GetSelectedWebContents();
    if ((download->GetTotalBytes() > 0) &&
        !ChromeDownloadManagerDelegate::IsExtensionDownload(download) &&
        platform_util::IsVisible(shelf_tab->GetNativeView()) &&
        ui::Animation::ShouldRenderRichAnimation()) {
      DownloadStartedAnimation::Show(shelf_tab);
    }
  }

  // If the download occurs in a new tab, close it.
  if (source->GetController().IsInitialNavigation() && tab_count() > 1)
    CloseContents(source);
}

void Browser::ViewSourceForTab(WebContents* source, const GURL& page_url) {
  DCHECK(source);
  TabContentsWrapper* wrapper = GetTabContentsWrapperAt(
      tab_strip_model_->GetWrapperIndex(source));
  ViewSource(wrapper);
}

void Browser::ViewSourceForFrame(WebContents* source,
                                 const GURL& frame_url,
                                 const std::string& frame_content_state) {
  DCHECK(source);
  TabContentsWrapper* wrapper = GetTabContentsWrapperAt(
      tab_strip_model_->GetWrapperIndex(source));
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

void Browser::ShowRepostFormWarningDialog(WebContents* source) {
  browser::ShowTabModalConfirmDialog(
      new RepostFormWarningController(source),
      TabContentsWrapper::GetCurrentWrapperForContents(source));
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
  // Create a TabContentsWrapper now, so all observers are in place, as the
  // network requests for its initial navigation will start immediately. The
  // WebContents will later be inserted into this browser using
  // Browser::Navigate via AddNewContents. The latter will retrieve the newly
  // created TabContentsWrapper from WebContents object.
  new TabContentsWrapper(new_contents);

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
  UpdateCommandsForContentRestrictionState();
}

void Browser::RendererUnresponsive(WebContents* source) {
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

void Browser::RendererResponsive(WebContents* source) {
  browser::HideHungRendererDialog(source);
}

void Browser::WorkerCrashed(WebContents* source) {
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(source);
  InfoBarTabHelper* infobar_helper = wrapper->infobar_tab_helper();
  infobar_helper->AddInfoBar(new SimpleAlertInfoBarDelegate(
      infobar_helper,
      NULL,
      l10n_util::GetStringUTF16(IDS_WEBWORKER_CRASHED_PROMPT),
      true));
}

void Browser::DidNavigateMainFramePostCommit(WebContents* tab) {
  if (tab == GetSelectedWebContents())
    UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
}

void Browser::DidNavigateToPendingEntry(WebContents* tab) {
  if (tab == GetSelectedWebContents())
    UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
}

content::JavaScriptDialogCreator* Browser::GetJavaScriptDialogCreator() {
  return GetJavaScriptDialogCreatorInstance();
}

content::ColorChooser* Browser::OpenColorChooser(WebContents* tab,
                                                 int color_chooser_id,
                                                 SkColor color) {
#if defined(OS_WIN)
  // On Windows, only create a color chooser if one doesn't exist, because we
  // can't close the old color chooser dialog.
  if (!color_chooser_.get())
    color_chooser_.reset(content::ColorChooser::Create(color_chooser_id, tab,
                                                       color));
#else
  if (color_chooser_.get())
    color_chooser_->End();
  color_chooser_.reset(content::ColorChooser::Create(color_chooser_id, tab,
                                                     color));
#endif
  return color_chooser_.get();
}

void Browser::DidEndColorChooser() {
  color_chooser_.reset();
}

void Browser::RunFileChooser(WebContents* tab,
                             const content::FileChooserParams& params) {
  RunFileChooserHelper(tab, params);
}

void Browser::EnumerateDirectory(WebContents* tab, int request_id,
                                 const FilePath& path) {
  EnumerateDirectoryHelper(tab, request_id, path);
}

void Browser::ToggleFullscreenModeForTab(WebContents* tab,
                                         bool enter_fullscreen) {
  fullscreen_controller_->ToggleFullscreenModeForTab(tab, enter_fullscreen);
}

bool Browser::IsFullscreenForTabOrPending(const WebContents* tab) const {
  return fullscreen_controller_->IsFullscreenForTabOrPending(tab);
}

void Browser::JSOutOfMemory(WebContents* tab) {
  JSOutOfMemoryHelper(tab);
}

void Browser::RegisterProtocolHandler(WebContents* tab,
                                      const std::string& protocol,
                                      const GURL& url,
                                      const string16& title) {
  RegisterProtocolHandlerHelper(tab, protocol, url, title);
}

void Browser::RegisterIntentHandler(WebContents* tab,
                                    const string16& action,
                                    const string16& type,
                                    const string16& href,
                                    const string16& title,
                                    const string16& disposition) {
#if defined(ENABLE_WEB_INTENTS)
  RegisterIntentHandlerHelper(tab, action, type, href, title, disposition);
#endif
}

void Browser::WebIntentDispatch(
    WebContents* tab, content::WebIntentsDispatcher* intents_dispatcher) {
#if defined(ENABLE_WEB_INTENTS)
  if (!web_intents::IsWebIntentsEnabled(profile_))
    return;

  TabContentsWrapper* tcw =
      TabContentsWrapper::GetCurrentWrapperForContents(tab);
  tcw->web_intent_picker_controller()->SetIntentsDispatcher(intents_dispatcher);
  tcw->web_intent_picker_controller()->ShowDialog(
      intents_dispatcher->GetIntent().action,
      intents_dispatcher->GetIntent().type);
#endif  // defined(ENABLE_WEB_INTENTS)
}

void Browser::UpdatePreferredSize(WebContents* source,
                                  const gfx::Size& pref_size) {
  window_->UpdatePreferredSize(source, pref_size);
}

void Browser::ResizeDueToAutoResize(WebContents* source,
                                    const gfx::Size& new_size) {
  window_->ResizeDueToAutoResize(source, new_size);
}

void Browser::FindReply(WebContents* tab,
                        int request_id,
                        int number_of_matches,
                        const gfx::Rect& selection_rect,
                        int active_match_ordinal,
                        bool final_update) {
  FindReplyHelper(tab, request_id, number_of_matches, selection_rect,
                  active_match_ordinal, final_update);
}

void Browser::RequestToLockMouse(WebContents* tab, bool user_gesture) {
  fullscreen_controller_->RequestToLockMouse(tab, user_gesture);
}

void Browser::LostMouseLock() {
  fullscreen_controller_->LostMouseLock();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, CoreTabHelperDelegate implementation:

void Browser::SwapTabContents(TabContentsWrapper* old_tab_contents,
                              TabContentsWrapper* new_tab_contents) {
  int index = tab_strip_model_->GetIndexOfTabContents(old_tab_contents);
  DCHECK_NE(TabStripModel::kNoTab, index);
  tab_strip_model_->ReplaceTabContentsAt(index, new_tab_contents);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, SearchEngineTabHelperDelegate implementation:

void Browser::ConfirmAddSearchProvider(TemplateURL* template_url,
                                       Profile* profile) {
  window()->ConfirmAddSearchProvider(template_url, profile);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ConstrainedWindowTabHelperDelegate implementation:

void Browser::SetTabContentBlocked(TabContentsWrapper* wrapper, bool blocked) {
  int index = tab_strip_model_->GetIndexOfTabContents(wrapper);
  if (index == TabStripModel::kNoTab) {
    NOTREACHED();
    return;
  }
  tab_strip_model_->SetTabBlocked(index, blocked);
  UpdatePrintingState(wrapper->web_contents()->GetContentRestrictions());
  if (!blocked && GetSelectedTabContentsWrapper() == wrapper)
    wrapper->web_contents()->Focus();
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
// Browser, ExtensionTabHelperDelegate implementation:

void Browser::OnDidGetApplicationInfo(TabContentsWrapper* source,
                                      int32 page_id) {
  if (GetSelectedTabContentsWrapper() != source)
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
      if (GetSelectedWebContents() &&
          &GetSelectedWebContents()->GetController() ==
          content::Source<NavigationController>(source).ptr())
        UpdateToolbar(false);
      break;

    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      if (window()->GetLocationBar())
        window()->GetLocationBar()->UpdatePageActions();

      // Close any tabs from the unloaded extension, unless it's terminated,
      // in which case let the sad tabs remain.
      if (content::Details<UnloadedExtensionInfo>(details)->reason !=
          extension_misc::UNLOAD_REASON_TERMINATE) {
        const Extension* extension =
            content::Details<UnloadedExtensionInfo>(details)->extension;
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
      } else if (pref_name == prefs::kInManagedMode) {
        UpdateCommandsForMultipleProfiles();
      } else {
        NOTREACHED();
      }
      break;
    }

    case chrome::NOTIFICATION_WEB_CONTENT_SETTINGS_CHANGED: {
      WebContents* web_contents = content::Source<WebContents>(source).ptr();
      if (web_contents == GetSelectedWebContents()) {
        LocationBar* location_bar = window()->GetLocationBar();
        if (location_bar)
          location_bar->UpdateContentSettingsIcons();
      }
      break;
    }

    case content::NOTIFICATION_INTERSTITIAL_ATTACHED:
      UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
      UpdateCommandsForTabState();
      break;

    case content::NOTIFICATION_INTERSTITIAL_DETACHED:
      UpdateBookmarkBarState(BOOKMARK_BAR_STATE_CHANGE_TAB_STATE);
      UpdateCommandsForTabState();
      break;

    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ProfileSyncServiceObserver implementation:

void Browser::OnStateChanged() {
  DCHECK(ProfileSyncServiceFactory::GetInstance()->HasProfileSyncService(
      profile_));
  // For unit tests, we don't have a window.
  if (!window_)
    return;
  const bool show_main_ui = IsShowingMainUI(window_->IsFullscreen());
  command_updater_.UpdateCommandEnabled(IDC_SHOW_SYNC_SETUP,
      show_main_ui && profile_->GetOriginalProfile()->IsSyncAccessible());
}

///////////////////////////////////////////////////////////////////////////////
// Browser, InstantDelegate implementation:

void Browser::ShowInstant(TabContentsWrapper* preview_contents) {
  DCHECK(instant_->tab_contents() == GetSelectedTabContentsWrapper());
  window_->ShowInstant(preview_contents);

  GetSelectedWebContents()->HideContents();
  preview_contents->web_contents()->ShowContents();
}

void Browser::HideInstant() {
  window_->HideInstant();
  if (GetSelectedWebContents())
    GetSelectedWebContents()->ShowContents();
  if (instant_->GetPreviewContents())
    instant_->GetPreviewContents()->web_contents()->HideContents();
}

void Browser::CommitInstant(TabContentsWrapper* preview_contents) {
  TabContentsWrapper* tab_contents = instant_->tab_contents();
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



///////////////////////////////////////////////////////////////////////////////
// Browser, Command and state updating (private):

bool Browser::IsShowingMainUI(bool is_fullscreen) {
#if !defined(OS_MACOSX)
  return is_type_tabbed() && !is_fullscreen;
#else
  return is_type_tabbed();
#endif
}

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
  command_updater_.UpdateCommandEnabled(IDC_SHOW_KEYBOARD_OVERLAY, true);
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

void Browser::UpdateCommandsForTabState() {
  WebContents* current_tab = GetSelectedWebContents();
  TabContentsWrapper* current_tab_wrapper = GetSelectedTabContentsWrapper();
  if (!current_tab || !current_tab_wrapper)  // May be NULL during tab restore.
    return;

  // Navigation commands
  NavigationController& nc = current_tab->GetController();
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
      current_tab->GetController().CanViewSource());
  command_updater_.UpdateCommandEnabled(IDC_EMAIL_PAGE_LOCATION,
      toolbar_model_->ShouldDisplayURL() && current_tab->GetURL().is_valid());
  if (is_devtools())
      command_updater_.UpdateCommandEnabled(IDC_OPEN_FILE, false);

  // Changing the encoding is not possible on Chrome-internal webpages.
  bool is_chrome_internal = HasInternalURL(nc.GetActiveEntry()) ||
      current_tab->ShowingInterstitialPage();
  command_updater_.UpdateCommandEnabled(IDC_ENCODING_MENU,
      !is_chrome_internal && current_tab->IsSavable());

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

  // Show various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_DEVELOPER_MENU, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FEEDBACK, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_SYNC_SETUP,
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
#if defined (ENABLE_PROFILING) && !defined(NO_TCMALLOC)
  command_updater_.UpdateCommandEnabled(IDC_PROFILING_ENABLED, show_main_ui);
#endif

  UpdateCommandsForBookmarkBar();
  UpdateCommandsForMultipleProfiles();
}

void Browser::UpdateCommandsForMultipleProfiles() {
  bool show_main_ui = IsShowingMainUI(window_ && window_->IsFullscreen());
  command_updater_.UpdateCommandEnabled(IDC_SHOW_AVATAR_MENU,
      show_main_ui &&
      !profile()->IsOffTheRecord() &&
      ProfileManager::IsMultipleProfilesEnabled());
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

void Browser::UpdateReloadStopState(bool is_loading, bool force) {
  window_->UpdateReloadStopState(is_loading, force);
  command_updater_.UpdateCommandEnabled(IDC_STOP, is_loading);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, UI update coalescing and handling (private):

void Browser::UpdateToolbar(bool should_restore_state) {
  window_->UpdateToolbar(GetSelectedTabContentsWrapper(), should_restore_state);
}

void Browser::ScheduleUIUpdate(const WebContents* source,
                               unsigned changed_flags) {
  if (!source)
    return;

  // Do some synchronous updates.
  if (changed_flags & content::INVALIDATE_TYPE_URL &&
      source == GetSelectedWebContents()) {
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

    if (contents == GetSelectedWebContents()) {
      // Updates that only matter when the tab is selected go here.

      if (flags & content::INVALIDATE_TYPE_PAGE_ACTIONS) {
        LocationBar* location_bar = window()->GetLocationBar();
        if (location_bar)
          location_bar->UpdatePageActions();
      }
      // Updating the URL happens synchronously in ScheduleUIUpdate.
      if (flags & content::INVALIDATE_TYPE_LOAD && GetStatusBubble()) {
        GetStatusBubble()->SetStatus(
            GetSelectedTabContentsWrapper()->
                core_tab_helper()->GetStatusText());
      }

      if (flags & (content::INVALIDATE_TYPE_TAB |
                   content::INVALIDATE_TYPE_TITLE)) {
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
    if (flags &
        (content::INVALIDATE_TYPE_TAB | content::INVALIDATE_TYPE_TITLE)) {
      tab_strip_model_->UpdateTabContentsStateAt(
          tab_strip_model_->GetWrapperIndex(contents),
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
    WebContents* tab = *(tabs_needing_before_unload_fired_.begin());
    // Null check render_view_host here as this gets called on a PostTask and
    // the tab's render_view_host may have been nulled out.
    if (tab->GetRenderViewHost()) {
      tab->GetRenderViewHost()->FirePageBeforeUnload(false);
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
    WebContents* tab = *(tabs_needing_unload_fired_.begin());
    // Null check render_view_host here as this gets called on a PostTask and
    // the tab's render_view_host may have been nulled out.
    if (tab->GetRenderViewHost()) {
      tab->GetRenderViewHost()->ClosePage();
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

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BROWSER_CLOSE_CANCELLED,
      content::Source<Browser>(this),
      content::NotificationService::NoDetails());

  // Inform TabCloseableStateWatcher that closing of window has been canceled.
  TabCloseableStateWatcher* watcher =
      g_browser_process->tab_closeable_state_watcher();
  if (watcher)
    watcher->OnWindowCloseCanceled(this);
}

bool Browser::RemoveFromSet(UnloadListenerSet* set, WebContents* tab) {
  DCHECK(is_attempting_to_close_browser_);

  UnloadListenerSet::iterator iter = std::find(set->begin(), set->end(), tab);
  if (iter != set->end()) {
    set->erase(iter);
    return true;
  }
  return false;
}

void Browser::ClearUnloadState(WebContents* tab, bool process_now) {
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

void Browser::SetAsDelegate(TabContentsWrapper* tab, Browser* delegate) {
  // WebContents...
  tab->web_contents()->SetDelegate(delegate);

  // ...and all the helpers.
  tab->blocked_content_tab_helper()->set_delegate(delegate);
  tab->bookmark_tab_helper()->set_delegate(delegate);
  tab->constrained_window_tab_helper()->set_delegate(delegate);
  tab->core_tab_helper()->set_delegate(delegate);
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

  registrar_.Remove(this, content::NOTIFICATION_INTERSTITIAL_ATTACHED,
                    content::Source<WebContents>(contents->web_contents()));
  registrar_.Remove(this, content::NOTIFICATION_INTERSTITIAL_DETACHED,
                    content::Source<WebContents>(contents->web_contents()));
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                    content::Source<WebContents>(contents->web_contents()));
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

void Browser::ReloadInternal(WindowOpenDisposition disposition,
                             bool ignore_cache) {
  // If we are showing an interstitial, treat this as an OpenURL.
  WebContents* current_tab = GetSelectedWebContents();
  if (current_tab && current_tab->ShowingInterstitialPage()) {
    NavigationEntry* entry = current_tab->GetController().GetActiveEntry();
    DCHECK(entry);  // Should exist if interstitial is showing.
    OpenURL(OpenURLParams(
        entry->GetURL(), Referrer(), disposition,
        content::PAGE_TRANSITION_RELOAD, false));
    return;
  }

  // As this is caused by a user action, give the focus to the page.
  //
  // Also notify RenderViewHostDelegate of the user gesture; this is
  // normally done in Browser::Navigate, but a reload bypasses Navigate.
  WebContents* tab = GetOrCloneTabForDisposition(disposition);
  tab->GetRenderViewHost()->GetDelegate()->OnUserGesture();
  if (!tab->FocusLocationBarByDefault())
    tab->Focus();
  if (ignore_cache)
    tab->GetController().ReloadIgnoringCache(true);
  else
    tab->GetController().Reload(true);
}

WebContents* Browser::GetOrCloneTabForDisposition(
       WindowOpenDisposition disposition) {
  TabContentsWrapper* current_tab = GetSelectedTabContentsWrapper();
  switch (disposition) {
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB: {
      current_tab = current_tab->Clone();
      tab_strip_model_->AddTabContents(
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
  return current_tab->web_contents();
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

bool Browser::IsClosingPermitted() {
  TabCloseableStateWatcher* watcher =
      g_browser_process->tab_closeable_state_watcher();
  bool can_close = !watcher || watcher->CanCloseBrowser(this);
  if (!can_close && is_attempting_to_close_browser_)
    CancelWindowClose();
  return can_close;
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

  NavigationEntry* active_entry =
      contents->web_contents()->GetController().GetActiveEntry();
  if (!active_entry)
    return;

  ViewSource(contents, active_entry->GetURL(), active_entry->GetContentState());
}

void Browser::ViewSource(TabContentsWrapper* contents,
                         const GURL& url,
                         const std::string& content_state) {
  content::RecordAction(UserMetricsAction("ViewSource"));
  DCHECK(contents);

  TabContentsWrapper* view_source_contents = contents->Clone();
  view_source_contents->web_contents()->GetController().PruneAllButActive();
  NavigationEntry* active_entry =
      view_source_contents->web_contents()->GetController().GetActiveEntry();
  if (!active_entry)
    return;

  GURL view_source_url = GURL(chrome::kViewSourceScheme + std::string(":") +
      url.spec());
  active_entry->SetVirtualURL(view_source_url);

  // Do not restore scroller position.
  active_entry->SetContentState(
      webkit_glue::RemoveScrollOffsetFromHistoryState(content_state));

  // Do not restore title, derive it from the url.
  active_entry->SetTitle(string16());

  // Now show view-source entry.
  if (CanSupportWindowFeature(FEATURE_TABSTRIP)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    int index = tab_strip_model_->GetIndexOfTabContents(contents);
    int add_types = TabStripModel::ADD_ACTIVE |
        TabStripModel::ADD_INHERIT_GROUP;
    tab_strip_model_->InsertTabContentsAt(index + 1, view_source_contents,
                                          add_types);
  } else {
    Browser* browser = Browser::CreateWithParams(
        Browser::CreateParams(TYPE_TABBED, profile_));

    // Preserve the size of the original window. The new window has already
    // been given an offset by the OS, so we shouldn't copy the old bounds.
    BrowserWindow* new_window = browser->window();
    new_window->SetBounds(gfx::Rect(new_window->GetRestoredBounds().origin(),
                          window()->GetRestoredBounds().size()));

    // We need to show the browser now. Otherwise ContainerWin assumes the
    // WebContents is invisible and won't size it.
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
  WebContents* current_tab = GetSelectedWebContents();
  if (current_tab) {
    content_restrictions = current_tab->GetContentRestrictions();
    NavigationEntry* active_entry =
        current_tab->GetController().GetActiveEntry();
    // See comment in UpdateCommandsForTabState about why we call url().
    if (!download_util::IsSavableURL(
            active_entry ? active_entry->GetURL() : GURL())
        || current_tab->ShowingInterstitialPage())
      content_restrictions |= content::CONTENT_RESTRICTION_SAVE;
    if (current_tab->ShowingInterstitialPage())
      content_restrictions |= content::CONTENT_RESTRICTION_PRINT;
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

void Browser::OpenBookmarkManagerWithHash(const std::string& action,
                                          int64 node_id) {
  content::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  content::RecordAction(UserMetricsAction("ShowBookmarks"));
  browser::NavigateParams params(GetSingletonTabNavigateParams(
      GURL(chrome::kChromeUIBookmarksURL).Resolve(
      StringPrintf("/#%s%s", action.c_str(),
      base::Int64ToString(node_id).c_str()))));
  params.path_behavior = browser::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(params);
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
