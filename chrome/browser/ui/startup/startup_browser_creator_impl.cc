// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include "apps/app_restore_service.h"
#include "apps/app_restore_service_factory.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/install_chrome_app.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/session_crashed_bubble.h"
#include "chrome/browser/ui/startup/bad_flags_prompt.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#include "chrome/browser/ui/startup/google_api_keys_infobar_delegate.h"
#include "chrome/browser/ui/startup/obsolete_system_infobar_delegate.h"
#include "chrome/browser/ui/startup/session_crashed_infobar_delegate.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/installer/util/browser_distribution.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "chrome/browser/ui/cocoa/keystone_infobar_delegate.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/apps/app_launch_for_metro_restart_win.h"
#endif

#if defined(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"
#endif

using content::ChildProcessSecurityPolicy;
using content::WebContents;
using extensions::Extension;

namespace {

// Utility functions ----------------------------------------------------------

enum LaunchMode {
  LM_TO_BE_DECIDED = 0,       // Possibly direct launch or via a shortcut.
  LM_AS_WEBAPP,               // Launched as a installed web application.
  LM_WITH_URLS,               // Launched with urls in the cmd line.
  LM_SHORTCUT_NONE,           // Not launched from a shortcut.
  LM_SHORTCUT_NONAME,         // Launched from shortcut but no name available.
  LM_SHORTCUT_UNKNOWN,        // Launched from user-defined shortcut.
  LM_SHORTCUT_QUICKLAUNCH,    // Launched from the quick launch bar.
  LM_SHORTCUT_DESKTOP,        // Launched from a desktop shortcut.
  LM_SHORTCUT_TASKBAR,        // Launched from the taskbar.
  LM_LINUX_MAC_BEOS           // Other OS buckets start here.
};

#if defined(OS_WIN)
// Undocumented flag in the startup info structure tells us what shortcut was
// used to launch the browser. See http://www.catch22.net/tuts/undoc01 for
// more information. Confirmed to work on XP, Vista and Win7.
LaunchMode GetLaunchShortcutKind() {
  STARTUPINFOW si = { sizeof(si) };
  GetStartupInfoW(&si);
  if (si.dwFlags & 0x800) {
    if (!si.lpTitle)
      return LM_SHORTCUT_NONAME;
    base::string16 shortcut(si.lpTitle);
    // The windows quick launch path is not localized.
    if (shortcut.find(L"\\Quick Launch\\") != base::string16::npos) {
      if (base::win::GetVersion() >= base::win::VERSION_WIN7)
        return LM_SHORTCUT_TASKBAR;
      else
        return LM_SHORTCUT_QUICKLAUNCH;
    }
    scoped_ptr<base::Environment> env(base::Environment::Create());
    std::string appdata_path;
    env->GetVar("USERPROFILE", &appdata_path);
    if (!appdata_path.empty() &&
        shortcut.find(base::ASCIIToUTF16(appdata_path)) != base::string16::npos)
      return LM_SHORTCUT_DESKTOP;
    return LM_SHORTCUT_UNKNOWN;
  }
  return LM_SHORTCUT_NONE;
}
#else
// TODO(cpu): Port to other platforms.
LaunchMode GetLaunchShortcutKind() {
  return LM_LINUX_MAC_BEOS;
}
#endif

// Log in a histogram the frequency of launching by the different methods. See
// LaunchMode enum for the actual values of the buckets.
void RecordLaunchModeHistogram(LaunchMode mode) {
  int bucket = (mode == LM_TO_BE_DECIDED) ? GetLaunchShortcutKind() : mode;
  UMA_HISTOGRAM_COUNTS_100("Launch.Modes", bucket);
}

void UrlsToTabs(const std::vector<GURL>& urls, StartupTabs* tabs) {
  for (size_t i = 0; i < urls.size(); ++i) {
    StartupTab tab;
    tab.is_pinned = false;
    tab.url = urls[i];
    tabs->push_back(tab);
  }
}

// Return true if the command line option --app-id is used.  Set
// |out_extension| to the app to open, and |out_launch_container|
// to the type of window into which the app should be open.
bool GetAppLaunchContainer(
    Profile* profile,
    const std::string& app_id,
    const Extension** out_extension,
    extensions::LaunchContainer* out_launch_container) {

  const Extension* extension = extensions::ExtensionRegistry::Get(
      profile)->enabled_extensions().GetByID(app_id);
  // The extension with id |app_id| may have been uninstalled.
  if (!extension)
    return false;

  // Don't launch platform apps in incognito mode.
  if (profile->IsOffTheRecord() && extension->is_platform_app())
    return false;

  // Look at preferences to find the right launch container. If no
  // preference is set, launch as a window.
  extensions::LaunchContainer launch_container = extensions::GetLaunchContainer(
      extensions::ExtensionPrefs::Get(profile), extension);

  if (!extensions::util::IsNewBookmarkAppsEnabled() &&
      !extensions::HasPreferredLaunchContainer(
          extensions::ExtensionPrefs::Get(profile), extension)) {
    launch_container = extensions::LAUNCH_CONTAINER_WINDOW;
  }

  *out_extension = extension;
  *out_launch_container = launch_container;
  return true;
}

void RecordCmdLineAppHistogram(extensions::Manifest::Type app_type) {
  extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_CMD_LINE_APP,
                                  app_type);
}

void RecordAppLaunches(Profile* profile,
                       const std::vector<GURL>& cmd_line_urls,
                       const StartupTabs& autolaunch_tabs) {
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions();
  for (size_t i = 0; i < cmd_line_urls.size(); ++i) {
    const extensions::Extension* extension =
        extensions.GetAppByURL(cmd_line_urls.at(i));
    if (extension) {
      extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_CMD_LINE_URL,
                                      extension->GetType());
    }
  }
  for (size_t i = 0; i < autolaunch_tabs.size(); ++i) {
    const extensions::Extension* extension =
        extensions.GetAppByURL(autolaunch_tabs.at(i).url);
    if (extension) {
      extensions::RecordAppLaunchType(extension_misc::APP_LAUNCH_AUTOLAUNCH,
                                      extension->GetType());
    }
  }
}

// TODO(koz): Consolidate this function and remove the special casing.
const Extension* GetPlatformApp(Profile* profile,
                                const std::string& extension_id) {
  const Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::EVERYTHING);
  return extension && extension->is_platform_app() ? extension : NULL;
}

}  // namespace

namespace internals {

GURL GetTriggeredResetSettingsURL() {
  return GURL(
      chrome::GetSettingsUrl(chrome::kTriggeredResetProfileSettingsSubPage));
}

GURL GetWelcomePageURL() {
  return GURL(l10n_util::GetStringUTF8(IDS_WELCOME_PAGE_URL));
}

}  // namespace internals

StartupBrowserCreatorImpl::StartupBrowserCreatorImpl(
    const base::FilePath& cur_dir,
    const base::CommandLine& command_line,
    chrome::startup::IsFirstRun is_first_run)
    : cur_dir_(cur_dir),
      command_line_(command_line),
      profile_(NULL),
      browser_creator_(NULL),
      is_first_run_(is_first_run == chrome::startup::IS_FIRST_RUN),
      welcome_run_type_(WelcomeRunType::NONE) {
}

StartupBrowserCreatorImpl::StartupBrowserCreatorImpl(
    const base::FilePath& cur_dir,
    const base::CommandLine& command_line,
    StartupBrowserCreator* browser_creator,
    chrome::startup::IsFirstRun is_first_run)
    : cur_dir_(cur_dir),
      command_line_(command_line),
      profile_(NULL),
      browser_creator_(browser_creator),
      is_first_run_(is_first_run == chrome::startup::IS_FIRST_RUN),
      welcome_run_type_(WelcomeRunType::NONE) {
}

StartupBrowserCreatorImpl::~StartupBrowserCreatorImpl() {
}

bool StartupBrowserCreatorImpl::Launch(Profile* profile,
                                       const std::vector<GURL>& urls_to_open,
                                       bool process_startup,
                                       chrome::HostDesktopType desktop_type) {
  UMA_HISTOGRAM_COUNTS_100("Startup.BrowserLaunchURLCount",
      static_cast<base::HistogramBase::Sample>(urls_to_open.size()));
  RecordRapporOnStartupURLs(urls_to_open);

  DCHECK(profile);
  profile_ = profile;

  if (AppListService::HandleLaunchCommandLine(command_line_, profile))
    return true;

  if (command_line_.HasSwitch(switches::kAppId)) {
    std::string app_id = command_line_.GetSwitchValueASCII(switches::kAppId);
    const Extension* extension = GetPlatformApp(profile, app_id);
    // If |app_id| is a disabled or terminated platform app we handle it
    // specially here, otherwise it will be handled below.
    if (extension) {
      RecordCmdLineAppHistogram(extensions::Manifest::TYPE_PLATFORM_APP);
      AppLaunchParams params(profile, extension,
                             extensions::LAUNCH_CONTAINER_NONE, NEW_WINDOW,
                             extensions::SOURCE_COMMAND_LINE);
      params.command_line = command_line_;
      params.current_directory = cur_dir_;
      // If we are being launched from the command line, default to native
      // desktop.
      params.desktop_type = chrome::HOST_DESKTOP_TYPE_NATIVE;
      ::OpenApplicationWithReenablePrompt(params);
      return true;
    }
  }

  // Open the required browser windows and tabs. First, see if
  // we're being run as an application window. If so, the user
  // opened an app shortcut.  Don't restore tabs or open initial
  // URLs in that case. The user should see the window as an app,
  // not as chrome.
  // Special case is when app switches are passed but we do want to restore
  // session. In that case open app window + focus it after session is restored.
  if (OpenApplicationWindow(profile)) {
    RecordLaunchModeHistogram(LM_AS_WEBAPP);
  } else {
    RecordLaunchModeHistogram(urls_to_open.empty() ?
                              LM_TO_BE_DECIDED : LM_WITH_URLS);

    ProcessLaunchURLs(process_startup, urls_to_open, desktop_type);

    if (command_line_.HasSwitch(switches::kInstallChromeApp)) {
      install_chrome_app::InstallChromeApp(
          command_line_.GetSwitchValueASCII(switches::kInstallChromeApp));
    }

    // If this is an app launch, but we didn't open an app window, it may
    // be an app tab.
    OpenApplicationTab(profile);

#if defined(OS_MACOSX)
    if (process_startup) {
      // Check whether the auto-update system needs to be promoted from user
      // to system.
      KeystoneInfoBar::PromotionInfoBar(profile);
    }
#endif
  }

  // In kiosk mode, we want to always be fullscreen, so switch to that now.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStartFullscreen)) {
    // It's possible for there to be no browser window, e.g. if someone
    // specified a non-sensical combination of options
    // ("--kiosk --no_startup_window"); do nothing in that case.
    Browser* browser = BrowserList::GetInstance()->GetLastActive();
    if (browser)
      chrome::ToggleFullscreenMode(browser);
  }

#if defined(OS_WIN)
  // TODO(gab): This could now run only during Active Setup (i.e. on explicit
  // Active Setup versioning and on OS upgrades) instead of every startup.
  // http://crbug.com/577697
  if (process_startup)
    ShellIntegration::MigrateTaskbarPins();
#endif  // defined(OS_WIN)

  return true;
}

bool StartupBrowserCreatorImpl::IsAppLaunch(std::string* app_url,
                                            std::string* app_id) {
  if (command_line_.HasSwitch(switches::kApp)) {
    if (app_url)
      *app_url = command_line_.GetSwitchValueASCII(switches::kApp);
    return true;
  }
  if (command_line_.HasSwitch(switches::kAppId)) {
    if (app_id)
      *app_id = command_line_.GetSwitchValueASCII(switches::kAppId);
    return true;
  }
  return false;
}

bool StartupBrowserCreatorImpl::OpenApplicationTab(Profile* profile) {
  std::string app_id;
  // App shortcuts to URLs always open in an app window.  Because this
  // function will open an app that should be in a tab, there is no need
  // to look at the app URL.  OpenApplicationWindow() will open app url
  // shortcuts.
  if (!IsAppLaunch(NULL, &app_id) || app_id.empty())
    return false;

  extensions::LaunchContainer launch_container;
  const Extension* extension;
  if (!GetAppLaunchContainer(profile, app_id, &extension, &launch_container))
    return false;

  // If the user doesn't want to open a tab, fail.
  if (launch_container != extensions::LAUNCH_CONTAINER_TAB)
    return false;

  RecordCmdLineAppHistogram(extension->GetType());

  WebContents* app_tab = ::OpenApplication(
      AppLaunchParams(profile, extension, extensions::LAUNCH_CONTAINER_TAB,
                      NEW_FOREGROUND_TAB, extensions::SOURCE_COMMAND_LINE));
  return (app_tab != NULL);
}

bool StartupBrowserCreatorImpl::OpenApplicationWindow(Profile* profile) {
  std::string url_string, app_id;
  if (!IsAppLaunch(&url_string, &app_id))
    return false;

  // This can fail if the app_id is invalid.  It can also fail if the
  // extension is external, and has not yet been installed.
  // TODO(skerner): Do something reasonable here. Pop up a warning panel?
  // Open an URL to the gallery page of the extension id?
  if (!app_id.empty()) {
    extensions::LaunchContainer launch_container;
    const Extension* extension;
    if (!GetAppLaunchContainer(profile, app_id, &extension, &launch_container))
      return false;

    // TODO(skerner): Could pass in |extension| and |launch_container|,
    // and avoid calling GetAppLaunchContainer() both here and in
    // OpenApplicationTab().

    if (launch_container == extensions::LAUNCH_CONTAINER_TAB)
      return false;

    RecordCmdLineAppHistogram(extension->GetType());

    AppLaunchParams params(profile, extension, launch_container, NEW_WINDOW,
                           extensions::SOURCE_COMMAND_LINE);
    params.command_line = command_line_;
    params.current_directory = cur_dir_;
    WebContents* tab_in_app_window = ::OpenApplication(params);

    // Platform apps fire off a launch event which may or may not open a window.
    return (tab_in_app_window != NULL || CanLaunchViaEvent(extension));
  }

  if (url_string.empty())
    return false;

#if defined(OS_WIN)  // Fix up Windows shortcuts.
  base::ReplaceSubstringsAfterOffset(&url_string, 0, "\\x", "%");
#endif
  GURL url(url_string);

  // Restrict allowed URLs for --app switch.
  if (!url.is_empty() && url.is_valid()) {
    ChildProcessSecurityPolicy* policy =
        ChildProcessSecurityPolicy::GetInstance();
    if (policy->IsWebSafeScheme(url.scheme()) ||
        url.SchemeIs(url::kFileScheme)) {
      const extensions::Extension* extension =
          extensions::ExtensionRegistry::Get(profile)
              ->enabled_extensions().GetAppByURL(url);
      if (extension) {
        RecordCmdLineAppHistogram(extension->GetType());
      } else {
        extensions::RecordAppLaunchType(
            extension_misc::APP_LAUNCH_CMD_LINE_APP_LEGACY,
            extensions::Manifest::TYPE_HOSTED_APP);
      }

      WebContents* app_tab = ::OpenAppShortcutWindow(profile, url);
      return (app_tab != NULL);
    }
  }
  return false;
}

void StartupBrowserCreatorImpl::ProcessLaunchURLs(
    bool process_startup,
    const std::vector<GURL>& urls_to_open,
    chrome::HostDesktopType desktop_type) {
  // Don't open any browser windows if we're starting up in "background mode".
  if (process_startup && command_line_.HasSwitch(switches::kNoStartupWindow))
    return;

  // Determine whether or not this launch must include the welcome page.
  InitializeWelcomeRunType(urls_to_open);

// TODO(tapted): Move this to startup_browser_creator_win.cc after refactor.
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    // See if there are apps for this profile that should be launched on startup
    // due to a switch from Metro mode.
    app_metro_launch::HandleAppLaunchForMetroRestart(profile_);
  }
#endif

  if (process_startup && ProcessStartupURLs(urls_to_open, desktop_type)) {
    // ProcessStartupURLs processed the urls, nothing else to do.
    return;
  }

  chrome::startup::IsProcessStartup is_process_startup = process_startup ?
      chrome::startup::IS_PROCESS_STARTUP :
      chrome::startup::IS_NOT_PROCESS_STARTUP;
  if (!process_startup) {
    // Even if we're not starting a new process, this may conceptually be
    // "startup" for the user and so should be handled in a similar way.  Eg.,
    // Chrome may have been running in the background due to an app with a
    // background page being installed, or running with only an app window
    // displayed.
    SessionService* service =
        SessionServiceFactory::GetForProfileForSessionRestore(profile_);
    if (service && service->ShouldNewWindowStartSession()) {
      // Restore the last session if any, optionally including the welcome page
      // if desired.
      if (!HasPendingUncleanExit(profile_)) {
        std::vector<GURL> adjusted_urls(urls_to_open);
        AddSpecialURLs(&adjusted_urls);
        if (service->RestoreIfNecessary(adjusted_urls))
          return;
      }

      // Open user-specified URLs like pinned tabs and startup tabs.
      Browser* browser = ProcessSpecifiedURLs(urls_to_open, desktop_type);
      if (browser) {
        AddInfoBarsIfNecessary(browser, is_process_startup);
        return;
      }
    }
  }

  // Session startup didn't occur, open the urls.
  Browser* browser = NULL;
  std::vector<GURL> adjusted_urls = urls_to_open;
  if (adjusted_urls.empty()) {
    AddStartupURLs(&adjusted_urls);
  } else if (!command_line_.HasSwitch(switches::kOpenInNewWindow)) {
    // Always open a list of urls in a window on the native desktop.
    browser = chrome::FindTabbedBrowser(profile_, false,
                                        chrome::HOST_DESKTOP_TYPE_NATIVE);
  }
  // This will launch a browser; prevent session restore.
  StartupBrowserCreator::in_synchronous_profile_launch_ = true;
  browser =
      OpenURLsInBrowser(browser, process_startup, adjusted_urls, desktop_type);
  StartupBrowserCreator::in_synchronous_profile_launch_ = false;
  AddInfoBarsIfNecessary(browser, is_process_startup);
}

bool StartupBrowserCreatorImpl::ProcessStartupURLs(
    const std::vector<GURL>& urls_to_open,
    chrome::HostDesktopType desktop_type) {
  VLOG(1) << "StartupBrowserCreatorImpl::ProcessStartupURLs";
  SessionStartupPref pref =
      StartupBrowserCreator::GetSessionStartupPref(command_line_, profile_);
  if (pref.type == SessionStartupPref::LAST)
    VLOG(1) << "Pref: last";
  else if (pref.type == SessionStartupPref::URLS)
    VLOG(1) << "Pref: urls";
  else if (pref.type == SessionStartupPref::DEFAULT)
    VLOG(1) << "Pref: default";

  apps::AppRestoreService* restore_service =
      apps::AppRestoreServiceFactory::GetForProfile(profile_);
  // NULL in incognito mode.
  if (restore_service) {
    restore_service->HandleStartup(apps::AppRestoreService::ShouldRestoreApps(
        StartupBrowserCreator::WasRestarted()));
  }

  if (pref.type == SessionStartupPref::LAST) {
    if (profile_->GetLastSessionExitType() == Profile::EXIT_CRASHED &&
        !command_line_.HasSwitch(switches::kRestoreLastSession)) {
      // The last session crashed. It's possible automatically loading the
      // page will trigger another crash, locking the user out of chrome.
      // To avoid this, don't restore on startup but instead show the crashed
      // infobar.
      VLOG(1) << "Unclean exit; not processing";
      return false;
    }

    uint32_t restore_behavior = SessionRestore::SYNCHRONOUS;
    if (browser_defaults::kAlwaysCreateTabbedBrowserOnSessionRestore ||
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kCreateBrowserOnStartupForTests)) {
      restore_behavior |= SessionRestore::ALWAYS_CREATE_TABBED_BROWSER;
    }

#if defined(OS_MACOSX)
    // On Mac, when restoring a session with no windows, suppress the creation
    // of a new window in the case where the system is launching Chrome via a
    // login item or Lion's resume feature.
    if (base::mac::WasLaunchedAsLoginOrResumeItem()) {
      restore_behavior = restore_behavior &
                         ~SessionRestore::ALWAYS_CREATE_TABBED_BROWSER;
    }
#endif

    std::vector<GURL> adjusted_urls(urls_to_open);
    AddSpecialURLs(&adjusted_urls);

    // The startup code only executes for browsers launched in desktop mode.
    // i.e. HOST_DESKTOP_TYPE_NATIVE. Ash should never get here.
    Browser* browser = SessionRestore::RestoreSession(
        profile_, NULL, desktop_type, restore_behavior, adjusted_urls);

    AddInfoBarsIfNecessary(browser, chrome::startup::IS_PROCESS_STARTUP);
    return true;
  }

  Browser* browser = ProcessSpecifiedURLs(urls_to_open, desktop_type);
  if (!browser)
    return false;

  AddInfoBarsIfNecessary(browser, chrome::startup::IS_PROCESS_STARTUP);

  // Session restore may occur if the startup preference is "last" or if the
  // crash infobar is displayed. Otherwise, it's safe for the DOM storage system
  // to start deleting leftover data.
  if (pref.type != SessionStartupPref::LAST &&
      !HasPendingUncleanExit(profile_)) {
    content::BrowserContext::GetDefaultStoragePartition(profile_)->
        GetDOMStorageContext()->StartScavengingUnusedSessionStorage();
  }

  return true;
}

Browser* StartupBrowserCreatorImpl::ProcessSpecifiedURLs(
    const std::vector<GURL>& urls_to_open,
    chrome::HostDesktopType desktop_type) {
  SessionStartupPref pref =
      StartupBrowserCreator::GetSessionStartupPref(command_line_, profile_);
  StartupTabs tabs;
  // Pinned tabs should not be displayed when chrome is launched in incognito
  // mode. Also, no pages should be opened automatically if the session
  // crashed. Otherwise it might trigger another crash, locking the user out of
  // chrome. The crash infobar is shown in this case.
  if (!IncognitoModePrefs::ShouldLaunchIncognito(command_line_,
                                                 profile_->GetPrefs()) &&
      !HasPendingUncleanExit(profile_)) {
    tabs = PinnedTabCodec::ReadPinnedTabs(profile_);
  }

  RecordAppLaunches(profile_, urls_to_open, tabs);

  if (!urls_to_open.empty()) {
    // If urls were specified on the command line, use them.
    UrlsToTabs(urls_to_open, &tabs);
  } else if (pref.type == SessionStartupPref::DEFAULT ||
             (is_first_run_ &&
              browser_creator_ && !browser_creator_->first_run_tabs_.empty())) {
    std::vector<GURL> urls;
    AddStartupURLs(&urls);
    UrlsToTabs(urls, &tabs);
  } else if (pref.type == SessionStartupPref::URLS && !pref.urls.empty() &&
             !HasPendingUncleanExit(profile_)) {
    std::vector<GURL> extra_urls;
    AddSpecialURLs(&extra_urls);
    UrlsToTabs(extra_urls, &tabs);

    // Only use the set of urls specified in preferences if nothing was
    // specified on the command line. Filter out any urls that are to be
    // restored by virtue of having been previously pinned.
    AddUniqueURLs(pref.urls, &tabs);
  }

  if (tabs.empty())
    return NULL;

  Browser* browser = OpenTabsInBrowser(NULL, true, tabs, desktop_type);
  return browser;
}

void StartupBrowserCreatorImpl::AddUniqueURLs(const std::vector<GURL>& urls,
                                              StartupTabs* tabs) {
  size_t num_existing_tabs = tabs->size();
  for (size_t i = 0; i < urls.size(); ++i) {
    bool in_tabs = false;
    for (size_t j = 0; j < num_existing_tabs; ++j) {
      if (urls[i] == (*tabs)[j].url) {
        in_tabs = true;
        break;
      }
    }
    if (!in_tabs) {
      StartupTab tab;
      tab.is_pinned = false;
      tab.url = urls[i];
      tabs->push_back(tab);
    }
  }
}

Browser* StartupBrowserCreatorImpl::OpenURLsInBrowser(
    Browser* browser,
    bool process_startup,
    const std::vector<GURL>& urls,
    chrome::HostDesktopType desktop_type) {
  StartupTabs tabs;
  UrlsToTabs(urls, &tabs);
  return OpenTabsInBrowser(browser, process_startup, tabs, desktop_type);
}

Browser* StartupBrowserCreatorImpl::OpenTabsInBrowser(
    Browser* browser,
    bool process_startup,
    const StartupTabs& tabs,
    chrome::HostDesktopType desktop_type) {
  DCHECK(!tabs.empty());

  // If we don't yet have a profile, try to use the one we're given from
  // |browser|. While we may not end up actually using |browser| (since it
  // could be a popup window), we can at least use the profile.
  if (!profile_ && browser)
    profile_ = browser->profile();

  if (!browser || !browser->is_type_tabbed())
    browser = new Browser(Browser::CreateParams(profile_, desktop_type));

  bool first_tab = true;
  ProtocolHandlerRegistry* registry = profile_ ?
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile_) : NULL;
  for (size_t i = 0; i < tabs.size(); ++i) {
    // We skip URLs that we'd have to launch an external protocol handler for.
    // This avoids us getting into an infinite loop asking ourselves to open
    // a URL, should the handler be (incorrectly) configured to be us. Anyone
    // asking us to open such a URL should really ask the handler directly.
    bool handled_by_chrome = ProfileIOData::IsHandledURL(tabs[i].url) ||
        (registry && registry->IsHandledProtocol(tabs[i].url.scheme()));
    if (!process_startup && !handled_by_chrome)
      continue;

    int add_types = first_tab ? TabStripModel::ADD_ACTIVE :
                                TabStripModel::ADD_NONE;
    add_types |= TabStripModel::ADD_FORCE_INDEX;
    if (tabs[i].is_pinned)
      add_types |= TabStripModel::ADD_PINNED;

    chrome::NavigateParams params(browser, tabs[i].url,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = first_tab ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
    params.tabstrip_add_types = add_types;

#if defined(ENABLE_RLZ) && !defined(OS_IOS)
    if (process_startup && google_util::IsGoogleHomePageUrl(tabs[i].url)) {
      params.extra_headers = rlz::RLZTracker::GetAccessPointHttpHeader(
          rlz::RLZTracker::ChromeHomePage());
    }
#endif  // defined(ENABLE_RLZ) && !defined(OS_IOS)

    chrome::Navigate(&params);

    first_tab = false;
  }
  if (!browser->tab_strip_model()->GetActiveWebContents()) {
    // TODO(sky): this is a work around for 110909. Figure out why it's needed.
    if (!browser->tab_strip_model()->count())
      chrome::AddTabAt(browser, GURL(), -1, true);
    else
      browser->tab_strip_model()->ActivateTabAt(0, false);
  }

  // The default behavior is to show the window, as expressed by the default
  // value of StartupBrowserCreated::show_main_browser_window_. If this was set
  // to true ahead of this place, it means another task must have been spawned
  // to take care of that.
  if (!browser_creator_ || browser_creator_->show_main_browser_window())
    browser->window()->Show();

  return browser;
}

void StartupBrowserCreatorImpl::AddInfoBarsIfNecessary(
    Browser* browser,
    chrome::startup::IsProcessStartup is_process_startup) {
  if (!browser || !profile_ || browser->tab_strip_model()->count() == 0)
    return;

  if (HasPendingUncleanExit(browser->profile()) &&
      !SessionCrashedBubble::Show(browser)) {
    SessionCrashedInfoBarDelegate::Create(browser);
  }

  // The below info bars are only added to the first profile which is launched.
  // Other profiles might be restoring the browsing sessions asynchronously,
  // so we cannot add the info bars to the focused tabs here.
  if (is_process_startup == chrome::startup::IS_PROCESS_STARTUP &&
      !command_line_.HasSwitch(switches::kTestType)) {
    chrome::ShowBadFlagsPrompt(browser);
    GoogleApiKeysInfoBarDelegate::Create(InfoBarService::FromWebContents(
        browser->tab_strip_model()->GetActiveWebContents()));
    ObsoleteSystemInfoBarDelegate::Create(InfoBarService::FromWebContents(
        browser->tab_strip_model()->GetActiveWebContents()));

#if !defined(OS_CHROMEOS)
    if (!command_line_.HasSwitch(switches::kNoDefaultBrowserCheck)) {
      // Generally, the default browser prompt should not be shown on first
      // run. However, when the set-as-default dialog has been suppressed, we
      // need to allow it.
      if (!is_first_run_ ||
          (browser_creator_ &&
           browser_creator_->is_default_browser_dialog_suppressed())) {
        chrome::ShowDefaultBrowserPrompt(profile_,
                                         browser->host_desktop_type());
      }
    }
#endif
  }
}

void StartupBrowserCreatorImpl::AddStartupURLs(
    std::vector<GURL>* startup_urls) const {
  // TODO(atwilson): Simplify the logic that decides which tabs to open on
  // start-up and make it more consistent. http://crbug.com/248883

  // If we have urls specified by the first run master preferences use them
  // and nothing else.
  if (browser_creator_ && startup_urls->empty()) {
    if (!browser_creator_->first_run_tabs_.empty()) {
      std::vector<GURL>::iterator it =
          browser_creator_->first_run_tabs_.begin();
      while (it != browser_creator_->first_run_tabs_.end()) {
        // Replace magic names for the actual urls.
        if (it->host() == "new_tab_page") {
          startup_urls->push_back(GURL(chrome::kChromeUINewTabURL));
        } else if (it->host() == "welcome_page") {
          startup_urls->push_back(internals::GetWelcomePageURL());
        } else {
          startup_urls->push_back(*it);
        }
        ++it;
      }
      browser_creator_->first_run_tabs_.clear();
    }
  }

  // Otherwise open at least the new tab page (and any pages deemed needed by
  // AddSpecialURLs()), or the set of URLs specified on the command line.
  if (startup_urls->empty()) {
    AddSpecialURLs(startup_urls);
    startup_urls->push_back(GURL(chrome::kChromeUINewTabURL));

    // Special case the FIRST_RUN_LAST_TAB case of the welcome page.
    if (welcome_run_type_ == WelcomeRunType::FIRST_RUN_LAST_TAB)
      startup_urls->push_back(internals::GetWelcomePageURL());
  }

  if (signin::ShouldShowPromoAtStartup(profile_, is_first_run_)) {
    signin::DidShowPromoAtStartup(profile_);

    const GURL sync_promo_url = signin::GetPromoURL(
        signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE,
        signin_metrics::Reason::REASON_SIGNIN_PRIMARY_ACCOUNT, false);

    // No need to add if the sync promo is already in the startup list.
    bool add_promo = true;
    for (std::vector<GURL>::const_iterator it = startup_urls->begin();
         it != startup_urls->end(); ++it) {
      if (*it == sync_promo_url) {
        add_promo = false;
        break;
      }
    }

    if (add_promo) {
      // If the first URL is the NTP, replace it with the sync promo. This
      // behavior is desired because completing or skipping the sync promo
      // causes a redirect to the NTP.
      if (!startup_urls->empty() &&
          startup_urls->at(0) == GURL(chrome::kChromeUINewTabURL))
        startup_urls->at(0) = sync_promo_url;
      else
        startup_urls->insert(startup_urls->begin(), sync_promo_url);
    }
  }
}

void StartupBrowserCreatorImpl::AddSpecialURLs(
    std::vector<GURL>* url_list) const {
  // Optionally include the welcome page.
  if (welcome_run_type_ == WelcomeRunType::FIRST_TAB)
    url_list->insert(url_list->begin(), internals::GetWelcomePageURL());

  // If this Profile is marked for a reset prompt, ensure the reset
  // settings dialog appears.
  if (ProfileHasResetTrigger()) {
    url_list->insert(url_list->begin(),
                     internals::GetTriggeredResetSettingsURL());
  }
}

// For first-run, the type will be FIRST_RUN_LAST for all systems except for
// Windows 10+, where it will be FIRST_RUN_FIRST. For non-first run, the type
// will be NONE for all systems except for Windows 10+, where it will be
// ANY_RUN_FIRST if this is the first somewhat normal launch since an OS
// upgrade.
void StartupBrowserCreatorImpl::InitializeWelcomeRunType(
    const std::vector<GURL>& urls_to_open) {
  DCHECK_EQ(static_cast<int>(WelcomeRunType::NONE),
            static_cast<int>(welcome_run_type_));
#if defined(OS_WIN)
  // Do not welcome if there are any URLs to open.
  if (!urls_to_open.empty())
    return;

  base::win::OSInfo* const os_info = base::win::OSInfo::GetInstance();
  const base::win::OSInfo::VersionNumber v(os_info->version_number());
  const std::string this_version(base::StringPrintf("%d.%d", v.major, v.minor));
  PrefService* const local_state = g_browser_process->local_state();

  // Always welcome on first run.
  if (first_run::ShouldShowWelcomePage()) {
    // First tab for Win10+, last tab otherwise.
    welcome_run_type_ = os_info->version() >= base::win::VERSION_WIN10
                            ? WelcomeRunType::FIRST_TAB
                            : WelcomeRunType::FIRST_RUN_LAST_TAB;
  } else {
    // Otherwise, do not welcome pre-Win10.
    if (base::win::GetVersion() < base::win::VERSION_WIN10)
      return;

    // Do not welcome if launching into incognito.
    if (IncognitoModePrefs::ShouldLaunchIncognito(command_line_,
                                                  profile_->GetPrefs())) {
      return;
    }

    // Do not welcome if there is no local state (tests).
    if (!local_state)
      return;

    // Do not welcome if disabled by policy or master_preferences.
    if (!local_state->GetBoolean(prefs::kWelcomePageOnOSUpgradeEnabled))
      return;

    // Do not welcome if already shown for this OS version.
    if (local_state->GetString(prefs::kLastWelcomedOSVersion) == this_version)
      return;

    // Do not welcome if offline.
    if (net::NetworkChangeNotifier::IsOffline())
      return;

    // Do not welcome if Chrome was the default browser at startup.
    if (g_browser_process->CachedDefaultWebClientState() ==
        ShellIntegration::IS_DEFAULT) {
      return;
    }

    // Show the welcome page in the first tab.
    welcome_run_type_ = WelcomeRunType::FIRST_TAB;
  }

  // Remember that the welcome page was shown for this OS version.
  local_state->SetString(prefs::kLastWelcomedOSVersion, this_version);
#else  // OS_WIN
  // Show the welcome page as the last tab only on first-run.
  if (first_run::ShouldShowWelcomePage())
    welcome_run_type_ = WelcomeRunType::FIRST_RUN_LAST_TAB;
#endif  // !OS_WIN
}

void StartupBrowserCreatorImpl::RecordRapporOnStartupURLs(
    const std::vector<GURL>& urls_to_open) {
  for (const GURL& url : urls_to_open) {
    rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                            "Startup.BrowserLaunchURL", url);
  }
}

bool StartupBrowserCreatorImpl::ProfileHasResetTrigger() const {
  bool has_reset_trigger = false;
#if defined(OS_WIN)
  TriggeredProfileResetter* triggered_profile_resetter =
      TriggeredProfileResetterFactory::GetForBrowserContext(profile_);
  // TriggeredProfileResetter instance will be nullptr for incognito profiles.
  if (triggered_profile_resetter) {
    has_reset_trigger = triggered_profile_resetter->HasResetTrigger();
  }
#endif  // defined(OS_WIN)
  return has_reset_trigger;
}
