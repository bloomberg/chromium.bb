// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"

#if defined(OS_WIN)
#include <shellapi.h>
#include <windows.h>
#endif  // OS_WIN

#include <algorithm>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/browser_url_handler.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_toggle_action.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/dom_ui/bug_report_ui.h"
#include "chrome/browser/dom_ui/filebrowse_ui.h"
#include "chrome/browser/dom_ui/options/content_settings_handler.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/download_started_animation.h"
#include "chrome/browser/extensions/crashed_extension_infobar.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_disabled_infobar_delegate.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/host_zoom_map.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/instant/instant_unload_handler.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/net/browser_url_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_types.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/tab_closeable_state_watcher.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_manager.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/options/options_window.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/dock_info.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/window_sizer.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiling.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/web_apps.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/cookie_monster.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"
#include "net/base/static_cookie_policy.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/animation/animation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/point.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"

#if defined(OS_WIN)
#include "app/win/shell.h"
#include "chrome/browser/autofill/autofill_ie_toolbar_import_win.h"
#include "chrome/browser/browser_child_process_host.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#endif  // OS_WIN

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/find_pasteboard.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/boot_times_loader.h"
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
      "http://www.google.com/support/chromeos/";
  #endif
#else
    "http://www.google.com/support/chrome/";
#endif

// The URL to be opened when the Help link on the Autofill dialog is clicked.
const char kAutofillHelpUrl[] =
#if defined(OS_CHROMEOS)
    "http://www.google.com/support/chromeos/bin/answer.py?answer=142893";
#else
    "http://www.google.com/support/chrome/bin/answer.py?answer=142893";
#endif

// The URL to be loaded to display the "Report a broken page" form.
const char kBrokenPageUrl[] =
    "http://www.google.com/support/chrome/bin/request.py?contact_type="
    "broken_website&format=inproduct&p.page_title=$1&p.page_url=$2";

// The URL for the privacy dashboard.
const char kPrivacyDashboardUrl[] = "https://www.google.com/dashboard";

// How long we wait before updating the browser chrome while loading a page.
const int kUIUpdateCoalescingTimeMS = 200;

const char kHashMark[] = "#";

#if defined(OS_CHROMEOS)
// If a popup window is bigger than this fraction of the screen on chrome os,
// turn it into a tab
const float kPopupMaxWidthFactor = 0.5;
const float kPopupMaxHeightFactor = 0.6;
#endif

}  // namespace

extern bool g_log_bug53991;

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
      maximized_state_(MAXIMIZED_STATE_DEFAULT),
      method_factory_(this),
      block_command_execution_(false),
      last_blocked_command_id_(-1),
      last_blocked_command_disposition_(CURRENT_TAB),
      pending_web_app_action_(NONE) {
  registrar_.Add(this, NotificationType::SSL_VISIBLE_STATE_CHANGED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UPDATE_DISABLED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_PROCESS_TERMINATED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::PROFILE_ERROR,
                 NotificationService::AllSources());

  // Need to know when to alert the user of theme install delay.
  registrar_.Add(this, NotificationType::EXTENSION_READY_FOR_INSTALL,
                 NotificationService::AllSources());

  // NOTE: These prefs all need to be explicitly destroyed in the destructor
  // or you'll get a nasty surprise when you run the incognito tests.
  PrefService* local_state = g_browser_process->local_state();
  if (local_state)
    printing_enabled_.Init(prefs::kPrintingEnabled, local_state, this);
  dev_tools_disabled_.Init(prefs::kDevToolsDisabled,
                           profile_->GetPrefs(), this);
  incognito_mode_allowed_.Init(prefs::kIncognitoEnabled,
                               profile_->GetPrefs(), this);

  InitCommandState();
  BrowserList::AddBrowser(this);

  encoding_auto_detect_.Init(prefs::kWebKitUsesUniversalDetector,
                             profile_->GetPrefs(), NULL);
  use_vertical_tabs_.Init(prefs::kUseVerticalTabs, profile_->GetPrefs(), this);
  instant_enabled_.Init(prefs::kInstantEnabled, profile_->GetPrefs(), this);
  if (!TabMenuModel::AreVerticalTabsEnabled()) {
    // If vertical tabs aren't enabled, explicitly turn them off. Otherwise we
    // might show vertical tabs but not show an option to turn them off.
    use_vertical_tabs_.SetValue(false);
  }
  UpdateTabStripModelInsertionPolicy();

  tab_restore_service_ = profile->GetTabRestoreService();
  if (tab_restore_service_) {
    tab_restore_service_->AddObserver(this);
    TabRestoreServiceChanged(tab_restore_service_);
  }

  if (profile_->GetProfileSyncService())
    profile_->GetProfileSyncService()->AddObserver(this);

  CreateInstantIfNecessary();
}

Browser::~Browser() {
  VLOG_IF(1, g_log_bug53991) << "~Browser: " << profile_->IsOffTheRecord()
                             << "; stillActive="
                             << BrowserList::IsOffTheRecordSessionActive();

  if (profile_->GetProfileSyncService())
    profile_->GetProfileSyncService()->RemoveObserver(this);

  BrowserList::RemoveBrowser(this);

#if defined(OS_WIN) || defined(OS_LINUX)
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
    profile_->ResetTabRestoreService();
  }
#endif

  SessionService* session_service = profile_->GetSessionService();
  if (session_service)
    session_service->WindowClosed(session_id_);

  TabRestoreService* tab_restore_service = profile()->GetTabRestoreService();
  if (tab_restore_service)
    tab_restore_service->BrowserClosed(this);

  encoding_auto_detect_.Destroy();
  printing_enabled_.Destroy();
  dev_tools_disabled_.Destroy();
  incognito_mode_allowed_.Destroy();
  instant_enabled_.Destroy();
  use_vertical_tabs_.Destroy();

  if (profile_->IsOffTheRecord() &&
      !BrowserList::IsOffTheRecordSessionActive()) {
    // An off-the-record profile is no longer needed, this indirectly
    // frees its cache and cookies.
    profile_->GetOriginalProfile()->DestroyOffTheRecordProfile();
  }

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  TabRestoreServiceDestroyed(tab_restore_service_);
}

// static
Browser* Browser::Create(Profile* profile) {
  Browser* browser = new Browser(TYPE_NORMAL, profile);
  browser->CreateBrowserWindow();
  return browser;
}

// static
Browser* Browser::CreateForPopup(Type type,
                                 Profile* profile,
                                 TabContents* new_contents,
                                 const gfx::Rect& initial_bounds) {
  DCHECK(type & TYPE_POPUP);
  Browser* browser = new Browser(type, profile);
  browser->set_override_bounds(initial_bounds);
  browser->CreateBrowserWindow();
  TabContentsWrapper* wrapper = new TabContentsWrapper(new_contents);
  browser->tabstrip_model()->AppendTabContents(wrapper, true);
  return browser;
}

// static
Browser* Browser::CreateForType(Type type, Profile* profile) {
  Browser* browser = new Browser(type, profile);
  browser->CreateBrowserWindow();
  return browser;
}

// static
Browser* Browser::CreateForApp(const std::string& app_name,
                               const gfx::Size& window_size,
                               Profile* profile,
                               bool is_panel) {
  Browser::Type type = TYPE_APP;

  if (is_panel) {
    // TYPE_APP_PANEL is the logical choice.  However, the panel UI
    // is not fully implemented.  See crbug/55943.
    type = TYPE_APP_POPUP;
  }

  Browser* browser = new Browser(type, profile);
  browser->app_name_ = app_name;

  if (!window_size.IsEmpty()) {
    gfx::Rect initial_pos(window_size);
    browser->set_override_bounds(initial_pos);
  }

  browser->CreateBrowserWindow();

  return browser;
}

// static
Browser* Browser::CreateForDevTools(Profile* profile) {
  Browser* browser = new Browser(TYPE_DEVTOOLS, profile);
  browser->app_name_ = DevToolsWindow::kDevToolsApp;
  browser->CreateBrowserWindow();
  return browser;
}

void Browser::CreateBrowserWindow() {
  DCHECK(!window_);

  window_ = BrowserWindow::CreateBrowserWindow(this);

#if defined(OS_WIN)
  {
    // TODO: This might hit the disk
    //   http://code.google.com/p/chromium/issues/detail?id=61638
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    // Set the app user model id for this application to that of the application
    // name.  See http://crbug.com/7028.
    app::win::SetAppIdForWindow(
        type_ & TYPE_APP ?
        ShellIntegration::GetAppId(UTF8ToWide(app_name_), profile_->GetPath()) :
        ShellIntegration::GetChromiumAppId(profile_->GetPath()),
        window()->GetNativeHandle());
  }
#endif

  NotificationService::current()->Notify(
      NotificationType::BROWSER_WINDOW_READY,
      Source<Browser>(this),
      NotificationService::NoDetails());

  // Show the First Run information bubble if we've been told to.
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return;
  if (local_state->FindPreference(prefs::kShouldShowFirstRunBubble) &&
      local_state->GetBoolean(prefs::kShouldShowFirstRunBubble)) {
    FirstRun::BubbleType bubble_type = FirstRun::LARGE_BUBBLE;
    if (local_state->
        FindPreference(prefs::kShouldUseOEMFirstRunBubble) &&
        local_state->GetBoolean(prefs::kShouldUseOEMFirstRunBubble)) {
      bubble_type = FirstRun::OEM_BUBBLE;
    } else if (local_state->
        FindPreference(prefs::kShouldUseMinimalFirstRunBubble) &&
        local_state->GetBoolean(prefs::kShouldUseMinimalFirstRunBubble)) {
      bubble_type = FirstRun::MINIMAL_BUBBLE;
    }
    // Reset the preference so we don't show the bubble for subsequent windows.
    local_state->ClearPref(prefs::kShouldShowFirstRunBubble);
    window_->GetLocationBar()->ShowFirstRunBubble(bubble_type);
  }
  if (local_state->FindPreference(
      prefs::kAutoFillPersonalDataManagerFirstRun) &&
      local_state->GetBoolean(prefs::kAutoFillPersonalDataManagerFirstRun)) {
    // Notify PDM that this is a first run.
#if defined(OS_WIN)
    ImportAutofillDataWin(profile_->GetPersonalDataManager());
#endif  // defined(OS_WIN)
    // Reset the preference so we don't call it again for subsequent windows.
    local_state->ClearPref(prefs::kAutoFillPersonalDataManagerFirstRun);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Getters & Setters

const std::vector<std::wstring>& Browser::user_data_dir_profiles() const {
  return g_browser_process->user_data_dir_profiles();
}

void Browser::set_user_data_dir_profiles(
    const std::vector<std::wstring>& profiles) {
  g_browser_process->user_data_dir_profiles() = profiles;
}

FindBarController* Browser::GetFindBarController() {
  if (!find_bar_controller_.get()) {
    FindBar* find_bar = BrowserWindow::CreateFindBar(this);
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
  TabRestoreService* service = profile->GetTabRestoreService();
  if (service)
    service->RestoreMostRecentEntry(NULL);
}

// static
void Browser::OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  Browser* browser = GetOrCreateTabbedBrowser(
      profile->GetOffTheRecordProfile());
  browser->AddSelectedTabWithURL(url, PageTransition::LINK);
  browser->window()->Show();
}

// static
// TODO(erikkay): There are multiple reasons why this could fail.  Should
// this function return an error reason as well so that callers can show
// reasonable errors?
TabContents* Browser::OpenApplication(Profile* profile,
                                      const std::string& app_id,
                                      TabContents* existing_tab) {
  ExtensionService* extensions_service = profile->GetExtensionService();

  // If the extension with |app_id| could't be found, most likely because it
  // was uninstalled.
  const Extension* extension =
      extensions_service->GetExtensionById(app_id, false);
  if (!extension)
    return NULL;

  return OpenApplication(profile, extension, extension->launch_container(),
                         existing_tab);
}

// static
TabContents* Browser::OpenApplication(
    Profile* profile,
    const Extension* extension,
    extension_misc::LaunchContainer container,
    TabContents* existing_tab) {
  TabContents* tab = NULL;

  UMA_HISTOGRAM_ENUMERATION("Extensions.AppLaunchContainer", container, 100);

  switch (container) {
    case extension_misc::LAUNCH_WINDOW:
    case extension_misc::LAUNCH_PANEL:
      tab = Browser::OpenApplicationWindow(profile, extension, container,
                                           GURL(), NULL);
      break;
    case extension_misc::LAUNCH_TAB: {
      tab = Browser::OpenApplicationTab(profile, extension, existing_tab);
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
  GURL url;
  if (!url_input.is_empty()) {
    if (extension)
      DCHECK(extension->web_extent().ContainsURL(url_input));
    url = url_input;
  } else {
    DCHECK(extension);
    url = extension->GetFullLaunchURL();
  }

  std::string app_name;
  if (extension)
    app_name =
        web_app::GenerateApplicationNameFromExtensionId(extension->id());
  else
    app_name = web_app::GenerateApplicationNameFromURL(url);

  RegisterAppPrefs(app_name, profile);

  bool as_panel = extension && (container == extension_misc::LAUNCH_PANEL);

  gfx::Size window_size;
  if (extension)
    window_size.SetSize(extension->launch_width(),
                        extension->launch_height());

  Browser* browser = Browser::CreateForApp(app_name, window_size, profile,
                                           as_panel);
  if (app_browser)
    *app_browser = browser;

  TabContentsWrapper* wrapper =
      browser->AddSelectedTabWithURL(url, PageTransition::START_PAGE);
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
                                         TabContents* existing_tab) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  TabContents* contents = NULL;
  if (!browser || browser->type() != Browser::TYPE_NORMAL)
    return contents;

  // Check the prefs for overridden mode.
  ExtensionService* extensions_service = profile->GetExtensionService();
  DCHECK(extensions_service);

  ExtensionPrefs::LaunchType launch_type =
      extensions_service->extension_prefs()->GetLaunchType(
          extension->id(), ExtensionPrefs::LAUNCH_REGULAR);
  UMA_HISTOGRAM_ENUMERATION("Extensions.AppTabLaunchType", launch_type, 100);
  int add_type = TabStripModel::ADD_SELECTED;
  if (launch_type == ExtensionPrefs::LAUNCH_PINNED)
    add_type |= TabStripModel::ADD_PINNED;

  // For extensions lacking launch urls, determine a reasonable fallback.
  GURL extension_url = extension->GetFullLaunchURL();
  if (!extension_url.is_valid()) {
    extension_url = extension->options_url();
    if (!extension_url.is_valid())
      extension_url = GURL(chrome::kChromeUIExtensionsURL);
  }

  // TODO(erikkay): START_PAGE doesn't seem like the right transition in all
  // cases.
  browser::NavigateParams params(browser, extension_url,
                                 PageTransition::START_PAGE);
  params.tabstrip_add_types = add_type;

  // Launch the application in the existing TabContents, if it was supplied.
  if (existing_tab) {
    TabStripModel* model = browser->tabstrip_model();
    int tab_index = model->GetWrapperIndex(existing_tab);

    existing_tab->OpenURL(extension->GetFullLaunchURL(), existing_tab->GetURL(),
                          CURRENT_TAB, PageTransition::LINK);
    if (params.tabstrip_add_types & TabStripModel::ADD_PINNED) {
      model->SetTabPinned(tab_index, true);
      tab_index = model->GetWrapperIndex(existing_tab);
    }
    if (params.tabstrip_add_types & TabStripModel::ADD_SELECTED)
      model->SelectTabContentsAt(tab_index, true);

    contents = existing_tab;
  } else {
    params.disposition = NEW_FOREGROUND_TAB;
    browser::Navigate(&params);
    contents = params.target_contents->tab_contents();
  }

  if (launch_type == ExtensionPrefs::LAUNCH_FULLSCREEN)
    browser->window()->SetFullscreen(true);

  return contents;
}

// static
void Browser::OpenBookmarkManagerWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->ShowBookmarkManagerTab();
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
  browser->OpenHelpTab();
  browser->window()->Show();
}

// static
void Browser::OpenOptionsWindow(Profile* profile) {
  Browser* browser = Browser::Create(profile);
  browser->ShowOptionsTab(chrome::kDefaultOptionsSubPage);
  browser->window()->Show();
}

// static
void Browser::OpenClearBrowingDataDialogWindow(Profile* profile) {
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
  // Only save the window placement of popups if they are restored.
  return (type() & TYPE_POPUP) == 0 || browser_defaults::kRestorePopups;
}

void Browser::SaveWindowPlacement(const gfx::Rect& bounds, bool maximized) {
  // Save to the session storage service, used when reloading a past session.
  // Note that we don't want to be the ones who cause lazy initialization of
  // the session service. This function gets called during initial window
  // showing, and we don't want to bring in the session service this early.
  if (profile()->HasSessionService()) {
    SessionService* session_service = profile()->GetSessionService();
    if (session_service)
      session_service->SetWindowBounds(session_id_, bounds, maximized);
  }
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
  bool maximized;
  WindowSizer::GetBrowserWindowBounds(app_name_, restored_bounds, this,
                                      &restored_bounds, &maximized);
  return restored_bounds;
}

// TODO(beng): obtain maximized state some other way so we don't need to go
//             through all this hassle.
bool Browser::GetSavedMaximizedState() const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kStartMaximized))
    return true;

  if (maximized_state_ == MAXIMIZED_STATE_MAXIMIZED)
    return true;
  if (maximized_state_ == MAXIMIZED_STATE_UNMAXIMIZED)
    return false;

  // An explicit maximized state was not set. Query the window sizer.
  gfx::Rect restored_bounds;
  bool maximized = false;
  WindowSizer::GetBrowserWindowBounds(app_name_, restored_bounds, this,
                                      &restored_bounds, &maximized);
  return maximized;
}

SkBitmap Browser::GetCurrentPageIcon() const {
  TabContents* contents = GetSelectedTabContents();
  // |contents| can be NULL since GetCurrentPageIcon() is called by the window
  // during the window's creation (before tabs have been added).
  return contents ? contents->GetFavIcon() : SkBitmap();
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
    title = TabContents::GetDefaultTitle();

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  // On Mac or ChromeOS, we don't want to suffix the page title with
  // the application name.
  return title;
#elif defined(OS_WIN) || defined(OS_LINUX)
  int string_id = IDS_BROWSER_WINDOW_TITLE_FORMAT;
  // Don't append the app name to window titles on app frames and app popups
  if (type_ & TYPE_APP)
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

  // Don't use HasSessionService here, we want to force creation of the
  // session service so that user can restore what was open.
  SessionService* session_service = profile()->GetSessionService();
  if (session_service)
    session_service->WindowClosing(session_id());

  TabRestoreService* tab_restore_service = profile()->GetTabRestoreService();
  if (tab_restore_service)
    tab_restore_service->BrowserClosing(this);

  // TODO(sky): convert session/tab restore to use notification.
  NotificationService::current()->Notify(
      NotificationType::BROWSER_CLOSING,
      Source<Browser>(this),
      Details<bool>(&exiting));

  CloseAllTabs();
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

////////////////////////////////////////////////////////////////////////////////
// Browser, TabStripModel pass-thrus:

int Browser::tab_count() const {
  return tab_handler_->GetTabStripModel()->count();
}

int Browser::selected_index() const {
  return tab_handler_->GetTabStripModel()->selected_index();
}

int Browser::GetIndexOfController(
    const NavigationController* controller) const {
  return tab_handler_->GetTabStripModel()->GetIndexOfController(controller);
}

TabContentsWrapper* Browser::GetSelectedTabContentsWrapper() const {
  return tabstrip_model()->GetSelectedTabContents();
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

void Browser::SelectTabContentsAt(int index, bool user_gesture) {
  tab_handler_->GetTabStripModel()->SelectTabContentsAt(index, user_gesture);
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

TabContentsWrapper* Browser::AddSelectedTabWithURL(const GURL& url,
                                            PageTransition::Type transition) {
  browser::NavigateParams params(this, url, transition);
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
  return params.target_contents;
}

TabContents* Browser::AddTab(TabContentsWrapper* tab_contents,
                             PageTransition::Type type) {
  tab_handler_->GetTabStripModel()->AddTabContents(
      tab_contents, -1, type, TabStripModel::ADD_SELECTED);
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

void Browser::BrowserShowHtmlDialog(HtmlDialogUIDelegate* delegate,
                                    gfx::NativeWindow parent_window) {
  ShowHtmlDialog(delegate, parent_window);
}

void Browser::BrowserRenderWidgetShowing() {
  RenderWidgetShowing();
}

void Browser::ToolbarSizeChanged(bool is_animating) {
  ToolbarSizeChanged(NULL, is_animating);
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
  TabContentsWrapper* wrapper = TabContentsFactory(profile(), NULL,
      MSG_ROUTING_NONE,
      GetSelectedTabContents(),
      session_storage_namespace);
  TabContents* new_tab = wrapper->tab_contents();
  new_tab->SetExtensionAppById(extension_app_id);
  new_tab->controller().RestoreFromState(navigations, selected_navigation,
                                         from_last_session);

  bool really_pin =
      (pin && tab_index == tabstrip_model()->IndexOfFirstNonMiniTab());
  tab_handler_->GetTabStripModel()->InsertTabContentsAt(
      tab_index, wrapper,
      select ? TabStripModel::ADD_SELECTED : TabStripModel::ADD_NONE);
  if (really_pin)
    tab_handler_->GetTabStripModel()->SetTabPinned(tab_index, true);
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
  if (profile_->HasSessionService()) {
    SessionService* session_service = profile_->GetSessionService();
    if (session_service)
      session_service->TabRestored(&new_tab->controller(), really_pin);
  }
  return new_tab;
}

void Browser::ReplaceRestoredTab(
    const std::vector<TabNavigation>& navigations,
    int selected_navigation,
    bool from_last_session,
    const std::string& extension_app_id,
    SessionStorageNamespace* session_storage_namespace) {
  TabContentsWrapper* wrapper = TabContentsFactory(profile(), NULL,
      MSG_ROUTING_NONE,
      GetSelectedTabContents(),
      session_storage_namespace);
  TabContents* replacement = wrapper->tab_contents();
  replacement->SetExtensionAppById(extension_app_id);
  replacement->controller().RestoreFromState(navigations, selected_navigation,
                                             from_last_session);

  tab_handler_->GetTabStripModel()->ReplaceNavigationControllerAt(
      tab_handler_->GetTabStripModel()->selected_index(),
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

void Browser::ShowSingletonTab(const GURL& url, bool ignore_path) {
  browser::NavigateParams params(this, url, PageTransition::AUTO_BOOKMARK);
  params.disposition = SINGLETON_TAB;
  params.show_window = true;
  params.ignore_path = ignore_path;
  browser::Navigate(&params);
}

void Browser::UpdateCommandsForFullscreenMode(bool is_fullscreen) {
#if !defined(OS_MACOSX)
  const bool show_main_ui = (type() == TYPE_NORMAL) && !is_fullscreen;
#else
  const bool show_main_ui = (type() == TYPE_NORMAL);
#endif

  bool main_not_fullscreen_or_popup =
      show_main_ui && !is_fullscreen && (type() & TYPE_POPUP) == 0;

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_OPEN_CURRENT_URL, show_main_ui);

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_SHOW_AS_TAB,
      (type() & TYPE_POPUP) && !is_fullscreen);

  // Focus various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_TOOLBAR, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_LOCATION, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FOCUS_SEARCH, show_main_ui);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_MENU_BAR, main_not_fullscreen_or_popup);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_NEXT_PANE, main_not_fullscreen_or_popup);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_PREVIOUS_PANE, main_not_fullscreen_or_popup);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_BOOKMARKS, main_not_fullscreen_or_popup);
  command_updater_.UpdateCommandEnabled(
      IDC_FOCUS_CHROMEOS_STATUS, main_not_fullscreen_or_popup);

  // Show various bits of UI
  command_updater_.UpdateCommandEnabled(IDC_DEVELOPER_MENU, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_FEEDBACK, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_BOOKMARK_BAR,
      browser_defaults::bookmarks_enabled && show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SYNC_BOOKMARKS,
      show_main_ui && profile_->IsSyncAccessible());

  command_updater_.UpdateCommandEnabled(IDC_OPTIONS, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_EDIT_SEARCH_ENGINES, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_VIEW_PASSWORDS, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_ABOUT, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_APP_MENU, show_main_ui);
  command_updater_.UpdateCommandEnabled(IDC_TOGGLE_VERTICAL_TABS, show_main_ui);
#if defined (ENABLE_PROFILING) && !defined(NO_TCMALLOC)
  command_updater_.UpdateCommandEnabled(IDC_PROFILING_ENABLED, show_main_ui);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Browser, Assorted browser commands:

bool Browser::ShouldOpenNewTabForWindowDisposition(
    WindowOpenDisposition disposition) {
  return (disposition == NEW_FOREGROUND_TAB ||
          disposition == NEW_BACKGROUND_TAB);
}

TabContents* Browser::GetOrCloneTabForDisposition(
       WindowOpenDisposition disposition) {
  TabContentsWrapper* current_tab = GetSelectedTabContentsWrapper();
  if (ShouldOpenNewTabForWindowDisposition(disposition)) {
    current_tab = current_tab->Clone();
    tab_handler_->GetTabStripModel()->AddTabContents(
        current_tab, -1, PageTransition::LINK,
        disposition == NEW_FOREGROUND_TAB ? TabStripModel::ADD_SELECTED :
                                            TabStripModel::ADD_NONE);
  }
  return current_tab->tab_contents();
}

void Browser::UpdateTabStripModelInsertionPolicy() {
  tab_handler_->GetTabStripModel()->SetInsertionPolicy(UseVerticalTabs() ?
      TabStripModel::INSERT_BEFORE : TabStripModel::INSERT_AFTER);
}

void Browser::UseVerticalTabsChanged() {
  UpdateTabStripModelInsertionPolicy();
  window()->ToggleTabStripMode();
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

#if !defined(OS_CHROMEOS)
  // Chrome OS opens a FileBrowse pop up instead of using download shelf.
  // So FEATURE_DOWNLOADSHELF is only added for non-chromeos platforms.
  features |= FEATURE_DOWNLOADSHELF;
#endif  // !defined(OS_CHROMEOS)

  if (type() == TYPE_NORMAL) {
    features |= FEATURE_BOOKMARKBAR;
  }

  if (!hide_ui_for_fullscreen) {
    if (type() != TYPE_NORMAL)
      features |= FEATURE_TITLEBAR;

    if (type() == TYPE_NORMAL)
      features |= FEATURE_TABSTRIP;

    if (type() == TYPE_NORMAL)
      features |= FEATURE_TOOLBAR;

    if ((type() & Browser::TYPE_APP) == 0)
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

void Browser::GoBack(WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(UserMetricsAction("Back"), profile_);

  TabContentsWrapper* current_tab = GetSelectedTabContentsWrapper();
  if (current_tab->controller().CanGoBack()) {
    TabContents* new_tab = GetOrCloneTabForDisposition(disposition);
    // If we are on an interstitial page and clone the tab, it won't be copied
    // to the new tab, so we don't need to go back.
    if (current_tab->tab_contents()->showing_interstitial_page() &&
        (new_tab != current_tab->tab_contents()))
      return;
    new_tab->controller().GoBack();
  }
}

void Browser::GoForward(WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(UserMetricsAction("Forward"), profile_);
  if (GetSelectedTabContentsWrapper()->controller().CanGoForward())
    GetOrCloneTabForDisposition(disposition)->controller().GoForward();
}

void Browser::Reload(WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(UserMetricsAction("Reload"), profile_);
  ReloadInternal(disposition, false);
}

void Browser::ReloadIgnoringCache(WindowOpenDisposition disposition) {
  UserMetrics::RecordAction(UserMetricsAction("ReloadIgnoringCache"), profile_);
  ReloadInternal(disposition, true);
}

void Browser::ReloadInternal(WindowOpenDisposition disposition,
                             bool ignore_cache) {
  // If we are showing an interstitial, treat this as an OpenURL.
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab && current_tab->showing_interstitial_page()) {
    NavigationEntry* entry = current_tab->controller().GetActiveEntry();
    DCHECK(entry);  // Should exist if interstitial is showing.
    OpenURL(entry->url(), GURL(), disposition, PageTransition::RELOAD);
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
  UserMetrics::RecordAction(UserMetricsAction("Home"), profile_);
  OpenURL(GetHomePage(), GURL(), disposition, PageTransition::AUTO_BOOKMARK);
}

void Browser::OpenCurrentURL() {
  UserMetrics::RecordAction(UserMetricsAction("LoadURL"), profile_);
  LocationBar* location_bar = window_->GetLocationBar();
  WindowOpenDisposition open_disposition =
      location_bar->GetWindowOpenDisposition();
  if (OpenInstant(open_disposition))
    return;

  GURL url(WideToUTF8(location_bar->GetInputString()));
  browser::NavigateParams params(this, url, location_bar->GetPageTransition());
  params.disposition = open_disposition;
  // Use ADD_INHERIT_OPENER so that all pages opened by the omnibox at least
  // inherit the opener. In some cases the tabstrip will determine the group
  // should be inherited, in which case the group is inherited instead of the
  // opener.
  params.tabstrip_add_types =
      TabStripModel::ADD_FORCE_INDEX | TabStripModel::ADD_INHERIT_OPENER;
  browser::Navigate(&params);
}

void Browser::Stop() {
  UserMetrics::RecordAction(UserMetricsAction("Stop"), profile_);
  GetSelectedTabContentsWrapper()->tab_contents()->Stop();
}

void Browser::NewWindow() {
  if (browser_defaults::kAlwaysOpenIncognitoWindow &&
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kIncognito) &&
      incognito_mode_allowed_.GetValue()) {
    NewIncognitoWindow();
    return;
  }
  UserMetrics::RecordAction(UserMetricsAction("NewWindow"), profile_);
  SessionService* session_service =
      profile_->GetOriginalProfile()->GetSessionService();
  if (!session_service ||
      !session_service->RestoreIfNecessary(std::vector<GURL>())) {
    Browser::OpenEmptyWindow(profile_->GetOriginalProfile());
  }
}

void Browser::NewIncognitoWindow() {
  if (!incognito_mode_allowed_.GetValue()) {
    NewWindow();
    return;
  }

  UserMetrics::RecordAction(UserMetricsAction("NewIncognitoWindow"), profile_);
  Browser::OpenEmptyWindow(profile_->GetOffTheRecordProfile());
}

void Browser::CloseWindow() {
  UserMetrics::RecordAction(UserMetricsAction("CloseWindow"), profile_);
  window_->Close();
}

void Browser::NewTab() {
  UserMetrics::RecordAction(UserMetricsAction("NewTab"), profile_);

  if (type() == TYPE_NORMAL) {
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
  UserMetrics::RecordAction(UserMetricsAction("CloseTab_Accelerator"),
                            profile_);
  if (CanCloseTab()) {
    tab_handler_->GetTabStripModel()->CloseTabContentsAt(
        tab_handler_->GetTabStripModel()->selected_index(),
        TabStripModel::CLOSE_USER_GESTURE |
        TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);
  }
}

void Browser::SelectNextTab() {
  UserMetrics::RecordAction(UserMetricsAction("SelectNextTab"), profile_);
  tab_handler_->GetTabStripModel()->SelectNextTab();
}

void Browser::SelectPreviousTab() {
  UserMetrics::RecordAction(UserMetricsAction("SelectPrevTab"), profile_);
  tab_handler_->GetTabStripModel()->SelectPreviousTab();
}

void Browser::OpenTabpose() {
#if defined(OS_MACOSX)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableExposeForTabs)) {
    return;
  }

  UserMetrics::RecordAction(UserMetricsAction("OpenTabpose"), profile_);
  window()->OpenTabpose();
#else
  NOTREACHED();
#endif
}

void Browser::MoveTabNext() {
  UserMetrics::RecordAction(UserMetricsAction("MoveTabNext"), profile_);
  tab_handler_->GetTabStripModel()->MoveTabNext();
}

void Browser::MoveTabPrevious() {
  UserMetrics::RecordAction(UserMetricsAction("MoveTabPrevious"), profile_);
  tab_handler_->GetTabStripModel()->MoveTabPrevious();
}

void Browser::SelectNumberedTab(int index) {
  if (index < tab_count()) {
    UserMetrics::RecordAction(UserMetricsAction("SelectNumberedTab"),
                              profile_);
    tab_handler_->GetTabStripModel()->SelectTabContentsAt(index, true);
  }
}

void Browser::SelectLastTab() {
  UserMetrics::RecordAction(UserMetricsAction("SelectLastTab"), profile_);
  tab_handler_->GetTabStripModel()->SelectLastTab();
}

void Browser::DuplicateTab() {
  UserMetrics::RecordAction(UserMetricsAction("Duplicate"), profile_);
  DuplicateContentsAt(selected_index());
}

void Browser::RestoreTab() {
  UserMetrics::RecordAction(UserMetricsAction("RestoreTab"), profile_);
  TabRestoreService* service = profile_->GetTabRestoreService();
  if (!service)
    return;

  service->RestoreMostRecentEntry(this);
}

void Browser::WriteCurrentURLToClipboard() {
  // TODO(ericu): There isn't currently a metric for this.  Should there be?
  // We don't appear to track the action when it comes from the
  // RenderContextViewMenu.

  TabContents* contents = GetSelectedTabContents();
  if (!contents->ShouldDisplayURL())
    return;

  chrome_browser_net::WriteURLToClipboard(
      contents->GetURL(),
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages),
      g_browser_process->clipboard());
}

void Browser::ConvertPopupToTabbedBrowser() {
  UserMetrics::RecordAction(UserMetricsAction("ShowAsTab"), profile_);
  int tab_strip_index = tab_handler_->GetTabStripModel()->selected_index();
  TabContentsWrapper* contents =
      tab_handler_->GetTabStripModel()->DetachTabContentsAt(tab_strip_index);
  Browser* browser = Browser::Create(profile_);
  browser->tabstrip_model()->AppendTabContents(contents, true);
  browser->window()->Show();
}

void Browser::ToggleFullscreenMode() {
#if !defined(OS_MACOSX)
  // In kiosk mode, we always want to be fullscreen. When the browser first
  // starts we're not yet fullscreen, so let the initial toggle go through.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode) &&
      window_->IsFullscreen())
    return;
#endif

  UserMetrics::RecordAction(UserMetricsAction("ToggleFullscreen"), profile_);
  window_->SetFullscreen(!window_->IsFullscreen());
  // On Linux, setting fullscreen mode is an async call to the X server, which
  // may or may not support fullscreen mode.
#if !defined(OS_LINUX)
  UpdateCommandsForFullscreenMode(window_->IsFullscreen());
#endif
}

#if defined(OS_CHROMEOS)
void Browser::Search() {
  // If the NTP is showing, close it.
  if (StartsWithASCII(GetSelectedTabContents()->GetURL().spec(),
                       chrome::kChromeUINewTabURL, true)) {
    CloseTab();
    return;
  }

  // Exit fullscreen to show omnibox.
  if (window_->IsFullscreen()) {
    ToggleFullscreenMode();
    // ToggleFullscreenMode is asynchronous, so we don't have omnibox
    // visible at this point. Wait for next event cycle which toggles
    // the visibility of omnibox before creating new tab.
    MessageLoop::current()->PostTask(
        FROM_HERE, method_factory_.NewRunnableMethod(&Browser::Search));
    return;
  }

  // Otherwise just open it.
  NewTab();
}

void Browser::ShowKeyboardOverlay() {
  window_->ShowKeyboardOverlay(window_->GetNativeHandle());
}
#endif

void Browser::Exit() {
  UserMetrics::RecordAction(UserMetricsAction("Exit"), profile_);
#if defined(OS_CHROMEOS)
  chromeos::BootTimesLoader::Get()->AddLogoutTimeMarker("LogoutStarted", false);
  // Write /tmp/uptime-logout-started as well.
  const char kLogoutStarted[] = "logout-started";
  chromeos::BootTimesLoader::Get()->RecordCurrentStats(kLogoutStarted);
#endif
  BrowserList::Exit();
}

void Browser::BookmarkCurrentPage() {
  UserMetrics::RecordAction(UserMetricsAction("Star"), profile_);

  BookmarkModel* model = profile()->GetBookmarkModel();
  if (!model || !model->IsLoaded())
    return;  // Ignore requests until bookmarks are loaded.

  GURL url;
  string16 title;
  TabContents* tab = GetSelectedTabContents();
  bookmark_utils::GetURLAndTitleToBookmark(tab, &url, &title);
  bool was_bookmarked = model->IsBookmarked(url);
  if (!was_bookmarked && profile_->IsOffTheRecord()) {
    // If we're off the record the favicon may not have been saved. Save it now
    // so that bookmarks have an icon for the page.
    tab->SaveFavicon();
  }
  model->SetURLStarred(url, title, true);
  // Make sure the model actually added a bookmark before showing the star. A
  // bookmark isn't created if the url is invalid.
  if (window_->IsActive() && model->IsBookmarked(url)) {
    // Only show the bubble if the window is active, otherwise we may get into
    // weird situations were the bubble is deleted as soon as it is shown.
    window_->ShowBookmarkBubble(url, was_bookmarked);
  }
}

void Browser::SavePage() {
  UserMetrics::RecordAction(UserMetricsAction("SavePage"), profile_);
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab && current_tab->contents_mime_type() == "application/pdf")
    UserMetrics::RecordAction(UserMetricsAction("PDF.SavePage"), profile_);
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
  UserMetrics::RecordAction(UserMetricsAction("EmailPageLocation"), profile_);
  GetSelectedTabContents()->EmailPageLocation();
}

void Browser::Print() {
  UserMetrics::RecordAction(UserMetricsAction("PrintPreview"), profile_);
  GetSelectedTabContents()->PrintPreview();
}

void Browser::ToggleEncodingAutoDetect() {
  UserMetrics::RecordAction(UserMetricsAction("AutoDetectChange"), profile_);
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
  UserMetrics::RecordAction(UserMetricsAction("OverrideEncoding"), profile_);
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
  UserMetrics::RecordAction(UserMetricsAction("Cut"), profile_);
  window()->Cut();
}

void Browser::Copy() {
  UserMetrics::RecordAction(UserMetricsAction("Copy"), profile_);
  window()->Copy();
}

void Browser::Paste() {
  UserMetrics::RecordAction(UserMetricsAction("Paste"), profile_);
  window()->Paste();
}

void Browser::Find() {
  UserMetrics::RecordAction(UserMetricsAction("Find"), profile_);
  FindInPage(false, false);
}

void Browser::FindNext() {
  UserMetrics::RecordAction(UserMetricsAction("FindNext"), profile_);
  FindInPage(true, true);
}

void Browser::FindPrevious() {
  UserMetrics::RecordAction(UserMetricsAction("FindPrevious"), profile_);
  FindInPage(true, false);
}

void Browser::Zoom(PageZoom::Function zoom_function) {
  static const UserMetricsAction kActions[] = {
                      UserMetricsAction("ZoomMinus"),
                      UserMetricsAction("ZoomNormal"),
                      UserMetricsAction("ZoomPlus")
                      };

  UserMetrics::RecordAction(kActions[zoom_function - PageZoom::ZOOM_OUT],
                            profile_);
  TabContentsWrapper* tab_contents = GetSelectedTabContentsWrapper();
  tab_contents->render_view_host()->Zoom(zoom_function);
}

void Browser::FocusToolbar() {
  UserMetrics::RecordAction(UserMetricsAction("FocusToolbar"), profile_);
  window_->FocusToolbar();
}

void Browser::FocusAppMenu() {
  UserMetrics::RecordAction(UserMetricsAction("FocusAppMenu"), profile_);
  window_->FocusAppMenu();
}

void Browser::FocusLocationBar() {
  UserMetrics::RecordAction(UserMetricsAction("FocusLocation"), profile_);
  window_->SetFocusToLocationBar(true);
}

void Browser::FocusBookmarksToolbar() {
  UserMetrics::RecordAction(UserMetricsAction("FocusBookmarksToolbar"),
                            profile_);
  window_->FocusBookmarksToolbar();
}

void Browser::FocusChromeOSStatus() {
  UserMetrics::RecordAction(UserMetricsAction("FocusChromeOSStatus"), profile_);
  window_->FocusChromeOSStatus();
}

void Browser::FocusNextPane() {
  UserMetrics::RecordAction(UserMetricsAction("FocusNextPane"), profile_);
  window_->RotatePaneFocus(true);
}

void Browser::FocusPreviousPane() {
  UserMetrics::RecordAction(UserMetricsAction("FocusPreviousPane"), profile_);
  window_->RotatePaneFocus(false);
}

void Browser::FocusSearch() {
  // TODO(beng): replace this with FocusLocationBar
  UserMetrics::RecordAction(UserMetricsAction("FocusSearch"), profile_);
  window_->GetLocationBar()->FocusSearch();
}

void Browser::OpenFile() {
  UserMetrics::RecordAction(UserMetricsAction("OpenFile"), profile_);
#if defined(OS_CHROMEOS)
  FileBrowseUI::OpenPopup(profile_,
                          "",
                          FileBrowseUI::kPopupWidth,
                          FileBrowseUI::kPopupHeight);
#else
  if (!select_file_dialog_.get())
    select_file_dialog_ = SelectFileDialog::Create(this);

  const FilePath directory = profile_->last_selected_directory();

  // TODO(beng): figure out how to juggle this.
  gfx::NativeWindow parent_window = window_->GetNativeHandle();
  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_OPEN_FILE,
                                  string16(), directory,
                                  NULL, 0, FILE_PATH_LITERAL(""),
                                  parent_window, NULL);
#endif
}

void Browser::OpenCreateShortcutsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("CreateShortcut"), profile_);
#if defined(OS_WIN) || defined(OS_LINUX)
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
  current_tab->render_view_host()->GetApplicationInfo(entry->page_id());
#else
  NOTIMPLEMENTED();
#endif
}

void Browser::ToggleDevToolsWindow(DevToolsToggleAction action) {
  std::string uma_string;
  switch (action) {
    case DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE:
      uma_string = "DevTools_ToggleConsole";
      break;
    case DEVTOOLS_TOGGLE_ACTION_NONE:
    case DEVTOOLS_TOGGLE_ACTION_INSPECT:
    default:
      uma_string = "DevTools_ToggleWindow";
      break;
  }
  UserMetrics::RecordAction(UserMetricsAction(uma_string.c_str()), profile_);
  DevToolsManager::GetInstance()->ToggleDevToolsWindow(
      GetSelectedTabContentsWrapper()->render_view_host(), action);
}

void Browser::OpenTaskManager(bool highlight_background_resources) {
  UserMetrics::RecordAction(UserMetricsAction("TaskManager"), profile_);
  if (highlight_background_resources)
    window_->ShowBackgroundPages();
  else
    window_->ShowTaskManager();
}

void Browser::OpenBugReportDialog() {
  UserMetrics::RecordAction(UserMetricsAction("ReportBug"), profile_);
  browser::ShowHtmlBugReportView(this);
}

void Browser::ToggleBookmarkBar() {
  UserMetrics::RecordAction(UserMetricsAction("ShowBookmarksBar"), profile_);
  window_->ToggleBookmarkBar();
}

void Browser::OpenBookmarkManager() {
  UserMetrics::RecordAction(UserMetricsAction("ShowBookmarkManager"), profile_);
  ShowBookmarkManagerTab();
}

void Browser::ShowAppMenu() {
  // We record the user metric for this event in WrenchMenu::RunMenu.
  window_->ShowAppMenu();
}

void Browser::ShowBookmarkManagerTab() {
  UserMetrics::RecordAction(UserMetricsAction("ShowBookmarks"), profile_);
  ShowSingletonTab(GURL(chrome::kChromeUIBookmarksURL), false);
}

void Browser::ShowHistoryTab() {
  UserMetrics::RecordAction(UserMetricsAction("ShowHistory"), profile_);
  ShowSingletonTab(GURL(chrome::kChromeUIHistoryURL), false);
}

void Browser::ShowDownloadsTab() {
  UserMetrics::RecordAction(UserMetricsAction("ShowDownloads"), profile_);
  ShowSingletonTab(GURL(chrome::kChromeUIDownloadsURL), false);
}

void Browser::ShowExtensionsTab() {
  UserMetrics::RecordAction(UserMetricsAction("ShowExtensions"), profile_);
  ShowSingletonTab(GURL(chrome::kChromeUIExtensionsURL), false);
}

void Browser::ShowAboutConflictsTab() {
  UserMetrics::RecordAction(UserMetricsAction("AboutConflicts"), profile_);
  ShowSingletonTab(GURL(chrome::kChromeUIConflictsURL), false);
}

void Browser::ShowBrokenPageTab(TabContents* contents) {
  UserMetrics::RecordAction(UserMetricsAction("ReportBug"), profile_);
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
  ShowSingletonTab(GURL(report_page_url), false);
}

void Browser::ShowOptionsTab(const std::string& sub_page) {
  GURL url(chrome::kChromeUISettingsURL + sub_page);
  ShowSingletonTab(url, true);
}

void Browser::OpenClearBrowsingDataDialog() {
  UserMetrics::RecordAction(UserMetricsAction("ClearBrowsingData_ShowDlg"),
                            profile_);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTabbedOptions)) {
    ShowOptionsTab(
        chrome::kAdvancedOptionsSubPage + std::string(kHashMark) +
        chrome::kClearBrowserDataSubPage);
  } else {
    window_->ShowClearBrowsingDataDialog();
  }
}

void Browser::OpenOptionsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("ShowOptions"), profile_);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTabbedOptions)) {
    ShowOptionsTab(chrome::kDefaultOptionsSubPage);
  } else {
    ShowOptionsWindow(OPTIONS_PAGE_DEFAULT, OPTIONS_GROUP_NONE, profile_);
  }
}

void Browser::OpenKeywordEditor() {
  UserMetrics::RecordAction(UserMetricsAction("EditSearchEngines"), profile_);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTabbedOptions)) {
    ShowOptionsTab(chrome::kSearchEnginesSubPage);
  } else {
    window_->ShowSearchEnginesDialog();
  }
}

void Browser::OpenPasswordManager() {
  window_->ShowPasswordManager();
}

void Browser::OpenImportSettingsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("Import_ShowDlg"), profile_);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTabbedOptions)) {
    ShowOptionsTab(
        chrome::kPersonalOptionsSubPage + std::string(kHashMark) +
        chrome::kImportDataSubPage);
  } else {
    window_->ShowImportDialog();
  }
}

void Browser::OpenSyncMyBookmarksDialog() {
  sync_ui_util::OpenSyncMyBookmarksDialog(
      profile_, this, ProfileSyncService::START_FROM_WRENCH);
}

void Browser::OpenAboutChromeDialog() {
  UserMetrics::RecordAction(UserMetricsAction("AboutChrome"), profile_);
#if defined(OS_CHROMEOS)
  ShowSingletonTab(GURL(chrome::kChromeUIAboutURL), false);
#else
  window_->ShowAboutChromeDialog();
#endif
}

void Browser::OpenUpdateChromeDialog() {
  UserMetrics::RecordAction(UserMetricsAction("UpdateChrome"), profile_);
  window_->ShowUpdateChromeDialog();
}

void Browser::OpenHelpTab() {
  GURL help_url(kHelpContentUrl);
  GURL localized_help_url = google_util::AppendGoogleLocaleParam(help_url);
  AddSelectedTabWithURL(localized_help_url, PageTransition::AUTO_BOOKMARK);
}

void Browser::OpenThemeGalleryTabAndActivate() {
  AddSelectedTabWithURL(GURL(l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_URL)),
                        PageTransition::LINK);
}

void Browser::OpenPrivacyDashboardTabAndActivate() {
  OpenURL(GURL(kPrivacyDashboardUrl), GURL(),
          NEW_FOREGROUND_TAB, PageTransition::LINK);
  window_->Activate();
}

void Browser::OpenAutoFillHelpTabAndActivate() {
  GURL help_url = google_util::AppendGoogleLocaleParam(GURL(kAutofillHelpUrl));
  AddSelectedTabWithURL(help_url, PageTransition::LINK);
}

void Browser::OpenSearchEngineOptionsDialog() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTabbedOptions)) {
    OpenKeywordEditor();
  } else {
    ShowOptionsWindow(OPTIONS_PAGE_GENERAL, OPTIONS_GROUP_DEFAULT_SEARCH,
                      profile_);
  }
}

#if defined(OS_CHROMEOS)
void Browser::OpenSystemOptionsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("OpenSystemOptionsDialog"),
                            profile_);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTabbedOptions)) {
    ShowOptionsTab(chrome::kSystemOptionsSubPage);
  } else {
    ShowOptionsWindow(OPTIONS_PAGE_SYSTEM, OPTIONS_GROUP_NONE,
                      profile_);
  }
}

void Browser::OpenInternetOptionsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("OpenInternetOptionsDialog"),
                            profile_);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTabbedOptions)) {
    ShowOptionsTab(chrome::kInternetOptionsSubPage);
  } else {
    ShowOptionsWindow(OPTIONS_PAGE_INTERNET, OPTIONS_GROUP_DEFAULT_SEARCH,
                      profile_);
  }
}

void Browser::OpenLanguageOptionsDialog() {
  UserMetrics::RecordAction(UserMetricsAction("OpenLanguageOptionsDialog"),
                            profile_);
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTabbedOptions)) {
    ShowOptionsTab(chrome::kLanguageOptionsSubPage);
  } else {
    // Language options dialog has been replaced by DOMUI.
  }
}

void Browser::OpenSystemTabAndActivate() {
  OpenURL(GURL(chrome::kChromeUISystemInfoURL), GURL(),
          NEW_FOREGROUND_TAB, PageTransition::LINK);
  window_->Activate();
}

void Browser::OpenMobilePlanTabAndActivate() {
  OpenURL(GURL(chrome::kChromeUIMobileSetupURL), GURL(),
          NEW_FOREGROUND_TAB, PageTransition::LINK);
  window_->Activate();
}
#endif

void Browser::OpenPluginsTabAndActivate() {
  OpenURL(GURL(chrome::kAboutPluginsURL), GURL(),
          NEW_FOREGROUND_TAB, PageTransition::LINK);
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
  prefs->RegisterDictionaryPref(prefs::kPreferencesWindowPlacement);
  prefs->RegisterIntegerPref(prefs::kExtensionSidebarWidth, -1);
  prefs->RegisterIntegerPref(prefs::kMultipleProfilePrefMigration, 0);
  // Educated guess: Chrome has a bundled Flash version supporting
  // clearing LSO data, Chromium hasn't.
#if defined(GOOGLE_CHROME_BUILD)
  prefs->RegisterBooleanPref(prefs::kClearPluginLSODataEnabled, true);
#else
  prefs->RegisterBooleanPref(prefs::kClearPluginLSODataEnabled, false);
#endif
}

// static
void Browser::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kHomePage,
                            chrome::kChromeUINewTabURL);
  prefs->RegisterBooleanPref(prefs::kHomePageIsNewTabPage, true);
  prefs->RegisterBooleanPref(prefs::kClearSiteDataOnExit, false);
  prefs->RegisterBooleanPref(prefs::kShowHomeButton, false);
#if defined(OS_MACOSX)
  // This really belongs in platform code, but there's no good place to
  // initialize it between the time when the AppController is created
  // (where there's no profile) and the time the controller gets another
  // crack at the start of the main event loop. By that time, BrowserInit
  // has already created the browser window, and it's too late: we need the
  // pref to be already initialized. Doing it here also saves us from having
  // to hard-code pref registration in the several unit tests that use
  // this preference.
  prefs->RegisterBooleanPref(prefs::kShowPageOptionsButtons, false);
  prefs->RegisterBooleanPref(prefs::kShowUpdatePromotionInfoBar, true);
#endif
  prefs->RegisterStringPref(prefs::kRecentlySelectedEncoding, "");
  prefs->RegisterBooleanPref(prefs::kDeleteBrowsingHistory, true);
  prefs->RegisterBooleanPref(prefs::kDeleteDownloadHistory, true);
  prefs->RegisterBooleanPref(prefs::kDeleteCache, true);
  prefs->RegisterBooleanPref(prefs::kDeleteCookies, true);
  prefs->RegisterBooleanPref(prefs::kDeletePasswords, false);
  prefs->RegisterBooleanPref(prefs::kDeleteFormData, false);
  prefs->RegisterIntegerPref(prefs::kDeleteTimePeriod, 0);
  prefs->RegisterBooleanPref(prefs::kCheckDefaultBrowser, true);
  prefs->RegisterBooleanPref(prefs::kShowOmniboxSearchHint, true);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateOnDesktop, true);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateInAppsMenu, true);
  prefs->RegisterBooleanPref(prefs::kWebAppCreateInQuickLaunchBar, true);
  prefs->RegisterBooleanPref(prefs::kUseVerticalTabs, false);
  prefs->RegisterBooleanPref(prefs::kEnableTranslate, true);
  prefs->RegisterBooleanPref(prefs::kRemotingHasSetupCompleted, false);
  prefs->RegisterStringPref(prefs::kCloudPrintEmail, std::string());
  prefs->RegisterBooleanPref(prefs::kCloudPrintProxyEnabled, true);
  prefs->RegisterBooleanPref(prefs::kDevToolsDisabled, false);
  prefs->RegisterBooleanPref(prefs::kIncognitoEnabled, true);
  prefs->RegisterDoublePref(prefs::kDefaultZoomLevel, 0.0);
  prefs->RegisterIntegerPref(prefs::kDevToolsSplitLocation, -1);
  prefs->RegisterDictionaryPref(prefs::kBrowserWindowPlacement);
  // We need to register the type of this preference in order to query
  // it even though it's only typically controlled via policy.
  prefs->RegisterBooleanPref(prefs::kDisable3DAPIs, false);
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
    case IDC_FULLSCREEN:            ToggleFullscreenMode();           break;
    case IDC_EXIT:                  Exit();                           break;
    case IDC_TOGGLE_VERTICAL_TABS:  ToggleUseVerticalTabs();          break;
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
    case IDC_ZOOM_PLUS:             Zoom(PageZoom::ZOOM_IN);          break;
    case IDC_ZOOM_NORMAL:           Zoom(PageZoom::RESET);            break;
    case IDC_ZOOM_MINUS:            Zoom(PageZoom::ZOOM_OUT);         break;

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
    case IDC_SHOW_HISTORY:          ShowHistoryTab();                 break;
    case IDC_SHOW_DOWNLOADS:        ShowDownloadsTab();               break;
    case IDC_MANAGE_EXTENSIONS:     ShowExtensionsTab();              break;
    case IDC_SYNC_BOOKMARKS:        OpenSyncMyBookmarksDialog();      break;
    case IDC_OPTIONS:               OpenOptionsDialog();              break;
    case IDC_EDIT_SEARCH_ENGINES:   OpenKeywordEditor();              break;
    case IDC_VIEW_PASSWORDS:        OpenPasswordManager();            break;
    case IDC_CLEAR_BROWSING_DATA:   OpenClearBrowsingDataDialog();    break;
    case IDC_IMPORT_SETTINGS:       OpenImportSettingsDialog();       break;
    case IDC_ABOUT:                 OpenAboutChromeDialog();          break;
    case IDC_UPGRADE_DIALOG:        OpenUpdateChromeDialog();         break;
    case IDC_VIEW_INCOMPATIBILITIES: ShowAboutConflictsTab();         break;
    case IDC_HELP_PAGE:             OpenHelpTab();                    break;
#if defined(OS_CHROMEOS)
    case IDC_SYSTEM_OPTIONS:        OpenSystemOptionsDialog();        break;
    case IDC_INTERNET_OPTIONS:      OpenInternetOptionsDialog();      break;
    case IDC_LANGUAGE_OPTIONS:      OpenLanguageOptionsDialog();      break;
#endif

    default:
      LOG(WARNING) << "Received Unimplemented Command: " << id;
      break;
  }
}

bool Browser::ExecuteCommandIfEnabled(int id) {
  if (command_updater_.SupportsCommand(id) &&
      command_updater_.IsCommandEnabled(id)) {
    ExecuteCommand(id);
    return true;
  }
  return false;
}

bool Browser::IsReservedCommand(int command_id) {
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
                                         PageTransition::Type transition,
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

GURL Browser::GetHomePage() const {
  // --homepage overrides any preferences.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kHomePage)) {
    // TODO(evanm): clean up usage of DIR_CURRENT.
    //   http://code.google.com/p/chromium/issues/detail?id=60630
    // For now, allow this code to call getcwd().
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    FilePath browser_directory;
    PathService::Get(base::DIR_CURRENT, &browser_directory);
    GURL home_page(URLFixerUpper::FixupRelativeFile(browser_directory,
        command_line.GetSwitchValuePath(switches::kHomePage)));
    if (home_page.is_valid())
      return home_page;
  }

  if (profile_->GetPrefs()->GetBoolean(prefs::kHomePageIsNewTabPage))
    return GURL(chrome::kChromeUINewTabURL);
  GURL home_page(URLFixerUpper::FixupURL(
      profile_->GetPrefs()->GetString(prefs::kHomePage),
      std::string()));
  if (!home_page.is_valid())
    return GURL(chrome::kChromeUINewTabURL);
  return home_page;
}

///////////////////////////////////////////////////////////////////////////////
// Browser, PageNavigator implementation:

void Browser::OpenURL(const GURL& url, const GURL& referrer,
                      WindowOpenDisposition disposition,
                      PageTransition::Type transition) {
  OpenURLFromTab(NULL, url, referrer, disposition, transition);
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
                                 PageTransition::TYPED);
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
  Browser* browser = new Browser(TYPE_NORMAL, profile_);
  browser->set_override_bounds(new_window_bounds);
  browser->set_maximized_state(
      maximize ? MAXIMIZED_STATE_MAXIMIZED : MAXIMIZED_STATE_UNMAXIMIZED);
  browser->CreateBrowserWindow();
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
    PageTransition::Type transition, bool defer_load,
    SiteInstance* instance) const {
  TabContentsWrapper* contents = TabContentsFactory(profile, instance,
      MSG_ROUTING_NONE,
      GetSelectedTabContents(), NULL);
  if (!defer_load) {
    // Load the initial URL before adding the new tab contents to the tab strip
    // so that the tab contents has navigation state.
    contents->controller().LoadURL(url, referrer, transition);
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
  TabContents* new_contents = contents_dupe->tab_contents();

  bool pinned = false;
  if (CanSupportWindowFeature(FEATURE_TABSTRIP)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    int index = tab_handler_->GetTabStripModel()->
        GetIndexOfTabContents(contents);
    pinned = tab_handler_->GetTabStripModel()->IsTabPinned(index);
    int add_types = TabStripModel::ADD_SELECTED |
        TabStripModel::ADD_INHERIT_GROUP |
        (pinned ? TabStripModel::ADD_PINNED : 0);
    tab_handler_->GetTabStripModel()->InsertTabContentsAt(index + 1,
                                                          contents_dupe,
                                                          add_types);
  } else {
    Browser* browser = NULL;
    if (type_ & TYPE_APP) {
      CHECK((type_ & TYPE_POPUP) == 0);
      CHECK(type_ != TYPE_APP_PANEL);
      browser = Browser::CreateForApp(app_name_, gfx::Size(), profile_,
                                      false);
    } else if (type_ == TYPE_POPUP) {
      browser = Browser::CreateForType(TYPE_POPUP, profile_);
    }

    // Preserve the size of the original window. The new window has already
    // been given an offset by the OS, so we shouldn't copy the old bounds.
    BrowserWindow* new_window = browser->window();
    new_window->SetBounds(gfx::Rect(new_window->GetRestoredBounds().origin(),
                          window()->GetRestoredBounds().size()));

    // We need to show the browser now. Otherwise ContainerWin assumes the
    // TabContents is invisible and won't size it.
    browser->window()->Show();

    // The page transition below is only for the purpose of inserting the tab.
    browser->AddTab(contents_dupe, PageTransition::LINK);
  }

  if (profile_->HasSessionService()) {
    SessionService* session_service = profile_->GetSessionService();
    if (session_service)
      session_service->TabRestored(&new_contents->controller(), pinned);
  }
}

void Browser::CloseFrameAfterDragSession() {
#if defined(OS_WIN) || defined(OS_LINUX)
  // This is scheduled to run after we return to the message loop because
  // otherwise the frame will think the drag session is still active and ignore
  // the request.
  // TODO(port): figure out what is required here in a cross-platform world
  MessageLoop::current()->PostTask(
      FROM_HERE, method_factory_.NewRunnableMethod(&Browser::CloseFrame));
#endif
}

void Browser::CreateHistoricalTab(TabContentsWrapper* contents) {
  // We don't create historical tabs for incognito windows or windows without
  // profiles.
  if (!profile() || profile()->IsOffTheRecord() ||
      !profile()->GetTabRestoreService()) {
    return;
  }

  // We only create historical tab entries for tabbed browser windows.
  if (CanSupportWindowFeature(FEATURE_TABSTRIP)) {
    profile()->GetTabRestoreService()->CreateHistoricalTab(
        &contents->controller());
  }
}

bool Browser::RunUnloadListenerBeforeClosing(TabContentsWrapper* contents) {
  return Browser::RunUnloadEventsHelper(contents->tab_contents());
}

bool Browser::CanReloadContents(TabContents* source) const {
  return type() != TYPE_DEVTOOLS;
}

bool Browser::CanCloseContentsAt(int index) {
  if (!CanCloseTab())
    return false;
  if (tab_handler_->GetTabStripModel()->count() > 1)
    return true;
  // We are closing the last tab for this browser. Make sure to check for
  // in-progress downloads.
  // Note that the next call when it returns false will ask the user for
  // confirmation before closing the browser if the user decides so.
  return CanCloseWithInProgressDownloads();
}

bool Browser::CanBookmarkAllTabs() const {
  BookmarkModel* model = profile()->GetBookmarkModel();
  return (model && model->IsLoaded() && (tab_count() > 1));
}

void Browser::BookmarkAllTabs() {
  BookmarkModel* model = profile()->GetBookmarkModel();
  DCHECK(model && model->IsLoaded());

  BookmarkEditor::EditDetails details;
  details.type = BookmarkEditor::EditDetails::NEW_FOLDER;
  bookmark_utils::GetURLsForOpenTabs(this, &(details.urls));
  DCHECK(!details.urls.empty());

  BookmarkEditor::Show(window()->GetNativeHandle(), profile_,
                       model->GetParentForNewNodes(),  details,
                       BookmarkEditor::SHOW_TREE);
}

bool Browser::CanCloseTab() const {
  TabCloseableStateWatcher* watcher =
      g_browser_process->tab_closeable_state_watcher();
  return !watcher || watcher->CanCloseTab(this);
}

void Browser::ToggleUseVerticalTabs() {
  use_vertical_tabs_.SetValue(!UseVerticalTabs());
  UseVerticalTabsChanged();
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
  contents->set_delegate(this);
  contents->tab_contents()->set_delegate(this);
  contents->controller().SetWindowID(session_id());

  SyncHistoryWithTabs(index);

  // Make sure the loading state is updated correctly, otherwise the throbber
  // won't start if the page is loading.
  LoadingStateChanged(contents->tab_contents());

  // If the tab crashes in the beforeunload or unload handler, it won't be
  // able to ack. But we know we can close it.
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DISCONNECTED,
                 Source<TabContentsWrapper>(contents));
}

void Browser::TabClosingAt(TabStripModel* tab_strip_model,
                           TabContentsWrapper* contents,
                           int index) {
  NotificationService::current()->Notify(
      NotificationType::TAB_CLOSING,
      Source<NavigationController>(&contents->controller()),
      NotificationService::NoDetails());

  // Sever the TabContents' connection back to us.
  contents->set_delegate(NULL);
  contents->tab_contents()->set_delegate(NULL);
}

void Browser::TabDetachedAt(TabContentsWrapper* contents, int index) {
  TabDetachedAtImpl(contents, index, DETACH_TYPE_DETACH);
}

void Browser::TabDeselectedAt(TabContentsWrapper* contents, int index) {
  if (instant())
    instant()->DestroyPreviewContents();

  // Save what the user's currently typing, so it can be restored when we
  // switch back to this tab.
  window_->GetLocationBar()->SaveStateToContents(contents->tab_contents());
}

void Browser::TabSelectedAt(TabContentsWrapper* old_contents,
                            TabContentsWrapper* new_contents,
                            int index,
                            bool user_gesture) {
  DCHECK(old_contents != new_contents);

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

  // If we have any update pending, do it now.
  if (!chrome_updater_factory_.empty() && old_contents)
    ProcessPendingUIUpdates();

  // Propagate the profile to the location bar.
  UpdateToolbar(true);

  // Update reload/stop state.
  UpdateReloadStopState(new_contents->tab_contents()->is_loading(), true);

  // Update commands to reflect current state.
  UpdateCommandsForTabState();

  // Reset the status bubble.
  StatusBubble* status_bubble = GetStatusBubble();
  if (status_bubble) {
    status_bubble->Hide();

    // Show the loading state (if any).
    status_bubble->SetStatus(GetSelectedTabContents()->GetStatusText());
  }

  if (HasFindBarController()) {
    find_bar_controller_->ChangeTabContents(new_contents);
    find_bar_controller_->find_bar()->MoveWindowIfNecessary(gfx::Rect(), true);
  }

  // Update sessions. Don't force creation of sessions. If sessions doesn't
  // exist, the change will be picked up by sessions when created.
  if (profile_->HasSessionService()) {
    SessionService* session_service = profile_->GetSessionService();
    if (session_service && !tab_handler_->GetTabStripModel()->closing_all()) {
      session_service->SetSelectedTabInWindow(
          session_id(), tab_handler_->GetTabStripModel()->selected_index());
    }
  }
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
                (index == tab_handler_->GetTabStripModel()->selected_index()));

  int entry_count = new_contents->controller().entry_count();
  if (entry_count > 0) {
    // Send out notification so that observers are updated appropriately.
    new_contents->controller().NotifyEntryChanged(
        new_contents->controller().GetEntryAtIndex(entry_count - 1),
        entry_count - 1);
  }

  SessionService* session_service = profile()->GetSessionService();
  if (session_service) {
    // The new_contents may end up with a different navigation stack. Force
    // the session service to update itself.
    session_service->TabRestored(
        &new_contents->controller(),
        tab_handler_->GetTabStripModel()->IsTabPinned(index));
  }

  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
  if (devtools_manager)  // NULL in unit tests.
    devtools_manager->TabReplaced(old_contents, new_contents);
}

void Browser::TabPinnedStateChanged(TabContentsWrapper* contents, int index) {
  if (!profile()->HasSessionService())
    return;
  SessionService* session_service = profile()->GetSessionService();
  if (session_service) {
    session_service->SetPinnedState(
        session_id(),
        GetTabContentsAt(index)->controller().session_id(),
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
      FROM_HERE, method_factory_.NewRunnableMethod(&Browser::CloseFrame));
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabContentsDelegate implementation:

void Browser::OpenURLFromTab(TabContents* source,
                             const GURL& url,
                             const GURL& referrer,
                             WindowOpenDisposition disposition,
                             PageTransition::Type transition) {
  browser::NavigateParams params(this, url, transition);
  params.source_contents =
    tabstrip_model()->GetTabContentsAt(
      tabstrip_model()->GetWrapperIndex(source));
  params.referrer = referrer;
  params.disposition = disposition;
  params.tabstrip_add_types = TabStripModel::ADD_NONE;
  params.show_window = true;
  browser::Navigate(&params);
}

void Browser::NavigationStateChanged(const TabContents* source,
                                     unsigned changed_flags) {
  // Only update the UI when something visible has changed.
  if (changed_flags)
    ScheduleUIUpdate(source, changed_flags);

  // We don't schedule updates to commands since they will only change once per
  // navigation, so we don't have to worry about flickering.
  if (changed_flags & TabContents::INVALIDATE_URL)
    UpdateCommandsForTabState();
}

void Browser::AddNewContents(TabContents* source,
                             TabContents* new_contents,
                             WindowOpenDisposition disposition,
                             const gfx::Rect& initial_pos,
                             bool user_gesture) {
  // No code for this yet
  DCHECK(disposition != SAVE_TO_DISK);
  // Can't create a new contents for the current tab - invalid case.
  DCHECK(disposition != CURRENT_TAB);

  // TODO(beng): This belongs behind the platform-specific View interface.
  //             That's why it's there.
#if defined(OS_CHROMEOS)
  if (disposition == NEW_POPUP) {
    // If the popup is bigger than a given factor of the screen, then
    // turn it into a foreground tab (on chrome os only)
    // Also check for width or height == 0, which would otherwise indicate
    // a tab sized popup window.
    GdkScreen* screen = gdk_screen_get_default();
    int max_width = gdk_screen_get_width(screen) * kPopupMaxWidthFactor;
    int max_height = gdk_screen_get_height(screen) * kPopupMaxHeightFactor;
    if (initial_pos.width() > max_width || initial_pos.width() == 0 ||
        initial_pos.height() > max_height || initial_pos.height() == 0) {
      disposition = NEW_FOREGROUND_TAB;
    }
  }
#endif

  TabContentsWrapper* wrapper = new TabContentsWrapper(new_contents);
  browser::NavigateParams params(this, wrapper);
  params.source_contents =
      tabstrip_model()->GetTabContentsAt(
          tabstrip_model()->GetWrapperIndex(source));
  params.disposition = disposition;
  params.window_bounds = initial_pos;
  params.show_window = true;
  browser::Navigate(&params);
}

void Browser::ActivateContents(TabContents* contents) {
  tab_handler_->GetTabStripModel()->SelectTabContentsAt(
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
    UpdateReloadStopState(source->is_loading(), false);
    if (GetStatusBubble()) {
      GetStatusBubble()->SetStatus(GetSelectedTabContents()->GetStatusText());
    }

    if (!source->is_loading() &&
        pending_web_app_action_ == UPDATE_SHORTCUT) {
      // Schedule a shortcut update when web application info is available if
      // last committed entry is not NULL. Last committed entry could be NULL
      // when an interstitial page is injected (e.g. bad https certificate,
      // malware site etc). When this happens, we abort the shortcut update.
      NavigationEntry* entry = source->controller().GetLastCommittedEntry();
      if (entry) {
        source->render_view_host()->GetApplicationInfo(entry->page_id());
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
  if ((type() & TYPE_POPUP) == 0) {
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

bool Browser::IsPopup(const TabContents* source) const {
  // A non-tabbed BROWSER is an unconstrained popup.
  return !!(type() & TYPE_POPUP);
}

void Browser::ToolbarSizeChanged(TabContents* source, bool is_animating) {
  if (source == GetSelectedTabContents() || source == NULL) {
    // This will refresh the shelf if needed.
    window_->SelectedTabToolbarSizeChanged(is_animating);
  }
}

void Browser::ContentsMouseEvent(
    TabContents* source, const gfx::Point& location, bool motion) {
  if (!GetStatusBubble())
    return;

  if (source == GetSelectedTabContents()) {
    GetStatusBubble()->MouseMoved(location, !motion);
    if (!motion)
      GetStatusBubble()->SetURL(GURL(), string16());
  }
}

void Browser::UpdateTargetURL(TabContents* source, const GURL& url) {
  if (!GetStatusBubble())
    return;

  if (source == GetSelectedTabContents()) {
    PrefService* prefs = profile_->GetPrefs();
    GetStatusBubble()->SetURL(
        url, UTF8ToUTF16(prefs->GetString(prefs::kAcceptLanguages)));
  }
}

void Browser::UpdateDownloadShelfVisibility(bool visible) {
  if (GetStatusBubble())
    GetStatusBubble()->UpdateDownloadShelfVisibility(visible);
}

bool Browser::UseVerticalTabs() const {
  return use_vertical_tabs_.GetValue();
}

void Browser::ContentsZoomChange(bool zoom_in) {
  ExecuteCommand(zoom_in ? IDC_ZOOM_PLUS : IDC_ZOOM_MINUS);
}

void Browser::OnContentSettingsChange(TabContents* source) {
  if (source == GetSelectedTabContents())
    window_->GetLocationBar()->UpdateContentSettingsIcons();
}

void Browser::SetTabContentBlocked(TabContents* contents, bool blocked) {
  int index = tabstrip_model()->GetWrapperIndex(contents);
  if (index == TabStripModel::kNoTab) {
    NOTREACHED();
    return;
  }
  tabstrip_model()->SetTabBlocked(index, blocked);
}

void Browser::TabContentsFocused(TabContents* tab_content) {
  window_->TabContentsFocused(tab_content);
}

bool Browser::TakeFocus(bool reverse) {
  NotificationService::current()->Notify(
      NotificationType::FOCUS_RETURNED_TO_BROWSER,
      Source<Browser>(this),
      NotificationService::NoDetails());
  return false;
}

bool Browser::IsApplication() const {
  return (type_ & TYPE_APP) != 0;
}

void Browser::ConvertContentsToApplication(TabContents* contents) {
  const GURL& url = contents->controller().GetActiveEntry()->url();
  std::string app_name = web_app::GenerateApplicationNameFromURL(url);
  RegisterAppPrefs(app_name, contents->profile());

  DetachContents(contents);
  Browser* app_browser = Browser::CreateForApp(
      app_name, gfx::Size(), profile_, false);
  TabContentsWrapper* wrapper = new TabContentsWrapper(contents);
  app_browser->tabstrip_model()->AppendTabContents(wrapper, true);

  contents->GetMutableRendererPrefs()->can_accept_load_drops = false;
  contents->render_view_host()->SyncRendererPrefs();
  app_browser->window()->Show();
}

bool Browser::ShouldDisplayURLField() {
  return !IsApplication();
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

void Browser::ShowHtmlDialog(HtmlDialogUIDelegate* delegate,
                             gfx::NativeWindow parent_window) {
  window_->ShowHTMLDialog(delegate, parent_window);
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

void Browser::OnStartDownload(DownloadItem* download, TabContents* tab) {
  if (!window())
    return;

#if defined(OS_CHROMEOS)
  // Don't show content browser for extension/theme downloads from gallery.
  if (download->is_extension_install()) {
    ExtensionService* service = profile_->GetExtensionService();
    if (service && service->IsDownloadFromGallery(download->url(),
                                                  download->referrer_url())) {
      return;
    }
  }

  // skip the download shelf and just open the file browser in chromeos
  std::string arg = download->full_path().DirName().value();
  FileBrowseUI::OpenPopup(profile_,
                          arg,
                          FileBrowseUI::kPopupWidth,
                          FileBrowseUI::kPopupHeight);

#else
  // GetDownloadShelf creates the download shelf if it was not yet created.
  window()->GetDownloadShelf()->AddDownload(new DownloadItemModel(download));

  // Don't show the animation for "Save file" downloads.
  if (download->total_bytes() <= 0)
    return;

  // For non-theme extensions, we don't show the download animation.
  if (download->is_extension_install() &&
      !ExtensionService::IsDownloadFromMiniGallery(download->url()))
    return;

  TabContents* current_tab = GetSelectedTabContents();
  // We make this check for the case of minimized windows, unit tests, etc.
  if (platform_util::IsVisible(current_tab->GetNativeView()) &&
      ui::Animation::ShouldRenderRichAnimation()) {
    DownloadStartedAnimation::Show(current_tab);
  }
#endif

  // If the download occurs in a new tab, close it
  if (tab->controller().IsInitialNavigation() &&
      GetConstrainingContents(tab) == tab && tab_count() > 1) {
    CloseContents(tab);
  }
}

void Browser::ConfirmSetDefaultSearchProvider(
    TabContents* tab_contents,
    TemplateURL* template_url,
    TemplateURLModel* template_url_model) {
  window()->ConfirmSetDefaultSearchProvider(tab_contents, template_url,
                                            template_url_model);
}
void Browser::ConfirmAddSearchProvider(const TemplateURL* template_url,
                                       Profile* profile) {
  window()->ConfirmAddSearchProvider(template_url, profile);
}

void Browser::ShowPageInfo(Profile* profile,
                           const GURL& url,
                           const NavigationEntry::SSLStatus& ssl,
                           bool show_history) {
  window()->ShowPageInfo(profile, url, ssl, show_history);
}

void Browser::ViewSourceForTab(TabContents* source, const GURL& page_url) {
  DCHECK(source);
  int index = tabstrip_model()->GetWrapperIndex(source);
  TabContentsWrapper* wrapper = tabstrip_model()->GetTabContentsAt(index);
  ViewSource(wrapper);
}

bool Browser::PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                     bool* is_keyboard_shortcut) {
  return window()->PreHandleKeyboardEvent(event, is_keyboard_shortcut);
}

void Browser::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  window()->HandleKeyboardEvent(event);
}

void Browser::ShowRepostFormWarningDialog(TabContents *tab_contents) {
  window()->ShowRepostFormWarningDialog(tab_contents);
}

void Browser::ShowContentSettingsWindow(ContentSettingsType content_type) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTabbedOptions)) {
    ShowOptionsTab(
        chrome::kContentSettingsExceptionsSubPage + std::string(kHashMark) +
        ContentSettingsHandler::ContentSettingsTypeToGroupName(content_type));
  } else {
    window()->ShowContentSettingsWindow(content_type,
                                        profile_->GetOriginalProfile());
  }
}

void Browser::ShowCollectedCookiesDialog(TabContents *tab_contents) {
  window()->ShowCollectedCookiesDialog(tab_contents);
}

bool Browser::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    NavigationType::Type navigation_type) {
  // Don't update history if running as app.
  return !IsApplication();
}

void Browser::OnDidGetApplicationInfo(TabContents* tab_contents,
                                      int32 page_id) {
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab != tab_contents)
    return;

  NavigationEntry* entry = current_tab->controller().GetLastCommittedEntry();
  if (!entry || (entry->page_id() != page_id))
    return;

  switch (pending_web_app_action_) {
    case CREATE_SHORTCUT: {
      window()->ShowCreateWebAppShortcutsDialog(current_tab);
      break;
    }
    case UPDATE_SHORTCUT: {
      web_app::UpdateShortcutForTabContents(current_tab);
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
  ExtensionService* extensions_service = profile()->GetExtensionService();
  if (!extensions_service)
    return;

  scoped_refptr<CrxInstaller> installer(
      new CrxInstaller(extensions_service,
                       extensions_service->show_extensions_prompts() ?
                       new ExtensionInstallUI(profile()) : NULL));
  installer->InstallWebApp(web_app);
}

void Browser::ContentRestrictionsChanged(TabContents* source) {
  UpdateCommandsForContentRestrictionState();
}

///////////////////////////////////////////////////////////////////////////////
// Browser, TabContentsWrapperDelegate implementation:

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
    OpenURL(file_url, GURL(), CURRENT_TAB, PageTransition::TYPED);
}

///////////////////////////////////////////////////////////////////////////////
// Browser, NotificationObserver implementation:

void Browser::Observe(NotificationType type,
                      const NotificationSource& source,
                      const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::TAB_CONTENTS_DISCONNECTED:
      if (is_attempting_to_close_browser_) {
        // Pass in false so that we delay processing. We need to delay the
        // processing as it may close the tab, which is currently on the call
        // stack above us.
        ClearUnloadState(Source<TabContents>(source).ptr(), false);
      }
      break;

    case NotificationType::SSL_VISIBLE_STATE_CHANGED:
      // When the current tab's SSL state changes, we need to update the URL
      // bar to reflect the new state. Note that it's possible for the selected
      // tab contents to be NULL. This is because we listen for all sources
      // (NavigationControllers) for convenience, so the notification could
      // actually be for a different window while we're doing asynchronous
      // closing of this one.
      if (GetSelectedTabContents() &&
          &GetSelectedTabContents()->controller() ==
          Source<NavigationController>(source).ptr())
        UpdateToolbar(false);
      break;

    case NotificationType::EXTENSION_UPDATE_DISABLED: {
      // Show the UI if the extension was disabled for escalated permissions.
      Profile* profile = Source<Profile>(source).ptr();
      if (profile_->IsSameProfile(profile)) {
        ExtensionService* service = profile->GetExtensionService();
        DCHECK(service);
        const Extension* extension = Details<const Extension>(details).ptr();
        if (service->extension_prefs()->DidExtensionEscalatePermissions(
                extension->id()))
          ShowExtensionDisabledUI(service, profile_, extension);
      }
      break;
    }

    case NotificationType::EXTENSION_UNLOADED: {
      window()->GetLocationBar()->UpdatePageActions();

      // Close any tabs from the unloaded extension.
      const Extension* extension =
          Details<UnloadedExtensionInfo>(details)->extension;
      TabStripModel* model = tab_handler_->GetTabStripModel();
      for (int i = model->count() - 1; i >= 0; --i) {
        TabContents* tc = model->GetTabContentsAt(i)->tab_contents();
        if (tc->GetURL().SchemeIs(chrome::kExtensionScheme) &&
            tc->GetURL().host() == extension->id()) {
          CloseTabContents(tc);
        }
      }

      break;
    }

    case NotificationType::EXTENSION_PROCESS_TERMINATED: {
      window()->GetLocationBar()->InvalidatePageActions();

      TabContents* tab_contents = GetSelectedTabContents();
      if (!tab_contents)
        break;
      ExtensionService* extensions_service =
          Source<Profile>(source).ptr()->GetExtensionService();
      ExtensionHost* extension_host = Details<ExtensionHost>(details).ptr();
      tab_contents->AddInfoBar(new CrashedExtensionInfoBarDelegate(
          tab_contents, extensions_service, extension_host->extension()));
      break;
    }

    case NotificationType::EXTENSION_LOADED: {
      window()->GetLocationBar()->UpdatePageActions();

      // If any "This extension has crashed" InfoBarDelegates are around for
      // this extension, it means that it has been reloaded in another window
      // so just remove the remaining CrashedExtensionInfoBarDelegate objects.
      const Extension* extension = Details<const Extension>(details).ptr();
      TabStripModel* model = tab_handler_->GetTabStripModel();
      for (int m = 0; m < model->count(); ++m) {
        TabContents* tab_contents = model->GetTabContentsAt(m)->tab_contents();
        for (size_t i = 0; i < tab_contents->infobar_count(); ) {
          CrashedExtensionInfoBarDelegate* delegate = tab_contents->
              GetInfoBarDelegateAt(i)->AsCrashedExtensionInfoBarDelegate();
          if (delegate && delegate->extension_id() == extension->id())
            tab_contents->RemoveInfoBar(delegate);
          else
            ++i;
        }
      }
      break;
    }

    case NotificationType::BROWSER_THEME_CHANGED:
      window()->UserChangedTheme();
      break;

    case NotificationType::EXTENSION_READY_FOR_INSTALL: {
      // Handle EXTENSION_READY_FOR_INSTALL for last active normal browser.
      if (BrowserList::FindBrowserWithType(profile(),
                                           Browser::TYPE_NORMAL,
                                           true) != this)
        break;

      // We only want to show the loading dialog for themes, but we don't want
      // to wait until unpack to find out an extension is a theme, so we test
      // the download_url GURL instead. This means that themes in the extensions
      // gallery won't get the loading dialog.
      GURL download_url = *(Details<GURL>(details).ptr());
      if (ExtensionService::IsDownloadFromMiniGallery(download_url))
        window()->ShowThemeInstallBubble();
      break;
    }

    case NotificationType::PROFILE_ERROR: {
      if (BrowserList::GetLastActive() != this)
        break;
      int* message_id = Details<int>(details).ptr();
      window()->ShowProfileErrorDialog(*message_id);
      break;
    }

    case NotificationType::PREF_CHANGED: {
      const std::string& pref_name = *Details<std::string>(details).ptr();
      if (pref_name == prefs::kUseVerticalTabs) {
        UseVerticalTabsChanged();
      } else if (pref_name == prefs::kPrintingEnabled) {
        UpdatePrintingState(0);
      } else if (pref_name == prefs::kInstantEnabled) {
        if (!InstantController::IsEnabled(profile())) {
          if (instant()) {
            instant()->DestroyPreviewContents();
            instant_.reset();
            instant_unload_handler_.reset();
          }
        } else {
          CreateInstantIfNecessary();
        }
      } else if (pref_name == prefs::kDevToolsDisabled) {
        UpdateCommandsForDevTools();
        if (dev_tools_disabled_.GetValue())
          g_browser_process->devtools_manager()->CloseAllClientHosts();
      } else if (pref_name == prefs::kIncognitoEnabled) {
        break;  // No further action is required.
      } else {
        NOTREACHED();
      }
      break;
    }

    default:
      NOTREACHED() << "Got a notification we didn't register for.";
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, ProfileSyncServiceObserver implementation:

void Browser::OnStateChanged() {
  DCHECK(profile_->GetProfileSyncService());

#if !defined(OS_MACOSX)
  const bool show_main_ui = (type() == TYPE_NORMAL) && !window_->IsFullscreen();
#else
  const bool show_main_ui = (type() == TYPE_NORMAL);
#endif

  command_updater_.UpdateCommandEnabled(IDC_SYNC_BOOKMARKS,
      show_main_ui && profile_->IsSyncAccessible());
}

///////////////////////////////////////////////////////////////////////////////
// Browser, InstantDelegate implementation:

void Browser::PrepareForInstant() {
  window_->PrepareForInstant();
}

void Browser::ShowInstant(TabContentsWrapper* preview_contents) {
  DCHECK(instant_->tab_contents() == GetSelectedTabContentsWrapper());
  window_->ShowInstant(preview_contents->tab_contents());
}

void Browser::HideInstant() {
  window_->HideInstant(instant_->is_active());
}

void Browser::CommitInstant(TabContentsWrapper* preview_contents) {
  TabContentsWrapper* tab_contents = instant_->tab_contents();
  int index =
      tab_handler_->GetTabStripModel()->GetIndexOfTabContents(tab_contents);
  DCHECK_NE(TabStripModel::kNoTab, index);
  preview_contents->controller().CopyStateFromAndPrune(
      &tab_contents->controller());
  // TabStripModel takes ownership of preview_contents.
  TabContentsWrapper* old_contents =
      tab_handler_->GetTabStripModel()->ReplaceTabContentsAt(
          index, preview_contents);
  // InstantUnloadHandler takes ownership of old_contents.
  instant_unload_handler_->RunUnloadListenersOrDestroy(old_contents, index);
}

void Browser::SetSuggestedText(const string16& text) {
  window()->GetLocationBar()->SetSuggestedText(text);
}

gfx::Rect Browser::GetInstantBounds() {
  return window()->GetInstantBounds();
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
  command_updater_.UpdateCommandEnabled(IDC_NEW_WINDOW, true);
  command_updater_.UpdateCommandEnabled(IDC_NEW_INCOGNITO_WINDOW,
                                        incognito_mode_allowed_.GetValue());
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_WINDOW, true);
  command_updater_.UpdateCommandEnabled(IDC_NEW_TAB, true);
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_TAB, true);
  command_updater_.UpdateCommandEnabled(IDC_DUPLICATE_TAB, true);
  command_updater_.UpdateCommandEnabled(IDC_RESTORE_TAB, false);
  command_updater_.UpdateCommandEnabled(IDC_EXIT, true);
  command_updater_.UpdateCommandEnabled(IDC_TOGGLE_VERTICAL_TABS, true);

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
  command_updater_.UpdateCommandEnabled(IDC_OPEN_FILE, true);
  command_updater_.UpdateCommandEnabled(IDC_CREATE_SHORTCUTS, false);
  UpdateCommandsForDevTools();
  command_updater_.UpdateCommandEnabled(IDC_TASK_MANAGER, true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_HISTORY, true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER,
                                        browser_defaults::bookmarks_enabled);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_DOWNLOADS, true);
  command_updater_.UpdateCommandEnabled(IDC_HELP_PAGE, true);
  command_updater_.UpdateCommandEnabled(IDC_IMPORT_SETTINGS, true);

#if defined(OS_CHROMEOS)
  command_updater_.UpdateCommandEnabled(IDC_SEARCH, true);
  command_updater_.UpdateCommandEnabled(IDC_SHOW_KEYBOARD_OVERLAY, true);
  command_updater_.UpdateCommandEnabled(IDC_SYSTEM_OPTIONS, true);
  command_updater_.UpdateCommandEnabled(IDC_INTERNET_OPTIONS, true);
#endif

  ExtensionService* extensions_service = profile()->GetExtensionService();
  bool enable_extensions =
      extensions_service && extensions_service->extensions_enabled();
  command_updater_.UpdateCommandEnabled(IDC_MANAGE_EXTENSIONS,
                                        enable_extensions);

  // Initialize other commands based on the window type.
  bool normal_window = type() == TYPE_NORMAL;
  bool non_devtools_window = type() != TYPE_DEVTOOLS;

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_HOME, normal_window);

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_FULLSCREEN,
      type() != TYPE_APP_PANEL);
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
#endif

  // Page-related commands
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARK_PAGE,
      browser_defaults::bookmarks_enabled && normal_window);

  // Clipboard commands
  command_updater_.UpdateCommandEnabled(IDC_COPY_URL, non_devtools_window);

  // Find-in-page
  command_updater_.UpdateCommandEnabled(IDC_FIND, non_devtools_window);
  command_updater_.UpdateCommandEnabled(IDC_FIND_NEXT, non_devtools_window);
  command_updater_.UpdateCommandEnabled(IDC_FIND_PREVIOUS, non_devtools_window);

  // AutoFill
  command_updater_.UpdateCommandEnabled(IDC_AUTOFILL_DEFAULT,
                                        non_devtools_window);

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

  // Initialize other commands whose state changes based on fullscreen mode.
  UpdateCommandsForFullscreenMode(false);

  UpdateCommandsForContentRestrictionState();
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
  bool non_app_window = !(type() & TYPE_APP);
  command_updater_.UpdateCommandEnabled(IDC_DUPLICATE_TAB,
      non_app_window && CanDuplicateContentsAt(selected_index()));
  command_updater_.UpdateCommandEnabled(IDC_SELECT_NEXT_TAB,
      non_app_window && tab_count() > 1);
  command_updater_.UpdateCommandEnabled(IDC_SELECT_PREVIOUS_TAB,
      non_app_window && tab_count() > 1);

  // Page-related commands
  window_->SetStarredState(current_tab_wrapper->is_starred());
  command_updater_.UpdateCommandEnabled(IDC_BOOKMARK_ALL_TABS,
      browser_defaults::bookmarks_enabled && CanBookmarkAllTabs());
  command_updater_.UpdateCommandEnabled(IDC_VIEW_SOURCE,
      current_tab->controller().CanViewSource());
  command_updater_.UpdateCommandEnabled(IDC_EMAIL_PAGE_LOCATION,
      current_tab->ShouldDisplayURL() && current_tab->GetURL().is_valid());

  // Changing the encoding is not possible on Chrome-internal webpages.
  // Instead of using GetURL here, we use url() (which is the "real" url of the
  // page) from the NavigationEntry because its reflects their origin rather
  // than the display one (returned by GetURL) which may be different (like
  // having "view-source:" on the front).
  NavigationEntry* active_entry = nc.GetActiveEntry();
  bool is_chrome_internal = (active_entry ?
      active_entry->url().SchemeIs(chrome::kChromeUIScheme) : false);
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
}

void Browser::UpdateCommandsForContentRestrictionState() {
  int restrictions = 0;
  TabContents* current_tab = GetSelectedTabContents();
  if (current_tab) {
    restrictions = current_tab->content_restrictions();
    NavigationEntry* active_entry = current_tab->controller().GetActiveEntry();
    // See comment in UpdateCommandsForTabState about why we call url().
    if (!SavePackage::IsSavableURL(active_entry ? active_entry->url() : GURL()))
      restrictions |= CONTENT_RESTRICTION_SAVE;
  }

  command_updater_.UpdateCommandEnabled(
      IDC_COPY, !(restrictions & CONTENT_RESTRICTION_COPY));
  command_updater_.UpdateCommandEnabled(
      IDC_CUT, !(restrictions & CONTENT_RESTRICTION_CUT));
  command_updater_.UpdateCommandEnabled(
      IDC_PASTE, !(restrictions & CONTENT_RESTRICTION_PASTE));
  command_updater_.UpdateCommandEnabled(
      IDC_SAVE_PAGE, !(restrictions & CONTENT_RESTRICTION_SAVE));
  UpdatePrintingState(restrictions);
}

void Browser::UpdatePrintingState(int content_restrictions) {
  bool enabled = true;
  if (content_restrictions & CONTENT_RESTRICTION_PRINT) {
    enabled = false;
  } else if (g_browser_process->local_state()) {
    enabled = printing_enabled_.GetValue();
  }
  command_updater_.UpdateCommandEnabled(IDC_PRINT, enabled);
}

void Browser::UpdateReloadStopState(bool is_loading, bool force) {
  window_->UpdateReloadStopState(is_loading, force);
  command_updater_.UpdateCommandEnabled(IDC_STOP, is_loading);
}

void Browser::UpdateCommandsForDevTools() {
  bool dev_tools_enabled = !dev_tools_disabled_.GetValue();
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_CONSOLE,
                                        dev_tools_enabled);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_INSPECT,
                                        dev_tools_enabled);
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

  if (changed_flags & TabContents::INVALIDATE_TITLE && !source->is_loading()) {
    // To correctly calculate whether the title changed while not loading
    // we need to process the update synchronously. This state only matters for
    // the TabStripModel, so we notify the TabStripModel now and notify others
    // asynchronously.
    tab_handler_->GetTabStripModel()->UpdateTabContentsStateAt(
        tab_handler_->GetTabStripModel()->GetIndexOfController(
            &source->controller()),
        TabStripModelObserver::TITLE_NOT_LOADING);
  }

  if (changed_flags & TabContents::INVALIDATE_BOOKMARK_BAR) {
    window()->ShelfVisibilityChanged();
    changed_flags &= ~TabContents::INVALIDATE_BOOKMARK_BAR;
  }

  // If the only updates were synchronously handled above, we're done.
  if (changed_flags == 0)
    return;

  // Save the dirty bits.
  scheduled_updates_[source] |= changed_flags;

  if (chrome_updater_factory_.empty()) {
    // No task currently scheduled, start another.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        chrome_updater_factory_.NewRunnableMethod(
            &Browser::ProcessPendingUIUpdates),
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

  chrome_updater_factory_.RevokeAll();

  for (UpdateMap::const_iterator i = scheduled_updates_.begin();
       i != scheduled_updates_.end(); ++i) {
    // Do not dereference |contents|, it may be out-of-date!
    const TabContents* contents = i->first;
    unsigned flags = i->second;

    if (contents == GetSelectedTabContents()) {
      // Updates that only matter when the tab is selected go here.

      if (flags & TabContents::INVALIDATE_PAGE_ACTIONS)
        window()->GetLocationBar()->UpdatePageActions();

      // Updating the URL happens synchronously in ScheduleUIUpdate.
      if (flags & TabContents::INVALIDATE_LOAD && GetStatusBubble())
        GetStatusBubble()->SetStatus(contents->GetStatusText());

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
  if (!profile()->HasSessionService())
    return;
  SessionService* session_service = profile()->GetSessionService();
  if (session_service) {
    for (int i = index; i < tab_count(); ++i) {
      TabContents* contents = GetTabContentsAt(i);
      if (contents) {
        session_service->SetTabIndexInWindow(
            session_id(), contents->controller().session_id(), i);
        session_service->SetPinnedState(
            session_id(),
            contents->controller().session_id(),
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
      tab->render_view_host()->ClosePage(false, -1, -1);
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
          method_factory_.NewRunnableMethod(&Browser::ProcessPendingTabs));
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Browser, In-progress download termination handling (private):

void Browser::CheckDownloadsInProgress(bool* normal_downloads_are_present,
                                       bool* incognito_downloads_are_present) {
  *normal_downloads_are_present = false;
  *incognito_downloads_are_present = false;

  // If there are no download in-progress, our job is done.
  DownloadManager* download_manager = NULL;
  // But first we need to check for the existance of the download manager, as
  // GetDownloadManager() will unnecessarily try to create one if it does not
  // exist.
  if (profile()->HasCreatedDownloadManager())
    download_manager = profile()->GetDownloadManager();
  if (profile()->IsOffTheRecord()) {
    // Browser is incognito and so download_manager if present is for incognito
    // downloads.
    *incognito_downloads_are_present =
        (download_manager && download_manager->in_progress_count() != 0);
    // Check original profile.
    if (profile()->GetOriginalProfile()->HasCreatedDownloadManager())
      download_manager = profile()->GetOriginalProfile()->GetDownloadManager();
  }

  *normal_downloads_are_present =
      (download_manager && download_manager->in_progress_count() != 0);
}

bool Browser::CanCloseWithInProgressDownloads() {
  if (cancel_download_confirmation_state_ != NOT_PROMPTED) {
    if (cancel_download_confirmation_state_ == WAITING_FOR_RESPONSE) {
      // We need to hear from the user before we can close.
      return false;
    }
    // RESPONSE_RECEIVED case, the user decided to go along with the closing.
    return true;
  }
  // Indicated that normal (non-incognito) downloads are pending.
  bool normal_downloads_are_present = false;
  bool incognito_downloads_are_present = false;
  CheckDownloadsInProgress(&normal_downloads_are_present,
                      &incognito_downloads_are_present);
  if (!normal_downloads_are_present && !incognito_downloads_are_present)
    return true;

  if (is_attempting_to_close_browser_)
    return true;

  if ((!normal_downloads_are_present && !profile()->IsOffTheRecord()) ||
      (!incognito_downloads_are_present && profile()->IsOffTheRecord()))
    return true;

  // Let's figure out if we are the last window for our profile.
  // Note that we cannot just use BrowserList::GetBrowserCount as browser
  // windows closing is delayed and the returned count might include windows
  // that are being closed.
  // The browser allowed to be closed only if:
  // 1. It is a regular browser and there are no regular downloads present or
  //    this is not the last regular browser window.
  // 2. It is an incognito browser and there are no incognito downloads present
  //    or this is not the last incognito browser window.
  int count = 0;
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    // Don't count this browser window or any other in the process of closing.
    // Only consider normal browser windows, not popups.
    Browser* const browser = *iter;
    if (browser == this
        || browser->is_attempting_to_close_browser_
        || browser->type() != Browser::TYPE_NORMAL)
      continue;

    // Verify that this is not the last non-incognito or incognito browser,
    // depending on the pending downloads.
    if (normal_downloads_are_present && !profile()->IsOffTheRecord() &&
        browser->profile()->IsOffTheRecord())
      continue;
    if (incognito_downloads_are_present && profile()->IsOffTheRecord() &&
        !browser->profile()->IsOffTheRecord())
      continue;

    // We test the original profile, because an incognito browser window keeps
    // the original profile alive (and its DownloadManager).
    // We also need to test explicitly the profile directly so that 2 incognito
    // profiles count as a match.
    if ((*iter)->profile() == profile() ||
        (*iter)->profile()->GetOriginalProfile() == profile())
      count++;
  }
  if (count > 0)
    return true;

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
  return BrowserList::FindBrowserWithType(profile, TYPE_NORMAL,
                                          match_incognito);
}

// static
Browser* Browser::GetOrCreateTabbedBrowser(Profile* profile) {
  Browser* browser = GetTabbedBrowser(profile, false);
  if (!browser)
    browser = Browser::Create(profile);
  return browser;
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
        GetFindManager()->StartFinding(find_text,
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
    if (contents == GetSelectedTabContentsWrapper())
      window_->GetLocationBar()->SaveStateToContents(contents->tab_contents());

    if (!tab_handler_->GetTabStripModel()->closing_all())
      SyncHistoryWithTabs(0);
  }

  contents->set_delegate(NULL);
  contents->tab_contents()->set_delegate(NULL);
  RemoveScheduledUpdatesFor(contents->tab_contents());

  if (find_bar_controller_.get() &&
      index == tab_handler_->GetTabStripModel()->selected_index()) {
    find_bar_controller_->ChangeTabContents(NULL);
  }

  if (is_attempting_to_close_browser_) {
    // If this is the last tab with unload handlers, then ProcessPendingTabs
    // would call back into the TabStripModel (which is invoking this method on
    // us). Avoid that by passing in false so that the call to
    // ProcessPendingTabs is delayed.
    ClearUnloadState(contents->tab_contents(), false);
  }

  registrar_.Remove(this, NotificationType::TAB_CONTENTS_DISCONNECTED,
                    Source<TabContentsWrapper>(contents));
}

// static
void Browser::RegisterAppPrefs(const std::string& app_name, Profile* profile) {
  // A set of apps that we've already started.
  static std::set<std::string>* g_app_names = NULL;

  if (!g_app_names)
    g_app_names = new std::set<std::string>;

  // Only register once for each app name.
  if (g_app_names->find(app_name) != g_app_names->end())
    return;
  g_app_names->insert(app_name);

  // We need to register the window position pref.
  std::string window_pref(prefs::kBrowserWindowPlacement);
  window_pref.append("_");
  window_pref.append(app_name);
  profile->GetPrefs()->RegisterDictionaryPref(window_pref.c_str());
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
  if (!instant() || !instant()->is_active() || !instant()->IsCurrent() ||
      disposition == NEW_BACKGROUND_TAB) {
    // NEW_BACKGROUND_TAB results in leaving the omnibox open, so we don't
    // attempt to use the instant preview.
    return false;
  }

  if (disposition == CURRENT_TAB) {
    instant()->CommitCurrentPreview(INSTANT_COMMIT_PRESSED_ENTER);
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
        disposition == NEW_FOREGROUND_TAB ? TabStripModel::ADD_SELECTED :
                                            TabStripModel::ADD_NONE);
    instant()->CompleteRelease(preview_contents->tab_contents());
    return true;
  }
  // The omnibox currently doesn't use other dispositions, so we don't attempt
  // to handle them. If you hit this NOTREACHED file a bug and I'll (sky) add
  // support for the new disposition.
  NOTREACHED();
  return false;
}

void Browser::CreateInstantIfNecessary() {
  if (type() == TYPE_NORMAL && InstantController::IsEnabled(profile()) &&
      !profile()->IsOffTheRecord()) {
    instant_.reset(new InstantController(profile_, this));
    instant_unload_handler_.reset(new InstantUnloadHandler(this));
  }
}

void Browser::ViewSource(TabContentsWrapper* contents) {
  UserMetrics::RecordAction(UserMetricsAction("ViewSource"), profile_);
  DCHECK(contents);

  TabContentsWrapper* view_source_contents = contents->Clone();
  view_source_contents->controller().PruneAllButActive();
  NavigationEntry* active_entry =
      view_source_contents->controller().GetActiveEntry();
  if (!active_entry)
    return;

  // Do not restore scroller position.
  std::string content_state = webkit_glue::RemoveScrollOffsetFromHistoryState(
      active_entry->content_state());
  active_entry->set_content_state(content_state);

  GURL url = GURL(chrome::kViewSourceScheme + std::string(":") +
      active_entry->url().spec());
  active_entry->set_virtual_url(url);
  // Do not restore title, derive it from the url.
  active_entry->set_title(string16());

  // Now show view-source entry.

  if (CanSupportWindowFeature(FEATURE_TABSTRIP)) {
    // If this is a tabbed browser, just create a duplicate tab inside the same
    // window next to the tab being duplicated.
    int index = tab_handler_->GetTabStripModel()->
        GetIndexOfTabContents(contents);
    int add_types = TabStripModel::ADD_SELECTED |
        TabStripModel::ADD_INHERIT_GROUP;
    tab_handler_->GetTabStripModel()->InsertTabContentsAt(index + 1,
                                                          view_source_contents,
                                                          add_types);
  } else {
    Browser* browser = Browser::CreateForType(TYPE_NORMAL, profile_);

    // Preserve the size of the original window. The new window has already
    // been given an offset by the OS, so we shouldn't copy the old bounds.
    BrowserWindow* new_window = browser->window();
    new_window->SetBounds(gfx::Rect(new_window->GetRestoredBounds().origin(),
                          window()->GetRestoredBounds().size()));

    // We need to show the browser now. Otherwise ContainerWin assumes the
    // TabContents is invisible and won't size it.
    browser->window()->Show();

    // The page transition below is only for the purpose of inserting the tab.
    browser->AddTab(view_source_contents, PageTransition::LINK);
  }

  if (profile_->HasSessionService()) {
    SessionService* session_service = profile_->GetSessionService();
    if (session_service)
      session_service->TabRestored(&view_source_contents->controller(), false);
  }
}
