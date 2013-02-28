// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include <algorithm>   // For max().
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/auto_launch_trial.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/chrome_frame_automation_provider.h"
#include "chrome/browser/automation/testing_automation_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/extensions/startup_helper.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/printing/print_dialog_cloud.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "grit/locale_settings.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_settings.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/profile_startup.h"
#endif

#if defined(TOOLKIT_VIEWS) && defined(OS_LINUX)
#include "ui/base/touch/touch_factory.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/startup/startup_browser_creator_win.h"
#endif

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;

namespace {

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
  virtual ~ProfileLaunchObserver() {}

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
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
    // Stop reacting to new windows being opened to avoid posting more than
    // once before ActivateProfile gets called and set |profile_to_activate_|
    // to NULL.
    registrar_.Remove(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
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

}  // namespace

StartupBrowserCreator::StartupBrowserCreator()
    : is_default_browser_dialog_suppressed_(false),
      show_main_browser_window_(true) {
}

StartupBrowserCreator::~StartupBrowserCreator() {}

// static
bool StartupBrowserCreator::was_restarted_read_ = false;

// static
bool StartupBrowserCreator::in_synchronous_profile_launch_ = false;

void StartupBrowserCreator::AddFirstRunTab(const GURL& url) {
  first_run_tabs_.push_back(url);
}

// static
bool StartupBrowserCreator::InSynchronousProfileLaunch() {
  return in_synchronous_profile_launch_;
}

bool StartupBrowserCreator::LaunchBrowser(
    const CommandLine& command_line,
    Profile* profile,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    int* return_code) {
  in_synchronous_profile_launch_ =
      process_startup == chrome::startup::IS_PROCESS_STARTUP;
  DCHECK(profile);

  // Continue with the incognito profile from here on if Incognito mode
  // is forced.
  if (IncognitoModePrefs::ShouldLaunchIncognito(command_line,
                                                profile->GetPrefs())) {
    profile = profile->GetOffTheRecordProfile();
  } else if (command_line.HasSwitch(switches::kIncognito)) {
    LOG(WARNING) << "Incognito mode disabled by policy, launching a normal "
                 << "browser session.";
  }

  StartupBrowserCreatorImpl lwp(cur_dir, command_line, this, is_first_run);
  std::vector<GURL> urls_to_launch =
      GetURLsFromCommandLine(command_line, cur_dir, profile);
  bool launched = lwp.Launch(profile, urls_to_launch,
                             in_synchronous_profile_launch_);
  in_synchronous_profile_launch_ = false;

  if (!launched) {
    LOG(ERROR) << "launch error";
    if (return_code)
      *return_code = chrome::RESULT_CODE_INVALID_CMDLINE_URL;
    return false;
  }
  profile_launch_observer.Get().AddLaunched(profile);

#if defined(OS_CHROMEOS)
  chromeos::ProfileStartup(profile, process_startup);
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
    const CommandLine& command_line,
    Profile* profile) {
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs);

  // IsChromeFirstRun() looks for a sentinel file to determine whether the user
  // is starting Chrome for the first time. On Chrome OS, the sentinel is stored
  // in a location shared by all users and the check is meaningless. Query the
  // UserManager instead to determine whether the user is new.
#if defined(OS_CHROMEOS)
  const bool is_first_run = chromeos::UserManager::Get()->IsCurrentUserNew();
#else
  const bool is_first_run = first_run::IsChromeFirstRun();
#endif

  // The pref has an OS-dependent default value. For the first run only, this
  // default is overridden with SessionStartupPref::DEFAULT so that first run
  // behavior (sync promo, welcome page) is consistently invoked.
  // This applies only if the pref is still at its default and has not been
  // set by the user, managed prefs or policy.
  if (is_first_run && SessionStartupPref::TypeIsDefault(prefs))
    pref.type = SessionStartupPref::DEFAULT;

  if (command_line.HasSwitch(switches::kRestoreLastSession) ||
      StartupBrowserCreator::WasRestarted()) {
    pref.type = SessionStartupPref::LAST;
  }
  if (pref.type == SessionStartupPref::LAST &&
      IncognitoModePrefs::ShouldLaunchIncognito(command_line, prefs)) {
    // We don't store session information when incognito. If the user has
    // chosen to restore last session and launched incognito, fallback to
    // default launch behavior.
    pref.type = SessionStartupPref::DEFAULT;
  }

#if defined(OS_CHROMEOS)
  // Kiosk/Retail mode has no profile to restore and fails to open the tabs
  // specified in the startup_urls policy if we try to restore the non-existent
  // session which is the default for ChromeOS in general.
  if (chromeos::KioskModeSettings::Get()->IsKioskModeEnabled()) {
    DCHECK(pref.type == SessionStartupPref::LAST);
    pref.type = SessionStartupPref::DEFAULT;
  }
#endif  // OS_CHROMEOS

  return pref;
}

// static
void StartupBrowserCreator::ClearLaunchedProfilesForTesting() {
  profile_launch_observer.Get().Clear();
}

// static
std::vector<GURL> StartupBrowserCreator::GetURLsFromCommandLine(
    const CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile) {
  std::vector<GURL> urls;
  const CommandLine::StringVector& params = command_line.GetArgs();

  for (size_t i = 0; i < params.size(); ++i) {
    base::FilePath param = base::FilePath(params[i]);
    // Handle Vista way of searching - "? <search-term>"
    if (param.value().size() > 2 &&
        param.value()[0] == '?' && param.value()[1] == ' ') {
      const TemplateURL* default_provider =
          TemplateURLServiceFactory::GetForProfile(profile)->
          GetDefaultSearchProvider();
      if (default_provider) {
        const TemplateURLRef& search_url = default_provider->url_ref();
        DCHECK(search_url.SupportsReplacement());
        string16 search_term = param.LossyDisplayName().substr(2);
        urls.push_back(GURL(search_url.ReplaceSearchTerms(
            TemplateURLRef::SearchTermsArgs(search_term))));
        continue;
      }
    }

    // Otherwise, fall through to treating it as a URL.

    // This will create a file URL or a regular URL.
    // This call can (in rare circumstances) block the UI thread.
    // Allow it until this bug is fixed.
    //  http://code.google.com/p/chromium/issues/detail?id=60641
    GURL url;
    {
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      url = URLFixerUpper::FixupRelativeFile(cur_dir, param);
    }
    // Exclude dangerous schemes.
    if (url.is_valid()) {
      ChildProcessSecurityPolicy *policy =
          ChildProcessSecurityPolicy::GetInstance();
      if (policy->IsWebSafeScheme(url.scheme()) ||
          url.SchemeIs(chrome::kFileScheme) ||
#if defined(OS_CHROMEOS)
          // In ChromeOS, allow a settings page to be specified on the
          // command line. See ExistingUserController::OnLoginSuccess.
          (url.spec().find(chrome::kChromeUISettingsURL) == 0) ||
#endif
          (url.spec().compare(chrome::kAboutBlankURL) == 0)) {
        urls.push_back(url);
      }
    }
  }
#if defined(OS_WIN)
  if (urls.empty()) {
    // If we are in Windows 8 metro mode and were launched as a result of the
    // search charm or via a url navigation in metro, then fetch the
    // corresponding url.
    GURL url = chrome::GetURLToOpen(profile);
    if (url.is_valid())
      urls.push_back(GURL(url));
  }
#endif  // OS_WIN
  return urls;
}

// static
bool StartupBrowserCreator::ProcessCmdLineImpl(
    const CommandLine& command_line,
    const base::FilePath& cur_dir,
    bool process_startup,
    Profile* last_used_profile,
    const Profiles& last_opened_profiles,
    int* return_code,
    StartupBrowserCreator* browser_creator) {
  DCHECK(last_used_profile);
  if (process_startup) {
    if (command_line.HasSwitch(switches::kDisablePromptOnRepost))
      content::NavigationController::DisablePromptOnRepost();
  }

  bool silent_launch = false;

#if defined(ENABLE_AUTOMATION)
  // Look for the testing channel ID ONLY during process startup
  if (process_startup &&
      command_line.HasSwitch(switches::kTestingChannelID)) {
    std::string testing_channel_id = command_line.GetSwitchValueASCII(
        switches::kTestingChannelID);
    // TODO(sanjeevr) Check if we need to make this a singleton for
    // compatibility with the old testing code
    // If there are any extra parameters, we expect each one to generate a
    // new tab; if there are none then we get one homepage tab.
    int expected_tab_count = 1;
    if (command_line.HasSwitch(switches::kNoStartupWindow) &&
        !command_line.HasSwitch(switches::kAutoLaunchAtStartup)) {
      expected_tab_count = 0;
#if defined(OS_CHROMEOS)
    // kLoginManager will cause Chrome to start up with the ChromeOS login
    // screen instead of a browser window, so it won't load any tabs.
    } else if (command_line.HasSwitch(switches::kLoginManager)) {
      expected_tab_count = 0;
#endif
    } else if (command_line.HasSwitch(switches::kRestoreLastSession)) {
      std::string restore_session_value(
          command_line.GetSwitchValueASCII(switches::kRestoreLastSession));
      base::StringToInt(restore_session_value, &expected_tab_count);
    } else {
      std::vector<GURL> urls_to_open = GetURLsFromCommandLine(
          command_line, cur_dir, last_used_profile);
      expected_tab_count =
          std::max(1, static_cast<int>(urls_to_open.size()));
    }
    if (!CreateAutomationProvider<TestingAutomationProvider>(
        testing_channel_id,
        last_used_profile,
        static_cast<size_t>(expected_tab_count)))
      return false;
  }

  if (command_line.HasSwitch(switches::kSilentLaunch)) {
    std::vector<GURL> urls_to_open = GetURLsFromCommandLine(
        command_line, cur_dir, last_used_profile);
    size_t expected_tabs =
        std::max(static_cast<int>(urls_to_open.size()), 0);
    if (expected_tabs == 0)
      silent_launch = true;
  }

  if (command_line.HasSwitch(switches::kAutomationClientChannelID)) {
    std::string automation_channel_id = command_line.GetSwitchValueASCII(
        switches::kAutomationClientChannelID);
    // If there are any extra parameters, we expect each one to generate a
    // new tab; if there are none then we have no tabs
    std::vector<GURL> urls_to_open = GetURLsFromCommandLine(
        command_line, cur_dir, last_used_profile);
    size_t expected_tabs =
        std::max(static_cast<int>(urls_to_open.size()), 0);
    if (expected_tabs == 0)
      silent_launch = true;

    if (command_line.HasSwitch(switches::kChromeFrame)) {
#if !defined(USE_AURA)
      if (!CreateAutomationProvider<ChromeFrameAutomationProvider>(
          automation_channel_id, last_used_profile, expected_tabs))
        return false;
#endif
    } else {
      if (!CreateAutomationProvider<AutomationProvider>(
          automation_channel_id, last_used_profile, expected_tabs))
        return false;
    }
  }
#endif  // defined(ENABLE_AUTOMATION)

  // If we are just displaying a print dialog we shouldn't open browser
  // windows.
  if (command_line.HasSwitch(switches::kCloudPrintFile) &&
      print_dialog_cloud::CreatePrintDialogFromCommandLine(command_line)) {
    silent_launch = true;
  }

  // If we are checking the proxy enabled policy, don't open any windows.
  if (command_line.HasSwitch(switches::kCheckCloudPrintConnectorPolicy)) {
    silent_launch = true;
    if (CloudPrintProxyServiceFactory::GetForProfile(last_used_profile)->
        EnforceCloudPrintConnectorPolicyAndQuit())
      // Success, nothing more needs to be done, so return false to stop
      // launching and quit.
      return false;
  }

  if (command_line.HasSwitch(switches::kExplicitlyAllowedPorts)) {
    std::string allowed_ports =
        command_line.GetSwitchValueASCII(switches::kExplicitlyAllowedPorts);
    net::SetExplicitlyAllowedPorts(allowed_ports);
  }

  if (command_line.HasSwitch(switches::kInstallFromWebstore)) {
    int64 start_time = ShowAppInstallUI();
    extensions::StartupHelper helper;
    bool app_installed =
        helper.InstallFromWebstore(command_line, last_used_profile);
    // Nothing more needs to be done if we also don't want to run an app, so
    // return false to stop launching and quit.
    if (!chrome::IsRunningInAppMode())
      return false;

    HideAppInstallUI(start_time);
    if (!app_installed) {
      // TODO(zelidrag): Signal somehow to the session manager that app launch
      // attempt had failed.
      return false;
    }
  }

  if (command_line.HasSwitch(switches::kLimitedInstallFromWebstore)) {
    extensions::StartupHelper helper;
    helper.LimitedInstallFromWebstore(command_line, last_used_profile,
                                      base::Bind(&base::DoNothing));
  }

#if defined(OS_CHROMEOS)
  // The browser will be launched after the user logs in.
  if (command_line.HasSwitch(switches::kLoginManager) ||
      command_line.HasSwitch(switches::kLoginPassword)) {
    silent_launch = true;
  }
#endif

#if defined(TOOLKIT_VIEWS) && defined(OS_LINUX)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif

  // If we don't want to launch a new browser window or tab (in the case
  // of an automation request), we are done here.
  if (silent_launch)
    return true;

  // Check for --load-and-launch-app.
  if (command_line.HasSwitch(switches::kLoadAndLaunchApp) &&
      !IncognitoModePrefs::ShouldLaunchIncognito(
          command_line, last_used_profile->GetPrefs())) {
    CommandLine::StringType path = command_line.GetSwitchValueNative(
        switches::kLoadAndLaunchApp);
    extensions::UnpackedInstaller::Create(
        last_used_profile->GetExtensionService())->
            LoadFromCommandLine(base::FilePath(path), true);
    // Return early here since we don't want to open a browser window.
    // The exception is when there are no browser windows, since we don't want
    // chrome to shut down.
    // TODO(jackhou): Do this properly once keep-alive is handled by the
    // background page of apps. Tracked at http://crbug.com/175381
    if (chrome::GetTotalBrowserCountForProfile(last_used_profile) != 0)
      return true;
  }

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
    if (!browser_creator->LaunchBrowser(command_line, last_used_profile,
                                        cur_dir, is_process_startup,
                                        is_first_run, return_code)) {
      return false;
    }
  } else {
    // Launch the last used profile with the full command line, and the other
    // opened profiles without the URLs to launch.
    CommandLine command_line_without_urls(command_line.GetProgram());
    const CommandLine::SwitchMap& switches = command_line.GetSwitches();
    for (CommandLine::SwitchMap::const_iterator switch_it = switches.begin();
         switch_it != switches.end(); ++switch_it) {
      command_line_without_urls.AppendSwitchNative(switch_it->first,
                                                   switch_it->second);
    }
    // Launch the profiles in the order they became active.
    for (Profiles::const_iterator it = last_opened_profiles.begin();
         it != last_opened_profiles.end(); ++it) {
      // Don't launch additional profiles which would only open a new tab
      // page. When restarting after an update, all profiles will reopen last
      // open pages.
      SessionStartupPref startup_pref =
          GetSessionStartupPref(command_line, *it);
      if (*it != last_used_profile &&
          startup_pref.type == SessionStartupPref::DEFAULT &&
          !HasPendingUncleanExit(*it))
        continue;
      if (!browser_creator->LaunchBrowser((*it == last_used_profile) ?
          command_line : command_line_without_urls, *it, cur_dir,
          is_process_startup, is_first_run, return_code))
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

#if !defined(OS_CHROMEOS)
int64 StartupBrowserCreator::ShowAppInstallUI() {
  return base::TimeTicks::Now().ToInternalValue();
}

void StartupBrowserCreator::HideAppInstallUI(
    int64 start_time) {
}
#endif

template <class AutomationProviderClass>
bool StartupBrowserCreator::CreateAutomationProvider(
    const std::string& channel_id,
    Profile* profile,
    size_t expected_tabs) {
#if defined(ENABLE_AUTOMATION)
  scoped_refptr<AutomationProviderClass> automation =
      new AutomationProviderClass(profile);
  if (!automation->InitializeChannel(channel_id))
    return false;
  automation->SetExpectedTabCount(expected_tabs);

  AutomationProviderList* list = g_browser_process->GetAutomationProviderList();
  DCHECK(list);
  list->AddProvider(automation);
#endif  // defined(ENABLE_AUTOMATION)

  return true;
}

// static
void StartupBrowserCreator::ProcessCommandLineOnProfileCreated(
    const CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    ProcessCmdLineImpl(command_line, cur_dir, false, profile, Profiles(), NULL,
                       NULL);
}

// static
void StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
    const CommandLine& command_line,
    const base::FilePath& cur_dir,
    const base::FilePath& profile_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(profile_path);

  // The profile isn't loaded yet and so needs to be loaded asynchronously.
  if (!profile) {
    profile_manager->CreateProfileAsync(profile_path,
        base::Bind(&StartupBrowserCreator::ProcessCommandLineOnProfileCreated,
                   command_line, cur_dir), string16(), string16(), false);
    return;
  }

  ProcessCmdLineImpl(command_line, cur_dir, false, profile, Profiles(), NULL,
                     NULL);
}

// static
bool StartupBrowserCreator::ActivatedProfile() {
  return profile_launch_observer.Get().activated_profile();
}

bool HasPendingUncleanExit(Profile* profile) {
  return profile->GetLastSessionExitType() == Profile::EXIT_CRASHED &&
      !profile_launch_observer.Get().HasBeenLaunched(profile);
}
