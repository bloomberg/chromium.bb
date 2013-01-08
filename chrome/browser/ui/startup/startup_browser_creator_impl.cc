// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/event_recorder.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/auto_launch_trial.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/app_restore_service.h"
#include "chrome/browser/extensions/app_restore_service_factory.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/performance_monitor/startup_timer.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/rlz/rlz.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/autolaunch_prompt.h"
#include "chrome/browser/ui/startup/bad_flags_prompt.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#include "chrome/browser/ui/startup/obsolete_os_info_bar.h"
#include "chrome/browser/ui/startup/session_crashed_prompt.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_trial.h"
#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#include "chrome/browser/ui/cocoa/keystone_infobar_delegate.h"
#endif

#if defined(TOOLKIT_GTK)
#include "chrome/browser/ui/gtk/gtk_util.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(ENABLE_APP_LIST)
#include "chrome/browser/ui/app_list/app_list_controller.h"
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
    string16 shortcut(si.lpTitle);
    // The windows quick launch path is not localized.
    if (shortcut.find(L"\\Quick Launch\\") != string16::npos) {
      if (base::win::GetVersion() >= base::win::VERSION_WIN7)
        return LM_SHORTCUT_TASKBAR;
      else
        return LM_SHORTCUT_QUICKLAUNCH;
    }
    scoped_ptr<base::Environment> env(base::Environment::Create());
    std::string appdata_path;
    env->GetVar("USERPROFILE", &appdata_path);
    if (!appdata_path.empty() &&
        shortcut.find(ASCIIToWide(appdata_path)) != std::wstring::npos)
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
    extension_misc::LaunchContainer* out_launch_container) {

  ExtensionService* extensions_service = profile->GetExtensionService();
  const Extension* extension =
      extensions_service->GetExtensionById(app_id, false);

  // The extension with id |app_id| may have been uninstalled.
  if (!extension)
    return false;

  // Look at preferences to find the right launch container.  If no
  // preference is set, launch as a window.
  extension_misc::LaunchContainer launch_container =
      extensions_service->extension_prefs()->GetLaunchContainer(
          extension, extensions::ExtensionPrefs::LAUNCH_WINDOW);

  *out_extension = extension;
  *out_launch_container = launch_container;
  return true;
}

// Parse two comma-separated integers from string. Return true on success.
bool ParseCommaSeparatedIntegers(const std::string& str,
                                 int* ret_num1,
                                 int* ret_num2) {
  std::vector<std::string> dimensions;
  base::SplitString(str, ',', &dimensions);
  if (dimensions.size() != 2)
    return false;

  int num1, num2;
  if (!base::StringToInt(dimensions[0], &num1) ||
      !base::StringToInt(dimensions[1], &num2))
    return false;

  *ret_num1 = num1;
  *ret_num2 = num2;
  return true;
}

void RecordCmdLineAppHistogram() {
  AppLauncherHandler::RecordAppLaunchType(
      extension_misc::APP_LAUNCH_CMD_LINE_APP);
}

void RecordAppLaunches(Profile* profile,
                       const std::vector<GURL>& cmd_line_urls,
                       StartupTabs& autolaunch_tabs) {
  ExtensionService* extension_service = profile->GetExtensionService();
  DCHECK(extension_service);
  for (size_t i = 0; i < cmd_line_urls.size(); ++i) {
    if (extension_service->IsInstalledApp(cmd_line_urls.at(i))) {
      AppLauncherHandler::RecordAppLaunchType(
          extension_misc::APP_LAUNCH_CMD_LINE_URL);
    }
  }
  for (size_t i = 0; i < autolaunch_tabs.size(); ++i) {
    if (extension_service->IsInstalledApp(autolaunch_tabs.at(i).url)) {
      AppLauncherHandler::RecordAppLaunchType(
          extension_misc::APP_LAUNCH_AUTOLAUNCH);
    }
  }
}

class WebContentsCloseObserver : public content::NotificationObserver {
 public:
  WebContentsCloseObserver() : contents_(NULL) {}
  virtual ~WebContentsCloseObserver() {}

  void SetContents(content::WebContents* contents) {
    DCHECK(!contents_);
    contents_ = contents;

    registrar_.Add(this,
                   content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   content::Source<content::WebContents>(contents_));
  }

  content::WebContents* contents() { return contents_; }

 private:
  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(type, content::NOTIFICATION_WEB_CONTENTS_DESTROYED);
    contents_ = NULL;
  }

  content::WebContents* contents_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsCloseObserver);
};

}  // namespace

namespace internals {

GURL GetWelcomePageURL() {
  std::string welcome_url = l10n_util::GetStringUTF8(IDS_WELCOME_PAGE_URL);
  return GURL(welcome_url);
}

}  // namespace internals

StartupBrowserCreatorImpl::StartupBrowserCreatorImpl(
    const FilePath& cur_dir,
    const CommandLine& command_line,
    chrome::startup::IsFirstRun is_first_run)
    : cur_dir_(cur_dir),
      command_line_(command_line),
      profile_(NULL),
      browser_creator_(NULL),
      is_first_run_(is_first_run == chrome::startup::IS_FIRST_RUN) {
}

StartupBrowserCreatorImpl::StartupBrowserCreatorImpl(
    const FilePath& cur_dir,
    const CommandLine& command_line,
    StartupBrowserCreator* browser_creator,
    chrome::startup::IsFirstRun is_first_run)
    : cur_dir_(cur_dir),
      command_line_(command_line),
      profile_(NULL),
      browser_creator_(browser_creator),
      is_first_run_(is_first_run == chrome::startup::IS_FIRST_RUN) {
}

StartupBrowserCreatorImpl::~StartupBrowserCreatorImpl() {
}

bool StartupBrowserCreatorImpl::Launch(Profile* profile,
                                       const std::vector<GURL>& urls_to_open,
                                       bool process_startup) {
  DCHECK(profile);
  profile_ = profile;

  if (command_line_.HasSwitch(switches::kDnsLogDetails))
    chrome_browser_net::EnablePredictorDetailedLog(true);
  if (command_line_.HasSwitch(switches::kDnsPrefetchDisable) &&
      profile->GetNetworkPredictor()) {
    profile->GetNetworkPredictor()->EnablePredictor(false);
  }

  if (command_line_.HasSwitch(switches::kDumpHistogramsOnExit))
    base::StatisticsRecorder::set_dump_on_exit(true);

#if defined(ENABLE_APP_LIST)
  app_list_controller::InitAppList();
  if (command_line_.HasSwitch(switches::kShowAppList)) {
    app_list_controller::ShowAppList();
    return true;
  }
#endif

  // Open the required browser windows and tabs. First, see if
  // we're being run as an application window. If so, the user
  // opened an app shortcut.  Don't restore tabs or open initial
  // URLs in that case. The user should see the window as an app,
  // not as chrome.
  // Special case is when app switches are passed but we do want to restore
  // session. In that case open app window + focus it after session is restored.
  content::WebContents* app_contents = NULL;
  if (OpenApplicationWindow(profile, &app_contents)) {
    RecordLaunchModeHistogram(LM_AS_WEBAPP);
  } else {
    RecordLaunchModeHistogram(urls_to_open.empty() ?
                              LM_TO_BE_DECIDED : LM_WITH_URLS);

    ProcessLaunchURLs(process_startup, urls_to_open);

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

  // If we're recording or playing back, startup the EventRecorder now
  // unless otherwise specified.
  if (!command_line_.HasSwitch(switches::kNoEvents)) {
    FilePath script_path;
    PathService::Get(chrome::FILE_RECORDED_SCRIPT, &script_path);

    bool record_mode = command_line_.HasSwitch(switches::kRecordMode);
    bool playback_mode = command_line_.HasSwitch(switches::kPlaybackMode);

    if (record_mode && chrome::kRecordModeEnabled)
      base::EventRecorder::current()->StartRecording(script_path);
    // Do not enter Playback mode if PageCycler is running; Playback mode does
    // not work correctly.
    if (playback_mode && !command_line_.HasSwitch(switches::kVisitURLs))
      base::EventRecorder::current()->StartPlayback(script_path);
  }

#if defined(OS_WIN)
  if (process_startup)
    ShellIntegration::MigrateChromiumShortcuts();
#endif  // defined(OS_WIN)

  return true;
}

void StartupBrowserCreatorImpl::ExtractOptionalAppWindowSize(
    gfx::Rect* bounds) {
  if (command_line_.HasSwitch(switches::kAppWindowSize)) {
    int width, height;
    width = height = 0;
    std::string switch_value =
        command_line_.GetSwitchValueASCII(switches::kAppWindowSize);
    if (ParseCommaSeparatedIntegers(switch_value, &width, &height)) {
      // TODO(scottmg): NativeScreen might be wrong. http://crbug.com/133312
      const gfx::Rect work_area =
          gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area();
      width = std::min(width, work_area.width());
      height = std::min(height, work_area.height());
      bounds->set_size(gfx::Size(width, height));
      bounds->set_x((work_area.width() - bounds->width()) / 2);
      // TODO(nkostylev): work_area does include launcher but should not.
      // Launcher auto hide pref is synced and is most likely not applied here.
      bounds->set_y((work_area.height() - bounds->height()) / 2);
    }
  }
}

bool StartupBrowserCreatorImpl::IsAppLaunch(Profile* profile,
                                            std::string* app_url,
                                            std::string* app_id) {
  // Don't launch apps in incognito mode.
  if (profile->IsOffTheRecord())
    return false;
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
  if (!IsAppLaunch(profile, NULL, &app_id) || app_id.empty())
    return false;

  extension_misc::LaunchContainer launch_container;
  const Extension* extension;
  if (!GetAppLaunchContainer(profile, app_id, &extension, &launch_container))
    return false;

  // If the user doesn't want to open a tab, fail.
  if (launch_container != extension_misc::LAUNCH_TAB)
    return false;

  RecordCmdLineAppHistogram();

  WebContents* app_tab = application_launch::OpenApplication(
      application_launch::LaunchParams(profile, extension,
          extension_misc::LAUNCH_TAB, NEW_FOREGROUND_TAB));
  return (app_tab != NULL);
}

bool StartupBrowserCreatorImpl::OpenApplicationWindow(
    Profile* profile,
    content::WebContents** out_app_contents) {
  // Set |out_app_contents| to NULL early on (just in case).
  if (out_app_contents)
    *out_app_contents = NULL;

  std::string url_string, app_id;
  if (!IsAppLaunch(profile, &url_string, &app_id))
    return false;

  // This can fail if the app_id is invalid.  It can also fail if the
  // extension is external, and has not yet been installed.
  // TODO(skerner): Do something reasonable here. Pop up a warning panel?
  // Open an URL to the gallery page of the extension id?
  if (!app_id.empty()) {
    extension_misc::LaunchContainer launch_container;
    const Extension* extension;
    if (!GetAppLaunchContainer(profile, app_id, &extension, &launch_container))
      return false;

    // TODO(skerner): Could pass in |extension| and |launch_container|,
    // and avoid calling GetAppLaunchContainer() both here and in
    // OpenApplicationTab().

    if (launch_container == extension_misc::LAUNCH_TAB)
      return false;

    RecordCmdLineAppHistogram();

    application_launch::LaunchParams params(profile, extension,
                                            launch_container, NEW_WINDOW);
    params.command_line = &command_line_;
    params.current_directory = cur_dir_;
    WebContents* tab_in_app_window = application_launch::OpenApplication(
        params);

    if (out_app_contents)
      *out_app_contents = tab_in_app_window;

    // Platform apps fire off a launch event which may or may not open a window.
    return (tab_in_app_window != NULL || extension->is_platform_app());
  }

  if (url_string.empty())
    return false;

#if defined(OS_WIN)  // Fix up Windows shortcuts.
  ReplaceSubstringsAfterOffset(&url_string, 0, "\\x", "%");
#endif
  GURL url(url_string);

  // Restrict allowed URLs for --app switch.
  if (!url.is_empty() && url.is_valid()) {
    ChildProcessSecurityPolicy* policy =
        ChildProcessSecurityPolicy::GetInstance();
    if (policy->IsWebSafeScheme(url.scheme()) ||
        url.SchemeIs(chrome::kFileScheme)) {
      if (profile->GetExtensionService()->IsInstalledApp(url)) {
        RecordCmdLineAppHistogram();
      } else {
        AppLauncherHandler::RecordAppLaunchType(
            extension_misc::APP_LAUNCH_CMD_LINE_APP_LEGACY);
      }

      gfx::Rect override_bounds;
      ExtractOptionalAppWindowSize(&override_bounds);

      WebContents* app_tab = application_launch::OpenAppShortcutWindow(
          profile,
          url,
          override_bounds);

      if (out_app_contents)
        *out_app_contents = app_tab;

      return (app_tab != NULL);
    }
  }
  return false;
}

void StartupBrowserCreatorImpl::ProcessLaunchURLs(
    bool process_startup,
    const std::vector<GURL>& urls_to_open) {
  // If we're starting up in "background mode" (no open browser window) then
  // don't open any browser windows, unless kAutoLaunchAtStartup is also
  // specified.
  if (process_startup &&
      command_line_.HasSwitch(switches::kNoStartupWindow) &&
      !command_line_.HasSwitch(switches::kAutoLaunchAtStartup)) {
    return;
  }

  if (process_startup && ProcessStartupURLs(urls_to_open)) {
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
    SessionService* service = SessionServiceFactory::GetForProfile(profile_);
    if (service && service->ShouldNewWindowStartSession()) {
      // Restore the last session if any.
      if (!HasPendingUncleanExit(profile_) &&
          service->RestoreIfNecessary(urls_to_open)) {
        return;
      }
      // Open user-specified URLs like pinned tabs and startup tabs.
      Browser* browser = ProcessSpecifiedURLs(urls_to_open);
      if (browser) {
        AddInfoBarsIfNecessary(browser, is_process_startup);
        return;
      }
    }
  }

  // Session startup didn't occur, open the urls.
  Browser* browser = NULL;
  std::vector<GURL> adjust_urls = urls_to_open;
  if (adjust_urls.empty()) {
    AddStartupURLs(&adjust_urls);
    if (StartupBrowserCreatorImpl::OpenStartupURLsInExistingBrowser(
            profile_, adjust_urls))
      return;
  } else if (!command_line_.HasSwitch(switches::kOpenInNewWindow)) {
    // Always open a list of urls in a window on the native desktop.
    browser = browser::FindTabbedBrowser(profile_, false,
                                         chrome::HOST_DESKTOP_TYPE_NATIVE);
  }
  // This will launch a browser; prevent session restore.
  StartupBrowserCreator::in_synchronous_profile_launch_ = true;
  browser = OpenURLsInBrowser(browser, process_startup, adjust_urls);
  StartupBrowserCreator::in_synchronous_profile_launch_ = false;
  AddInfoBarsIfNecessary(browser, is_process_startup);
}

bool StartupBrowserCreatorImpl::ProcessStartupURLs(
    const std::vector<GURL>& urls_to_open) {
  VLOG(1) << "StartupBrowserCreatorImpl::ProcessStartupURLs";
  SessionStartupPref pref =
      StartupBrowserCreator::GetSessionStartupPref(command_line_, profile_);
  if (pref.type == SessionStartupPref::LAST)
    VLOG(1) << "Pref: last";
  else if (pref.type == SessionStartupPref::URLS)
    VLOG(1) << "Pref: urls";
  else if (pref.type == SessionStartupPref::DEFAULT)
    VLOG(1) << "Pref: default";

  extensions::AppRestoreService* service =
      extensions::AppRestoreServiceFactory::GetForProfile(profile_);
  // NULL in incognito mode.
  if (service) {
    bool should_restore_apps = StartupBrowserCreator::WasRestarted();
#if defined(OS_CHROMEOS)
    // Chromeos always restarts apps, even if it was a regular shutdown.
    should_restore_apps = true;
#endif
    service->HandleStartup(should_restore_apps);
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

    uint32 restore_behavior = SessionRestore::SYNCHRONOUS;
    if (browser_defaults::kAlwaysCreateTabbedBrowserOnSessionRestore ||
        CommandLine::ForCurrentProcess()->HasSwitch(
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

    // Pause the StartupTimer. Since the restore here is synchronous, we can
    // keep these two metrics (browser startup time and session restore time)
    // separate.
    performance_monitor::StartupTimer::PauseTimer();

    Browser* browser = SessionRestore::RestoreSession(profile_,
                                                      NULL,
                                                      restore_behavior,
                                                      urls_to_open);

    performance_monitor::StartupTimer::UnpauseTimer();

    AddInfoBarsIfNecessary(browser, chrome::startup::IS_PROCESS_STARTUP);
    return true;
  }

  Browser* browser = ProcessSpecifiedURLs(urls_to_open);
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
    const std::vector<GURL>& urls_to_open) {
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
    // Only use the set of urls specified in preferences if nothing was
    // specified on the command line. Filter out any urls that are to be
    // restored by virtue of having been previously pinned.
    AddUniqueURLs(pref.urls, &tabs);
  } else if (pref.type == SessionStartupPref::HOMEPAGE) {
    // If 'homepage' selected, either by the user or by a policy, we should
    // have migrated them to another value.
    NOTREACHED() << "SessionStartupPref has deprecated type HOMEPAGE";
  }

  if (tabs.empty())
    return NULL;

  Browser* browser = OpenTabsInBrowser(NULL, true, tabs);
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
    const std::vector<GURL>& urls) {
  StartupTabs tabs;
  UrlsToTabs(urls, &tabs);
  return OpenTabsInBrowser(browser, process_startup, tabs);
}

Browser* StartupBrowserCreatorImpl::OpenTabsInBrowser(Browser* browser,
                                                      bool process_startup,
                                                      const StartupTabs& tabs) {
  DCHECK(!tabs.empty());

  // If we don't yet have a profile, try to use the one we're given from
  // |browser|. While we may not end up actually using |browser| (since it
  // could be a popup window), we can at least use the profile.
  if (!profile_ && browser)
    profile_ = browser->profile();

  if (!browser || !browser->is_type_tabbed()) {
    // The startup code only executes for browsers launched in desktop mode.
    // i.e. HOST_DESKTOP_TYPE_NATIVE. Ash should never get here.
    browser = new Browser(Browser::CreateParams(
        profile_, chrome::HOST_DESKTOP_TYPE_NATIVE));
  } else {
#if defined(TOOLKIT_GTK)
    // Setting the time of the last action on the window here allows us to steal
    // focus, which is what the user wants when opening a new tab in an existing
    // browser window.
    gtk_util::SetWMLastUserActionTime(browser->window()->GetNativeWindow());
#endif
  }

  // In kiosk mode, we want to always be fullscreen, so switch to that now.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    chrome::ToggleFullscreenMode(browser);

  bool first_tab = true;
  for (size_t i = 0; i < tabs.size(); ++i) {
    // We skip URLs that we'd have to launch an external protocol handler for.
    // This avoids us getting into an infinite loop asking ourselves to open
    // a URL, should the handler be (incorrectly) configured to be us. Anyone
    // asking us to open such a URL should really ask the handler directly.
    bool handled_by_chrome = ProfileIOData::IsHandledURL(tabs[i].url) ||
        (profile_ && profile_->GetProtocolHandlerRegistry()->IsHandledProtocol(
            tabs[i].url.scheme()));
    if (!process_startup && !handled_by_chrome)
      continue;

    int add_types = first_tab ? TabStripModel::ADD_ACTIVE :
                                TabStripModel::ADD_NONE;
    add_types |= TabStripModel::ADD_FORCE_INDEX;
    if (tabs[i].is_pinned)
      add_types |= TabStripModel::ADD_PINNED;

    chrome::NavigateParams params(browser, tabs[i].url,
                                  content::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = first_tab ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
    params.tabstrip_add_types = add_types;
    params.extension_app_id = tabs[i].app_id;

#if defined(ENABLE_RLZ)
    if (process_startup &&
        google_util::IsGoogleHomePageUrl(tabs[i].url.spec())) {
      params.extra_headers = RLZTracker::GetAccessPointHttpHeader(
          RLZTracker::CHROME_HOME_PAGE);
    }
#endif

    chrome::Navigate(&params);

    first_tab = false;
  }
  if (!chrome::GetActiveWebContents(browser)) {
    // TODO: this is a work around for 110909. Figure out why it's needed.
    if (!browser->tab_count())
      chrome::AddBlankTabAt(browser, -1, true);
    else
      browser->tab_strip_model()->ActivateTabAt(0, false);
  }

  // The default behaviour is to show the window, as expressed by the default
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
  if (!browser || !profile_ || browser->tab_count() == 0)
    return;

  if (HasPendingUncleanExit(browser->profile()))
    SessionCrashedInfoBarDelegate::Create(browser);

  // The bad flags info bar and the obsolete system info bar are only added to
  // the first profile which is launched. Other profiles might be restoring the
  // browsing sessions asynchronously, so we cannot add the info bars to the
  // focused tabs here.
  if (is_process_startup == chrome::startup::IS_PROCESS_STARTUP) {
    chrome::ShowBadFlagsPrompt(browser);
    chrome::ObsoleteOSInfoBar::Create(
        InfoBarService::FromWebContents(chrome::GetActiveWebContents(browser)));

    if (browser_defaults::kOSSupportsOtherBrowsers &&
        !command_line_.HasSwitch(switches::kNoDefaultBrowserCheck)) {
      // Generally, the default browser prompt should not be shown on first
      // run. However, when the set-as-default dialog has been suppressed, we
      // need to allow it.
      if ((!is_first_run_ ||
           (browser_creator_ &&
            browser_creator_->is_default_browser_dialog_suppressed())) &&
          !chrome::ShowAutolaunchPrompt(browser)) {
        chrome::ShowDefaultBrowserPrompt(profile_,
                                         browser->host_desktop_type());
      }
    }
  }
}


void StartupBrowserCreatorImpl::AddStartupURLs(
    std::vector<GURL>* startup_urls) const {
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

  // Otherwise open at least the new tab page (and the welcome page, if this
  // is the first time the browser is being started), or the set of URLs
  // specified on the command line.
  if (startup_urls->empty()) {
    startup_urls->push_back(GURL(chrome::kChromeUINewTabURL));
    PrefService* prefs = g_browser_process->local_state();
    if (prefs->FindPreference(prefs::kShouldShowWelcomePage) &&
        prefs->GetBoolean(prefs::kShouldShowWelcomePage)) {
      // Reset the preference so we don't show the welcome page next time.
      prefs->ClearPref(prefs::kShouldShowWelcomePage);
      startup_urls->push_back(internals::GetWelcomePageURL());
    }
  }

  PrefService* prefs = profile_->GetPrefs();
  if (is_first_run_ && prefs->GetBoolean(prefs::kProfileIsManaged)) {
    startup_urls->insert(startup_urls->begin(),
                         GURL(std::string(chrome::kChromeUISettingsURL) +
                              chrome::kManagedUserSettingsSubPage));
  } else if (SyncPromoUI::ShouldShowSyncPromoAtStartup(profile_,
                                                       is_first_run_)) {
    // If the sync promo page is going to be displayed then insert it at the
    // front of the list.
    GURL continue_url;
    if (!SyncPromoUI::UseWebBasedSigninFlow()) {
      continue_url = GURL(chrome::kChromeUINewTabURL);
    }

    SyncPromoUI::DidShowSyncPromoAtStartup(profile_);
    GURL old_url = (*startup_urls)[0];
    (*startup_urls)[0] =
        SyncPromoUI::GetSyncPromoURL(continue_url,
                                     SyncPromoUI::SOURCE_START_PAGE,
                                     false);

    // An empty URL means to go to the home page.
    if (old_url.is_empty() &&
        profile_->GetHomePage() == GURL(chrome::kChromeUINewTabURL)) {
      old_url = GURL(chrome::kChromeUINewTabURL);
    }

    // If the old URL is not the NTP then insert it right after the sync promo.
    if (old_url != GURL(chrome::kChromeUINewTabURL))
      startup_urls->insert(startup_urls->begin() + 1, old_url);
  }
}

#if !defined(OS_WIN) || defined(USE_AURA)
// static
bool StartupBrowserCreatorImpl::OpenStartupURLsInExistingBrowser(
    Profile* profile,
    const std::vector<GURL>& startup_urls) {
  return false;
}
#endif
