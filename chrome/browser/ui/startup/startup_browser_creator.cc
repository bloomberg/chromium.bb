// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include <stddef.h>

#include <algorithm>   // For max().
#include <set>

#include "apps/app_load_service.h"
#include "apps/switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/profiler/scoped_profile.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/startup_helper.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/user_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/util.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/switches.h"
#include "net/base/port_util.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/app_mode/app_launch_utils.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_app_launcher.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/chromeos_switches.h"
#include "components/user_manager/user_manager.h"
#endif

#if defined(TOOLKIT_VIEWS) && defined(OS_LINUX)
#include "ui/events/devices/x11/touch_factory_x11.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/browser/web_applications/web_app_mac.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/metrics/jumplist_metrics_win.h"
#include "components/search_engines/desktop_search_utils.h"
#endif

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#endif

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;

namespace {

#if defined(OS_WIN)
const wchar_t kSetDefaultBrowserHelpUrl[] =
    L"https://support.google.com/chrome?p=default_browser";

// Not thread-safe. Always use or modify this callback on the UI thread.
base::Closure* g_default_browser_callback = nullptr;
#endif  // defined(OS_WIN)

// Keeps track on which profiles have been launched.
class ProfileLaunchObserver : public content::NotificationObserver {
 public:
  ProfileLaunchObserver()
      : profile_to_activate_(NULL),
        activated_profile_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                   content::NotificationService::AllSources());
  }
  ~ProfileLaunchObserver() override {}

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case chrome::NOTIFICATION_PROFILE_DESTROYED: {
        Profile* profile = content::Source<Profile>(source).ptr();
        launched_profiles_.erase(profile);
        opened_profiles_.erase(profile);
        if (profile == profile_to_activate_)
          profile_to_activate_ = NULL;
        // If this profile was the last launched one without an opened window,
        // then we may be ready to activate |profile_to_activate_|.
        MaybeActivateProfile();
        break;
      }
      case chrome::NOTIFICATION_BROWSER_WINDOW_READY: {
        Browser* browser = content::Source<Browser>(source).ptr();
        DCHECK(browser);
        opened_profiles_.insert(browser->profile());
        MaybeActivateProfile();
        break;
      }
      default:
        NOTREACHED();
    }
  }

  bool HasBeenLaunched(const Profile* profile) const {
    return launched_profiles_.find(profile) != launched_profiles_.end();
  }

  void AddLaunched(Profile* profile) {
    launched_profiles_.insert(profile);
    // Since the startup code only executes for browsers launched in
    // desktop mode, i.e., HOST_DESKTOP_TYPE_NATIVE. Ash should never get here.
    if (chrome::FindBrowserWithProfile(profile,
                                       chrome::HOST_DESKTOP_TYPE_NATIVE)) {
      // A browser may get opened before we get initialized (e.g., in tests),
      // so we never see the NOTIFICATION_BROWSER_WINDOW_READY for it.
      opened_profiles_.insert(profile);
    }
  }

  void Clear() {
    launched_profiles_.clear();
    opened_profiles_.clear();
  }

  bool activated_profile() { return activated_profile_; }

  void set_profile_to_activate(Profile* profile) {
    profile_to_activate_ = profile;
    MaybeActivateProfile();
  }

 private:
  void MaybeActivateProfile() {
    if (!profile_to_activate_)
      return;
    // Check that browsers have been opened for all the launched profiles.
    // Note that browsers opened for profiles that were not added as launched
    // profiles are simply ignored.
    std::set<const Profile*>::const_iterator i = launched_profiles_.begin();
    for (; i != launched_profiles_.end(); ++i) {
      if (opened_profiles_.find(*i) == opened_profiles_.end())
        return;
    }
    // Asynchronous post to give a chance to the last window to completely
    // open and activate before trying to activate |profile_to_activate_|.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ProfileLaunchObserver::ActivateProfile,
                   base::Unretained(this)));
    // Avoid posting more than once before ActivateProfile gets called.
    registrar_.Remove(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                      content::NotificationService::AllSources());
    registrar_.Remove(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                      content::NotificationService::AllSources());
  }

  void ActivateProfile() {
    // We need to test again, in case the profile got deleted in the mean time.
    if (profile_to_activate_) {
      Browser* browser = chrome::FindBrowserWithProfile(
          profile_to_activate_, chrome::HOST_DESKTOP_TYPE_NATIVE);
      // |profile| may never get launched, e.g., if it only had
      // incognito Windows and one of them was used to exit Chrome.
      // So it won't have a browser in that case.
      if (browser)
        browser->window()->Activate();
      // No need try to activate this profile again.
      profile_to_activate_ = NULL;
    }
    // Assign true here, even if no browser was actually activated, so that
    // the test can stop waiting, and fail gracefully when needed.
    activated_profile_ = true;
  }

  // These are the profiles that get launched by
  // StartupBrowserCreator::LaunchBrowser.
  std::set<const Profile*> launched_profiles_;
  // These are the profiles for which at least one browser window has been
  // opened. This is needed to know when it is safe to activate
  // |profile_to_activate_|, otherwise, new browser windows being opened will
  // be activated on top of it.
  std::set<const Profile*> opened_profiles_;
  content::NotificationRegistrar registrar_;
  // This is NULL until the profile to activate has been chosen. This value,
  // should only be set once all profiles have been launched, otherwise,
  // activation may not happen after the launch of newer profiles.
  Profile* profile_to_activate_;
  // Set once we attempted to activate a profile. We only get one shot at this.
  bool activated_profile_;

  DISALLOW_COPY_AND_ASSIGN(ProfileLaunchObserver);
};

base::LazyInstance<ProfileLaunchObserver> profile_launch_observer =
    LAZY_INSTANCE_INITIALIZER;

// Dumps the current set of the browser process's histograms to |output_file|.
// The file is overwritten if it exists. This function should only be called in
// the blocking pool.
void DumpBrowserHistograms(const base::FilePath& output_file) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string output_string(base::StatisticsRecorder::ToJSON(std::string()));
  base::WriteFile(output_file, output_string.data(),
                  static_cast<int>(output_string.size()));
}

// Shows the User Manager on startup if the last used profile must sign in or
// if the last used profile was the guest or system profile.
// Returns true if the User Manager was shown, false otherwise.
bool ShowUserManagerOnStartupIfNeeded(
    Profile* last_used_profile, const base::CommandLine& command_line) {
#if defined(OS_CHROMEOS)
  // ChromeOS never shows the User Manager on startup.
  return false;
#else
  const ProfileInfoCache& profile_info =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  size_t profile_index = profile_info.GetIndexOfProfileWithPath(
      last_used_profile->GetPath());

  if (profile_index == std::string::npos ||
      !profile_info.ProfileIsSigninRequiredAtIndex(profile_index)) {
    // Signin is not required. However, guest, system or locked profiles cannot
    // be re-opened on startup. The only exception is if there's already a Guest
    // window open in a separate process (for example, launching a new browser
    // after clicking on a downloaded file in Guest mode).
    if ((!last_used_profile->IsGuestSession() &&
         !last_used_profile->IsSystemProfile()) ||
        (chrome::GetTotalBrowserCountForProfile(
           last_used_profile->GetOffTheRecordProfile()) > 0)) {
      return false;
    }
  }

  // Show the User Manager.
  profiles::UserManagerProfileSelected action =
      command_line.HasSwitch(switches::kShowAppList) ?
          profiles::USER_MANAGER_SELECT_PROFILE_APP_LAUNCHER :
          profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION;
  UserManager::Show(
      base::FilePath(), profiles::USER_MANAGER_NO_TUTORIAL, action);
  return true;
#endif
}

}  // namespace

StartupBrowserCreator::StartupBrowserCreator()
    : is_default_browser_dialog_suppressed_(false),
      show_main_browser_window_(true),
      show_desktop_search_redirection_infobar_(false) {}

StartupBrowserCreator::~StartupBrowserCreator() {}

// static
bool StartupBrowserCreator::was_restarted_read_ = false;

// static
bool StartupBrowserCreator::in_synchronous_profile_launch_ = false;

void StartupBrowserCreator::AddFirstRunTab(const GURL& url) {
  first_run_tabs_.push_back(url);
}

bool StartupBrowserCreator::Start(const base::CommandLine& cmd_line,
                                  const base::FilePath& cur_dir,
                                  Profile* last_used_profile,
                                  const Profiles& last_opened_profiles) {
  TRACE_EVENT0("startup", "StartupBrowserCreator::Start");
  TRACK_SCOPED_REGION("Startup", "StartupBrowserCreator::Start");
  SCOPED_UMA_HISTOGRAM_TIMER("Startup.StartupBrowserCreator_Start");
  return ProcessCmdLineImpl(cmd_line, cur_dir, true, last_used_profile,
                            last_opened_profiles);
}

// static
bool StartupBrowserCreator::InSynchronousProfileLaunch() {
  return in_synchronous_profile_launch_;
}

bool StartupBrowserCreator::LaunchBrowser(
    const base::CommandLine& command_line,
    Profile* profile,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run) {
  DCHECK(profile);
  in_synchronous_profile_launch_ =
      process_startup == chrome::startup::IS_PROCESS_STARTUP;

  // ChromeOS does a direct browser launch from UserSessionManager, so this is
  // the earliest place we can enable the log.
  if (command_line.HasSwitch(switches::kDnsLogDetails))
    chrome_browser_net::EnablePredictorDetailedLog(true);

  // Continue with the incognito profile from here on if Incognito mode
  // is forced.
  if (IncognitoModePrefs::ShouldLaunchIncognito(command_line,
                                                profile->GetPrefs())) {
    profile = profile->GetOffTheRecordProfile();
  } else if (command_line.HasSwitch(switches::kIncognito)) {
    LOG(WARNING) << "Incognito mode disabled by policy, launching a normal "
                 << "browser session.";
  }

  // Note: This check should have been done in ProcessCmdLineImpl()
  // before calling this function. However chromeos/login/login_utils.cc
  // calls this function directly (see comments there) so it has to be checked
  // again.
  const bool silent_launch = command_line.HasSwitch(switches::kSilentLaunch);

  if (!silent_launch) {
    StartupBrowserCreatorImpl lwp(cur_dir, command_line, this, is_first_run);
    const std::vector<GURL> urls_to_launch =
        GetURLsFromCommandLine(command_line, cur_dir, profile,
                               &show_desktop_search_redirection_infobar_);
    chrome::HostDesktopType host_desktop_type =
        chrome::HOST_DESKTOP_TYPE_NATIVE;

#if defined(USE_ASH) && !defined(OS_CHROMEOS)
    // We want to maintain only one type of instance for now, either ASH
    // or desktop.
    // TODO(shrikant): Remove this code once we decide on running both desktop
    // and ASH instances side by side.
    if (ash::Shell::HasInstance())
      host_desktop_type = chrome::HOST_DESKTOP_TYPE_ASH;
#endif

    const bool launched = lwp.Launch(profile, urls_to_launch,
                               in_synchronous_profile_launch_,
                               host_desktop_type);
    in_synchronous_profile_launch_ = false;
    if (!launched) {
      LOG(ERROR) << "launch error";
      return false;
    }
  } else {
    in_synchronous_profile_launch_ = false;
  }

  profile_launch_observer.Get().AddLaunched(profile);

#if defined(OS_CHROMEOS)
  chromeos::ProfileHelper::Get()->ProfileStartup(profile, process_startup);
#endif
  return true;
}

// static
bool StartupBrowserCreator::WasRestarted() {
  // Stores the value of the preference kWasRestarted had when it was read.
  static bool was_restarted = false;

  if (!was_restarted_read_) {
    PrefService* pref_service = g_browser_process->local_state();
    was_restarted = pref_service->GetBoolean(prefs::kWasRestarted);
    pref_service->SetBoolean(prefs::kWasRestarted, false);
    was_restarted_read_ = true;
  }
  return was_restarted;
}

// static
SessionStartupPref StartupBrowserCreator::GetSessionStartupPref(
    const base::CommandLine& command_line,
    Profile* profile) {
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs);

  // IsChromeFirstRun() looks for a sentinel file to determine whether the user
  // is starting Chrome for the first time. On Chrome OS, the sentinel is stored
  // in a location shared by all users and the check is meaningless. Query the
  // UserManager instead to determine whether the user is new.
#if defined(OS_CHROMEOS)
  const bool is_first_run =
      user_manager::UserManager::Get()->IsCurrentUserNew();
  // On ChromeOS restarts force the user to login again. The expectation is that
  // after a login the user gets clean state. For this reason we ignore
  // StartupBrowserCreator::WasRestarted().
  const bool did_restart = false;
#else
  const bool is_first_run = first_run::IsChromeFirstRun();
  const bool did_restart = StartupBrowserCreator::WasRestarted();
#endif

  // The pref has an OS-dependent default value. For the first run only, this
  // default is overridden with SessionStartupPref::DEFAULT so that first run
  // behavior (sync promo, welcome page) is consistently invoked.
  // This applies only if the pref is still at its default and has not been
  // set by the user, managed prefs or policy.
  if (is_first_run && SessionStartupPref::TypeIsDefault(prefs))
    pref.type = SessionStartupPref::DEFAULT;

  // The switches::kRestoreLastSession command line switch is used to restore
  // sessions after a browser self restart (e.g. after a Chrome upgrade).
  // However, new profiles can be created from a browser process that has this
  // switch so do not set the session pref to SessionStartupPref::LAST for
  // those as there is nothing to restore.
  if ((command_line.HasSwitch(switches::kRestoreLastSession) || did_restart) &&
      !profile->IsNewProfile()) {
    pref.type = SessionStartupPref::LAST;
  }

  // A browser starting for a profile being unlocked should always restore.
  if (!profile->IsGuestSession()) {
    ProfileInfoCache& info_cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t index = info_cache.GetIndexOfProfileWithPath(profile->GetPath());

    if (index != std::string::npos &&
        info_cache.ProfileIsSigninRequiredAtIndex(index)) {
      pref.type = SessionStartupPref::LAST;
    }
  }

  if (pref.type == SessionStartupPref::LAST &&
      IncognitoModePrefs::ShouldLaunchIncognito(command_line, prefs)) {
    // We don't store session information when incognito. If the user has
    // chosen to restore last session and launched incognito, fallback to
    // default launch behavior.
    pref.type = SessionStartupPref::DEFAULT;
  }

  return pref;
}

// static
void StartupBrowserCreator::ClearLaunchedProfilesForTesting() {
  profile_launch_observer.Get().Clear();
}

// static
void StartupBrowserCreator::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
#if defined(OS_WIN)
  registry->RegisterStringPref(prefs::kLastWelcomedOSVersion, std::string());
  registry->RegisterBooleanPref(prefs::kWelcomePageOnOSUpgradeEnabled, true);
#endif
  registry->RegisterBooleanPref(prefs::kWasRestarted, false);
}


#if defined(OS_WIN)
// static
bool StartupBrowserCreator::SetDefaultBrowserCallback(
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!g_default_browser_callback) {
    // This won't leak because the worker class invoking this function always
    // calls ClearDefaultBrowserCallback() in its destructor.
    g_default_browser_callback = new base::Closure(callback);
    return true;
  }
  return false;
}

// static
void StartupBrowserCreator::ClearDefaultBrowserCallback() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  delete g_default_browser_callback;
  g_default_browser_callback = nullptr;
}

// static
const wchar_t* StartupBrowserCreator::GetDefaultBrowserUrl() {
  return kSetDefaultBrowserHelpUrl;
}
#endif  // defined(OS_WIN)

// static
std::vector<GURL> StartupBrowserCreator::GetURLsFromCommandLine(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    bool* show_desktop_search_redirection_infobar) {
  DCHECK(profile);
  DCHECK(show_desktop_search_redirection_infobar);

  std::vector<GURL> urls;

  const base::CommandLine::StringVector& params = command_line.GetArgs();
  for (size_t i = 0; i < params.size(); ++i) {
    base::FilePath param = base::FilePath(params[i]);
    // Handle Vista way of searching - "? <search-term>"
    if ((param.value().size() > 2) && (param.value()[0] == '?') &&
        (param.value()[1] == ' ')) {
      GURL url(GetDefaultSearchURLForSearchTerms(
          TemplateURLServiceFactory::GetForProfile(profile),
          param.LossyDisplayName().substr(2)));
      if (url.is_valid()) {
        urls.push_back(url);
        continue;
      }
    }

    // Otherwise, fall through to treating it as a URL.

    // This will create a file URL or a regular URL.
    // This call can (in rare circumstances) block the UI thread.
    // Allow it until this bug is fixed.
    //  http://code.google.com/p/chromium/issues/detail?id=60641
    GURL url = GURL(param.MaybeAsASCII());

#if defined(OS_WIN)
    // Replace desktop search URL by a default search engine URL if needed.
    // Ignore cases where there are multiple command line arguments, because
    // desktop search never passes multiple URLs to the browser.
    if (params.size() == 1) {
      *show_desktop_search_redirection_infobar =
          ReplaceDesktopSearchURLWithDefaultSearchURLIfNeeded(
              profile->GetPrefs(),
              TemplateURLServiceFactory::GetForProfile(profile), &url);
    }
#endif  // defined(OS_WIN)

    // http://crbug.com/371030: Only use URLFixerUpper if we don't have a valid
    // URL, otherwise we will look in the current directory for a file named
    // 'about' if the browser was started with a about:foo argument.
    if (!url.is_valid()) {
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      url = url_formatter::FixupRelativeFile(cur_dir, param);
    }
    // Exclude dangerous schemes.
    if (url.is_valid()) {
      ChildProcessSecurityPolicy* policy =
          ChildProcessSecurityPolicy::GetInstance();
      if (policy->IsWebSafeScheme(url.scheme()) ||
          url.SchemeIs(url::kFileScheme) ||
#if defined(OS_CHROMEOS)
          // In ChromeOS, allow any settings page to be specified on the command
          // line. See ExistingUserController::OnLoginSuccess.
          (url.spec().find(chrome::kChromeUISettingsURL) == 0) ||
#else
          // Exposed for external cleaners to offer a settings reset to the
          // user. So the URL must match exactly, without any param or prefix.
          (url.spec() ==
           std::string(chrome::kChromeUISettingsURL) +
               chrome::kResetProfileSettingsSubPage) ||
#endif
          (url.spec().compare(url::kAboutBlankURL) == 0)) {
        urls.push_back(url);
      }
    }
  }
  return urls;
}

bool StartupBrowserCreator::ProcessCmdLineImpl(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    bool process_startup,
    Profile* last_used_profile,
    const Profiles& last_opened_profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("startup", "StartupBrowserCreator::ProcessCmdLineImpl");

  DCHECK(last_used_profile);
  if (process_startup) {
    if (command_line.HasSwitch(switches::kDisablePromptOnRepost))
      content::NavigationController::DisablePromptOnRepost();
  }

  bool silent_launch = false;

#if defined(ENABLE_PRINT_PREVIEW)
  // If we are just displaying a print dialog we shouldn't open browser
  // windows.
  if (command_line.HasSwitch(switches::kCloudPrintFile) &&
      print_dialog_cloud::CreatePrintDialogFromCommandLine(last_used_profile,
                                                           command_line)) {
    silent_launch = true;
  }
#endif  // defined(ENABLE_PRINT_PREVIEW)

  if (command_line.HasSwitch(switches::kExplicitlyAllowedPorts)) {
    std::string allowed_ports =
        command_line.GetSwitchValueASCII(switches::kExplicitlyAllowedPorts);
    net::SetExplicitlyAllowedPorts(allowed_ports);
  }

  if (command_line.HasSwitch(switches::kValidateCrx)) {
    if (!process_startup) {
      LOG(ERROR) << "chrome is already running; you must close all running "
                 << "instances before running with the --"
                 << switches::kValidateCrx << " flag";
      return false;
    }
    extensions::StartupHelper helper;
    std::string message;
    std::string error;
    if (helper.ValidateCrx(command_line, &error))
      message = std::string("ValidateCrx Success");
    else
      message = std::string("ValidateCrx Failure: ") + error;
    printf("%s\n", message.c_str());
    return false;
  }

#if defined(OS_CHROMEOS)

  // The browser will be launched after the user logs in.
  if (command_line.HasSwitch(chromeos::switches::kLoginManager))
    silent_launch = true;

  if (chrome::IsRunningInAppMode() &&
      command_line.HasSwitch(switches::kAppId)) {
    chromeos::LaunchAppOrDie(
        last_used_profile,
        command_line.GetSwitchValueASCII(switches::kAppId));

    // Skip browser launch since app mode launches its app window.
    silent_launch = true;
  }

  // If we are a demo app session and we crashed, there is no safe recovery
  // possible. We should instead cleanly exit and go back to the OOBE screen,
  // where we will launch again after the timeout has expired.
  if (chromeos::DemoAppLauncher::IsDemoAppSession(
      command_line.GetSwitchValueASCII(chromeos::switches::kLoginUser))) {
    chrome::AttemptUserExit();
    return false;
  }
#endif  // OS_CHROMEOS

#if defined(TOOLKIT_VIEWS) && defined(USE_X11)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif

#if defined(OS_MACOSX)
  if (web_app::MaybeRebuildShortcut(command_line))
    return true;
#endif

  if (!process_startup &&
      command_line.HasSwitch(switches::kDumpBrowserHistograms)) {
    // Only handle --dump-browser-histograms from a rendezvous. In this case, do
    // not open a new browser window even if no output file was given.
    base::FilePath output_file(
        command_line.GetSwitchValuePath(switches::kDumpBrowserHistograms));
    if (!output_file.empty()) {
      BrowserThread::PostBlockingPoolTask(
          FROM_HERE,
          base::Bind(&DumpBrowserHistograms, output_file));
    }
    silent_launch = true;
  }

  // If --no-startup-window is specified and Chrome is already running then do
  // not open a new window.
  if (!process_startup && command_line.HasSwitch(switches::kNoStartupWindow))
    silent_launch = true;

  // If we don't want to launch a new browser window or tab we are done here.
  if (silent_launch)
    return true;

  if (command_line.HasSwitch(extensions::switches::kLoadApps) &&
      !IncognitoModePrefs::ShouldLaunchIncognito(
          command_line, last_used_profile->GetPrefs())) {
    if (!ProcessLoadApps(command_line, cur_dir, last_used_profile))
      return false;

    // Return early here to avoid opening a browser window.
    // The exception is when there are no browser windows, since we don't want
    // chrome to shut down.
    // TODO(jackhou): Do this properly once keep-alive is handled by the
    // background page of apps. Tracked at http://crbug.com/175381
    if (chrome::GetTotalBrowserCountForProfile(last_used_profile) != 0)
      return true;
  }

  // Check for --load-and-launch-app.
  if (command_line.HasSwitch(apps::kLoadAndLaunchApp) &&
      !IncognitoModePrefs::ShouldLaunchIncognito(
          command_line, last_used_profile->GetPrefs())) {
    base::CommandLine::StringType path =
        command_line.GetSwitchValueNative(apps::kLoadAndLaunchApp);

    if (!apps::AppLoadService::Get(last_used_profile)->LoadAndLaunch(
            base::FilePath(path), command_line, cur_dir)) {
      return false;
    }

    // Return early here since we don't want to open a browser window.
    // The exception is when there are no browser windows, since we don't want
    // chrome to shut down.
    // TODO(jackhou): Do this properly once keep-alive is handled by the
    // background page of apps. Tracked at http://crbug.com/175381
    if (chrome::GetTotalBrowserCountForProfile(last_used_profile) != 0)
      return true;
  }

#if defined(OS_WIN)
  // Intercept a specific url when setting the default browser asynchronously.
  // This only happens on Windows 10+.
  if (g_default_browser_callback) {
    base::CommandLine::StringType default_browser_url_(
        kSetDefaultBrowserHelpUrl);
    for (const auto& arg : command_line.GetArgs()) {
      if (arg == default_browser_url_) {
        g_default_browser_callback->Run();
        return true;
      }
    }
  }

  // Log whether this process was a result of an action in the Windows Jumplist.
  if (command_line.HasSwitch(switches::kWinJumplistAction)) {
    jumplist::LogJumplistActionFromSwitchValue(
        command_line.GetSwitchValueASCII(switches::kWinJumplistAction));
  }
#endif  // defined(OS_WIN)

  chrome::startup::IsProcessStartup is_process_startup = process_startup ?
      chrome::startup::IS_PROCESS_STARTUP :
      chrome::startup::IS_NOT_PROCESS_STARTUP;
  chrome::startup::IsFirstRun is_first_run = first_run::IsChromeFirstRun() ?
      chrome::startup::IS_FIRST_RUN : chrome::startup::IS_NOT_FIRST_RUN;
  // |last_opened_profiles| will be empty in the following circumstances:
  // - This is the first launch. |last_used_profile| is the initial profile.
  // - The user exited the browser by closing all windows for all
  // profiles. |last_used_profile| is the profile which owned the last open
  // window.
  // - Only incognito windows were open when the browser exited.
  // |last_used_profile| is the last used incognito profile. Restoring it will
  // create a browser window for the corresponding original profile.
  if (last_opened_profiles.empty()) {
    if (ShowUserManagerOnStartupIfNeeded(last_used_profile, command_line))
      return true;

    Profile* profile_to_open = last_used_profile->IsGuestSession() ?
        last_used_profile->GetOffTheRecordProfile() : last_used_profile;

    if (!LaunchBrowser(command_line, profile_to_open, cur_dir,
                       is_process_startup, is_first_run)) {
      return false;
    }
  } else {
#if !defined(OS_CHROMEOS)
    // Guest profiles should not be reopened on startup. This can happen if
    // the last used profile was a Guest, but other profiles were also open
    // when Chrome was closed. In this case, pick a different open profile
    // to be the active one, since the Guest profile is never added to the list
    // of open profiles.
    if (last_used_profile->IsGuestSession()) {
      DCHECK(!last_opened_profiles[0]->IsGuestSession());
      last_used_profile = last_opened_profiles[0];
    }
#endif

    // Launch the last used profile with the full command line, and the other
    // opened profiles without the URLs to launch.
    base::CommandLine command_line_without_urls(command_line.GetProgram());
    const base::CommandLine::SwitchMap& switches = command_line.GetSwitches();
    for (base::CommandLine::SwitchMap::const_iterator switch_it =
             switches.begin();
         switch_it != switches.end(); ++switch_it) {
      command_line_without_urls.AppendSwitchNative(switch_it->first,
                                                   switch_it->second);
    }
    // Launch the profiles in the order they became active.
    for (Profiles::const_iterator it = last_opened_profiles.begin();
         it != last_opened_profiles.end(); ++it) {
      DCHECK(!(*it)->IsGuestSession());
      // Don't launch additional profiles which would only open a new tab
      // page. When restarting after an update, all profiles will reopen last
      // open pages.
      SessionStartupPref startup_pref =
          GetSessionStartupPref(command_line, *it);
      if (*it != last_used_profile &&
          startup_pref.type == SessionStartupPref::DEFAULT &&
          !HasPendingUncleanExit(*it))
        continue;

      if (!LaunchBrowser((*it == last_used_profile) ? command_line
                                                    : command_line_without_urls,
                         *it, cur_dir, is_process_startup, is_first_run))
        return false;
      // We've launched at least one browser.
      is_process_startup = chrome::startup::IS_NOT_PROCESS_STARTUP;
    }
    // This must be done after all profiles have been launched so the observer
    // knows about all profiles to wait for before activating this one.
    profile_launch_observer.Get().set_profile_to_activate(last_used_profile);
  }
  return true;
}

// static
bool StartupBrowserCreator::ProcessLoadApps(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile) {
  base::CommandLine::StringType path_list =
      command_line.GetSwitchValueNative(extensions::switches::kLoadApps);

  base::StringTokenizerT<base::CommandLine::StringType,
                         base::CommandLine::StringType::const_iterator>
      tokenizer(path_list, FILE_PATH_LITERAL(","));

  if (!tokenizer.GetNext())
    return false;

  base::FilePath app_absolute_dir =
      base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token()));
  if (!apps::AppLoadService::Get(profile)->LoadAndLaunch(
          app_absolute_dir, command_line, cur_dir)) {
    return false;
  }

  while (tokenizer.GetNext()) {
    app_absolute_dir =
        base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token()));

    if (!apps::AppLoadService::Get(profile)->Load(app_absolute_dir)) {
      return false;
    }
  }

  return true;
}

// static
void StartupBrowserCreator::ProcessCommandLineOnProfileCreated(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;
  StartupBrowserCreator startup_browser_creator;
  startup_browser_creator.ProcessCmdLineImpl(command_line, cur_dir, false,
                                             profile, Profiles());
}

// static
void StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    const base::FilePath& profile_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(profile_path);

  // The profile isn't loaded yet and so needs to be loaded asynchronously.
  if (!profile) {
    profile_manager->CreateProfileAsync(
        profile_path,
        base::Bind(&ProcessCommandLineOnProfileCreated, command_line, cur_dir),
        base::string16(), std::string(), std::string());
    return;
  }
  StartupBrowserCreator startup_browser_creator;
  startup_browser_creator.ProcessCmdLineImpl(command_line, cur_dir, false,
                                             profile, Profiles());
}

// static
bool StartupBrowserCreator::ActivatedProfile() {
  return profile_launch_observer.Get().activated_profile();
}

bool HasPendingUncleanExit(Profile* profile) {
  return profile->GetLastSessionExitType() == Profile::EXIT_CRASHED &&
      !profile_launch_observer.Get().HasBeenLaunched(profile);
}

base::FilePath GetStartupProfilePath(const base::FilePath& user_data_dir,
                                     const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kProfileDirectory)) {
    return user_data_dir.Append(
        command_line.GetSwitchValuePath(switches::kProfileDirectory));
  }

  // If we are showing the app list then chrome isn't shown so load the app
  // list's profile rather than chrome's.
  if (command_line.HasSwitch(switches::kShowAppList)) {
    return AppListService::Get(chrome::HOST_DESKTOP_TYPE_NATIVE)->
        GetProfilePath(user_data_dir);
  }

  return g_browser_process->profile_manager()->GetLastUsedProfileDir(
      user_data_dir);
}
