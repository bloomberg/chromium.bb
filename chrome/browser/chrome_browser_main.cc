// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main.h"

#include <set>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/debugger.h"
#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_value_store.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/process/process_info.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/component_updater/cld_component_installer.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/component_updater/ev_whitelist_component_installer.h"
#include "chrome/browser/component_updater/flash_component_installer.h"
#include "chrome/browser/component_updater/pnacl/pnacl_component_installer.h"
#include "chrome/browser/component_updater/recovery_component_installer.h"
#include "chrome/browser/component_updater/swiftshader_component_installer.h"
#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/startup_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/google/google_search_counter.h"
#include "chrome/browser/gpu/gl_string_manager.h"
#include "chrome/browser/gpu/three_d_api_observer.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/metrics/field_trial_synchronizer.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/metrics/tracking_synchronizer.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/crl_set_fetcher.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "chrome/browser/performance_monitor/startup_timer.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/pref_service_flags_storage.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/pref_metrics_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/startup/bad_flags_prompt.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/uma_browsing_activity_observer.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiling.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/google/core/browser/google_util.h"
#include "components/language_usage_metrics/language_usage_metrics.h"
#include "components/metrics/metrics_service.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/rappor/rappor_service.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/startup_metric_utils/startup_metric_utils.h"
#include "components/translate/content/common/cld_data_source.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/variations/variations_http_header_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "extensions/browser/extension_protocols.h"
#include "grit/app_locale_settings.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "net/base/net_module.h"
#include "net/base/sdch_manager.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_stream_factory.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "chrome/browser/metrics/thread_watcher_android.h"
#else
#include "chrome/browser/feedback/feedback_profile_observer.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "chrome/browser/first_run/upgrade_util_linux.h"
#include "chrome/browser/sxs_linux.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#endif

// TODO(port): several win-only methods have been pulled out of this, but
// BrowserMain() as a whole needs to be broken apart so that it's usable by
// other platforms. For now, it's just a stub. This is a serious work in
// progress and should not be taken as an indication of a real refactoring.

#if defined(OS_WIN)
#include "base/environment.h"  // For PreRead experiment.
#include "base/win/windows_version.h"
#include "chrome/browser/browser_util_win.h"
#include "chrome/browser/chrome_browser_main_win.h"
#include "chrome/browser/chrome_select_file_dialog_factory_win.h"
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#include "chrome/browser/first_run/try_chrome_dialog_view.h"
#include "chrome/browser/first_run/upgrade_util_win.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/gfx/win/dpi.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
#include <Security/Security.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/mac/keystone_glue.h"
#endif

#if !defined(DISABLE_NACL)
#include "components/nacl/browser/nacl_process_host.h"
#endif

#if defined(ENABLE_FULL_PRINTING) && !defined(OFFICIAL_BUILD)
#include "printing/printed_document.h"
#endif

#if defined(ENABLE_RLZ)
#include "chrome/browser/rlz/rlz.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "chrome/browser/media/webrtc_log_util.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

using content::BrowserThread;

namespace {

// This function provides some ways to test crash and assertion handling
// behavior of the program.
void HandleTestParameters(const CommandLine& command_line) {
  // This parameter causes an assertion.
  if (command_line.HasSwitch(switches::kBrowserAssertTest)) {
    DCHECK(false);
  }

  // This parameter causes a null pointer crash (crash reporter trigger).
  if (command_line.HasSwitch(switches::kBrowserCrashTest)) {
    int* bad_pointer = NULL;
    *bad_pointer = 0;
  }
}

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
void AddFirstRunNewTabs(StartupBrowserCreator* browser_creator,
                        const std::vector<GURL>& new_tabs) {
  for (std::vector<GURL>::const_iterator it = new_tabs.begin();
       it != new_tabs.end(); ++it) {
    if (it->is_valid())
      browser_creator->AddFirstRunTab(*it);
  }
}
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

// Returns the new local state object, guaranteed non-NULL.
// |local_state_task_runner| must be a shutdown-blocking task runner.
PrefService* InitializeLocalState(
    base::SequencedTaskRunner* local_state_task_runner,
    const CommandLine& parsed_command_line) {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::InitializeLocalState")
  base::FilePath local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  bool local_state_file_exists = base::PathExists(local_state_path);

  // Load local state.  This includes the application locale so we know which
  // locale dll to load.  This also causes local state prefs to be registered.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

#if defined(OS_WIN)
  if (first_run::IsChromeFirstRun()) {
    // During first run we read the google_update registry key to find what
    // language the user selected when downloading the installer. This
    // becomes our default language in the prefs.
    // Other platforms obey the system locale.
    base::string16 install_lang;
    if (GoogleUpdateSettings::GetLanguage(&install_lang)) {
      local_state->SetString(prefs::kApplicationLocale,
                             base::UTF16ToASCII(install_lang));
    }
  }
#endif  // defined(OS_WIN)

  // If the local state file for the current profile doesn't exist and the
  // parent profile command line flag is present, then we should inherit some
  // local state from the parent profile.
  // Checking that the local state file for the current profile doesn't exist
  // is the most robust way to determine whether we need to inherit or not
  // since the parent profile command line flag can be present even when the
  // current profile is not a new one, and in that case we do not want to
  // inherit and reset the user's setting.
  //
  // TODO(mnissler): We should probably just instantiate a
  // JSONPrefStore here instead of an entire PrefService. Once this is
  // addressed, the call to browser_prefs::RegisterLocalState can move
  // to chrome_prefs::CreateLocalState.
  if (!local_state_file_exists &&
      parsed_command_line.HasSwitch(switches::kParentProfile)) {
    base::FilePath parent_profile =
        parsed_command_line.GetSwitchValuePath(switches::kParentProfile);
    scoped_refptr<PrefRegistrySimple> registry = new PrefRegistrySimple();
    scoped_ptr<PrefService> parent_local_state(
        chrome_prefs::CreateLocalState(
            parent_profile,
            local_state_task_runner,
            g_browser_process->policy_service(),
            registry,
            false));
    registry->RegisterStringPref(prefs::kApplicationLocale, std::string());
    // Right now, we only inherit the locale setting from the parent profile.
    local_state->SetString(
        prefs::kApplicationLocale,
        parent_local_state->GetString(prefs::kApplicationLocale));
  }

#if defined(OS_CHROMEOS)
  if (parsed_command_line.HasSwitch(chromeos::switches::kLoginManager)) {
    std::string owner_locale = local_state->GetString(prefs::kOwnerLocale);
    // Ensure that we start with owner's locale.
    if (!owner_locale.empty() &&
        local_state->GetString(prefs::kApplicationLocale) != owner_locale &&
        !local_state->IsManagedPreference(prefs::kApplicationLocale)) {
      local_state->SetString(prefs::kApplicationLocale, owner_locale);
    }
  }
#endif

  return local_state;
}

// Initializes the primary profile, possibly doing some user prompting to pick
// a fallback profile. Returns the newly created profile, or NULL if startup
// should not continue.
Profile* CreatePrimaryProfile(const content::MainFunctionParams& parameters,
                              const base::FilePath& user_data_dir,
                              const CommandLine& parsed_command_line) {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::CreateProfile")
  base::Time start = base::Time::Now();
  if (profiles::IsMultipleProfilesEnabled() &&
      parsed_command_line.HasSwitch(switches::kProfileDirectory)) {
    g_browser_process->local_state()->SetString(prefs::kProfileLastUsed,
        parsed_command_line.GetSwitchValueASCII(switches::kProfileDirectory));
    // Clear kProfilesLastActive since the user only wants to launch a specific
    // profile.
    ListPrefUpdate update(g_browser_process->local_state(),
                          prefs::kProfilesLastActive);
    base::ListValue* profile_list = update.Get();
    profile_list->Clear();
  }

  Profile* profile = NULL;
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // On ChromeOS and Android the ProfileManager will use the same path as the
  // one we got passed. GetActiveUserProfile will therefore use the correct path
  // automatically.
  DCHECK_EQ(user_data_dir.value(),
            g_browser_process->profile_manager()->user_data_dir().value());
  profile = ProfileManager::GetActiveUserProfile();
#else
  base::FilePath profile_path =
      GetStartupProfilePath(user_data_dir, parsed_command_line);
  profile = g_browser_process->profile_manager()->GetProfile(
      profile_path);

  // If we're using the --new-profile-management flag and this profile is
  // signed out, then we should show the user manager instead. By switching
  // the active profile to the guest profile we ensure that no
  // browser windows will be opened for the guest profile.
  if (switches::IsNewProfileManagement() && !profile->IsGuestSession()) {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t profile_index = cache.GetIndexOfProfileWithPath(profile_path);

    if (cache.ProfileIsSigninRequiredAtIndex(profile_index))
      profile = g_browser_process->profile_manager()->GetProfile(
          ProfileManager::GetGuestProfilePath());
  }
#endif
  if (profile) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Startup.CreateFirstProfile", base::Time::Now() - start);
    return profile;
  }

#if !defined(OS_WIN)
  // TODO(port): fix this.  See comments near the definition of
  // user_data_dir.  It is better to CHECK-fail here than it is to
  // silently exit because of missing code in the above test.
  CHECK(profile) << "Cannot get default profile.";
#endif

  return NULL;
}

#if defined(OS_MACOSX)
OSStatus KeychainCallback(SecKeychainEvent keychain_event,
                          SecKeychainCallbackInfo* info, void* context) {
  return noErr;
}
#endif

void RegisterComponentsForUpdate() {
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();

  // Registration can be before or after cus->Start() so it is ok to post
  // a task to the UI thread to do registration once you done the necessary
  // file IO to know you existing component version.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  RegisterRecoveryComponent(cus, g_browser_process->local_state());
  RegisterPepperFlashComponent(cus);
  RegisterSwiftShaderComponent(cus);
  RegisterWidevineCdmComponent(cus);
  g_browser_process->pnacl_component_installer()->RegisterPnaclComponent(cus);
#endif

  if (translate::CldDataSource::ShouldRegisterForComponentUpdates()) {
    RegisterCldComponent(cus);
  }

  base::FilePath path;
  if (PathService::Get(chrome::DIR_USER_DATA, &path)) {
#if defined(OS_ANDROID)
    // The CRLSet component was enabled for some releases. This code attempts to
    // delete it from the local disk of those how may have downloaded it.
    g_browser_process->crl_set_fetcher()->DeleteFromDisk(path);
#elif !defined(OS_CHROMEOS)
    // CRLSetFetcher attempts to load a CRL set from either the local disk or
    // network.
    // For Chrome OS this registration is delayed until user login.
    g_browser_process->crl_set_fetcher()->StartInitialLoad(cus, path);
#endif
  }

#if !defined(OS_ANDROID)
  // Android does not currently have the EV indicator. No need to get the
  // EV certs whitelist on Android, then.
  RegisterEVWhitelistComponent(cus);
#endif

#if defined(OS_WIN)
  ExecutePendingSwReporter(cus, g_browser_process->local_state());
#endif

  cus->Start();
}

#if !defined(OS_ANDROID)
bool ProcessSingletonNotificationCallback(
    const CommandLine& command_line,
    const base::FilePath& current_directory) {
  // Drop the request if the browser process is already in shutdown path.
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return false;

  if (command_line.HasSwitch(switches::kOriginalProcessStartTime)) {
    std::string start_time_string =
        command_line.GetSwitchValueASCII(switches::kOriginalProcessStartTime);
    int64 remote_start_time;
    if (base::StringToInt64(start_time_string, &remote_start_time)) {
      base::TimeDelta elapsed =
          base::Time::Now() - base::Time::FromInternalValue(remote_start_time);
      if (command_line.HasSwitch(switches::kFastStart)) {
        UMA_HISTOGRAM_LONG_TIMES(
            "Startup.WarmStartTimeFromRemoteProcessStartFast", elapsed);
      } else {
        UMA_HISTOGRAM_LONG_TIMES(
            "Startup.WarmStartTimeFromRemoteProcessStart", elapsed);
      }
    }
  }

  g_browser_process->platform_part()->PlatformSpecificCommandLineProcessing(
      command_line);

  base::FilePath user_data_dir =
      g_browser_process->profile_manager()->user_data_dir();
  base::FilePath startup_profile_dir =
      GetStartupProfilePath(user_data_dir, command_line);

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, current_directory, startup_profile_dir);
  return true;
}
#endif  // !defined(OS_ANDROID)

void LaunchDevToolsHandlerIfNeeded(const CommandLine& command_line) {
  if (command_line.HasSwitch(::switches::kRemoteDebuggingPort)) {
    std::string port_str =
        command_line.GetSwitchValueASCII(::switches::kRemoteDebuggingPort);
    int port;
    if (base::StringToInt(port_str, &port) && port >= 0 && port < 65535) {
      g_browser_process->CreateDevToolsHttpProtocolHandler(
          chrome::HOST_DESKTOP_TYPE_NATIVE,
          "127.0.0.1",
          port);
    } else {
      DLOG(WARNING) << "Invalid http debugger port number " << port;
    }
  }
}

// Heap allocated class that listens for first page load, kicks off stat
// recording and then deletes itself.
class LoadCompleteListener : public content::NotificationObserver {
 public:
  LoadCompleteListener() {
    registrar_.Add(this,
                   content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                   content::NotificationService::AllSources());
  }
  virtual ~LoadCompleteListener() {}

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME, type);
    startup_metric_utils::OnInitialPageLoadComplete();
    delete this;
  }

 private:
  content::NotificationRegistrar registrar_;
  DISALLOW_COPY_AND_ASSIGN(LoadCompleteListener);
};

}  // namespace

namespace chrome_browser {

// This error message is not localized because we failed to load the
// localization data files.
#if defined(OS_WIN)
const char kMissingLocaleDataTitle[] = "Missing File Error";
#endif

#if defined(OS_WIN)
// TODO(port) This should be used on Linux Aura as well. http://crbug.com/338969
const char kMissingLocaleDataMessage[] =
    "Unable to find locale data files. Please reinstall.";
#endif

}  // namespace chrome_browser

// BrowserMainParts ------------------------------------------------------------

// static
bool ChromeBrowserMainParts::disable_enforcing_cookie_policies_for_tests_ =
    false;

ChromeBrowserMainParts::ChromeBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : parameters_(parameters),
      parsed_command_line_(parameters.command_line),
      result_code_(content::RESULT_CODE_NORMAL_EXIT),
      startup_watcher_(new StartupTimeBomb()),
      shutdown_watcher_(new ShutdownWatcherHelper()),
      startup_timer_(new performance_monitor::StartupTimer()),
      browser_field_trials_(parameters.command_line),
      profile_(NULL),
      run_message_loop_(true),
      notify_result_(ProcessSingleton::PROCESS_NONE),
      local_state_(NULL),
      restart_last_session_(false) {
  // If we're running tests (ui_task is non-null).
  if (parameters.ui_task)
    browser_defaults::enable_help_app = false;

  // Chrome disallows cookies by default. All code paths that want to use
  // cookies need to go through one of Chrome's URLRequestContexts which have
  // a ChromeNetworkDelegate attached that selectively allows cookies again.
  if (!disable_enforcing_cookie_policies_for_tests_)
    net::URLRequest::SetDefaultCookiePolicyToBlock();
}

ChromeBrowserMainParts::~ChromeBrowserMainParts() {
  for (int i = static_cast<int>(chrome_extra_parts_.size())-1; i >= 0; --i)
    delete chrome_extra_parts_[i];
  chrome_extra_parts_.clear();
}

// This will be called after the command-line has been mutated by about:flags
void ChromeBrowserMainParts::SetupMetricsAndFieldTrials() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::SetupMetricsAndFieldTrials");
  // Must initialize metrics after labs have been converted into switches,
  // but before field trials are set up (so that client ID is available for
  // one-time randomized field trials).

  // Initialize FieldTrialList to support FieldTrials that use one-time
  // randomization.
  MetricsService* metrics = browser_process_->metrics_service();
  field_trial_list_.reset(
      new base::FieldTrialList(metrics->CreateEntropyProvider().release()));

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableBenchmarking))
    base::FieldTrial::EnableBenchmarking();

  // Ensure any field trials specified on the command line are initialized.
  if (command_line->HasSwitch(switches::kForceFieldTrials)) {
    std::set<std::string> unforceable_field_trials;
#if defined(OFFICIAL_BUILD)
    unforceable_field_trials.insert("SettingsEnforcement");
#endif  // defined(OFFICIAL_BUILD)

    // Create field trials without activating them, so that this behaves in a
    // consistent manner with field trials created from the server.
    bool result = base::FieldTrialList::CreateTrialsFromString(
        command_line->GetSwitchValueASCII(switches::kForceFieldTrials),
        base::FieldTrialList::DONT_ACTIVATE_TRIALS,
        unforceable_field_trials);
    CHECK(result) << "Invalid --" << switches::kForceFieldTrials
                  << " list specified.";
  }
  if (command_line->HasSwitch(switches::kForceVariationIds)) {
    // Create default variation ids which will always be included in the
    // X-Client-Data request header.
    variations::VariationsHttpHeaderProvider* provider =
        variations::VariationsHttpHeaderProvider::GetInstance();
    bool result = provider->SetDefaultVariationIds(
        command_line->GetSwitchValueASCII(switches::kForceVariationIds));
    CHECK(result) << "Invalid --" << switches::kForceVariationIds
                  << " list specified.";
  }
  chrome_variations::VariationsService* variations_service =
      browser_process_->variations_service();
  if (variations_service)
    variations_service->CreateTrialsFromSeed();

  // This must be called after |local_state_| is initialized.
  browser_field_trials_.SetupFieldTrials(
      base::Time::FromTimeT(metrics->GetInstallDate()), local_state_);

  // Initialize FieldTrialSynchronizer system. This is a singleton and is used
  // for posting tasks via base::Bind. Its deleted when it goes out of scope.
  // Even though base::Bind does AddRef and Release, the object will not be
  // deleted after the Task is executed.
  field_trial_synchronizer_ = new FieldTrialSynchronizer();

  // Now that field trials have been created, initializes metrics recording.
  metrics->InitializeMetricsRecordingState();
}

// ChromeBrowserMainParts: |SetupMetricsAndFieldTrials()| related --------------

void ChromeBrowserMainParts::StartMetricsRecording() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::StartMetricsRecording");
  MetricsService* metrics = g_browser_process->metrics_service();

  const bool only_do_metrics_recording =
      parsed_command_line_.HasSwitch(switches::kMetricsRecordingOnly) ||
      parsed_command_line_.HasSwitch(switches::kEnableBenchmarking);
  if (only_do_metrics_recording) {
    // If we're testing then we don't care what the user preference is, we turn
    // on recording, but not reporting, otherwise tests fail.
    metrics->StartRecordingForTests();
    return;
  }

  metrics->CheckForClonedInstall(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  const bool metrics_enabled = metrics->StartIfMetricsReportingEnabled();
  // TODO(asvitkine): Since this function is not run on Android, RAPPOR is
  // currently disabled there. http://crbug.com/370041
  browser_process_->rappor_service()->Start(
      browser_process_->local_state(),
      browser_process_->system_request_context(),
      metrics_enabled);
}

void ChromeBrowserMainParts::RecordBrowserStartupTime() {
  // Don't record any metrics if UI was displayed before this point e.g.
  // warning dialogs.
  if (startup_metric_utils::WasNonBrowserUIDisplayed())
    return;

#if defined(OS_ANDROID)
  // On Android the first run is handled in Java code, and the C++ side of
  // Chrome doesn't know if this is the first run. This will cause some
  // inaccuracy in the UMA statistics, but this should be minor (first runs are
  // rare).
  bool is_first_run = false;
#else
  bool is_first_run = first_run::IsChromeFirstRun();
#endif

// CurrentProcessInfo::CreationTime() is currently only implemented on some
// platforms.
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
  const base::Time process_creation_time =
      base::CurrentProcessInfo::CreationTime();

  if (!is_first_run && !process_creation_time.is_null()) {
    base::TimeDelta delay = base::Time::Now() - process_creation_time;
    UMA_HISTOGRAM_LONG_TIMES_100("Startup.BrowserMessageLoopStartTime", delay);
  }
#endif  // defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)

  // Record collected startup metrics.
  startup_metric_utils::OnBrowserStartupComplete(is_first_run);

  // Deletes self.
  new LoadCompleteListener();
}

// -----------------------------------------------------------------------------
// TODO(viettrungluu): move more/rest of BrowserMain() into BrowserMainParts.

#if defined(OS_WIN)
#define DLLEXPORT __declspec(dllexport)

// We use extern C for the prototype DLLEXPORT to avoid C++ name mangling.
extern "C" {
DLLEXPORT void __cdecl RelaunchChromeBrowserWithNewCommandLineIfNeeded();
}

DLLEXPORT void __cdecl RelaunchChromeBrowserWithNewCommandLineIfNeeded() {
  // Need an instance of AtExitManager to handle singleton creations and
  // deletions.  We need this new instance because, the old instance created
  // in ChromeMain() got destructed when the function returned.
  base::AtExitManager exit_manager;
  upgrade_util::RelaunchChromeBrowserWithNewCommandLineIfNeeded();
}
#endif

// content::BrowserMainParts implementation ------------------------------------

void ChromeBrowserMainParts::PreEarlyInitialization() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreEarlyInitialization");
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreEarlyInitialization();
}

void ChromeBrowserMainParts::PostEarlyInitialization() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostEarlyInitialization");
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostEarlyInitialization();
}

void ChromeBrowserMainParts::ToolkitInitialized() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::ToolkitInitialized");
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->ToolkitInitialized();
}

void ChromeBrowserMainParts::PreMainMessageLoopStart() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreMainMessageLoopStart");
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreMainMessageLoopStart();
}

void ChromeBrowserMainParts::PostMainMessageLoopStart() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostMainMessageLoopStart");
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostMainMessageLoopStart();
}

int ChromeBrowserMainParts::PreCreateThreads() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreCreateThreads");
  result_code_ = PreCreateThreadsImpl();

  if (result_code_ == content::RESULT_CODE_NORMAL_EXIT) {
#if !defined(OS_ANDROID)
    // These members must be initialized before exiting this function normally.
    DCHECK(master_prefs_.get());
    DCHECK(browser_creator_.get());
#endif
    for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
      chrome_extra_parts_[i]->PreCreateThreads();
  }

  return result_code_;
}

int ChromeBrowserMainParts::PreCreateThreadsImpl() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreCreateThreadsImpl")
  run_message_loop_ = false;
#if !defined(OS_ANDROID)
  chrome::MaybeShowInvalidUserDataDirWarningDialog();
#endif
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_))
    return chrome::RESULT_CODE_MISSING_DATA;

  // Force MediaCaptureDevicesDispatcher to be created on UI thread.
  MediaCaptureDevicesDispatcher::GetInstance();

  // Android's first run is done in Java instead of native.
#if !defined(OS_ANDROID)
  process_singleton_.reset(new ChromeProcessSingleton(
      user_data_dir_, base::Bind(&ProcessSingletonNotificationCallback)));

  // Cache first run state early.
  first_run::IsChromeFirstRun();
#endif

  scoped_refptr<base::SequencedTaskRunner> local_state_task_runner =
      JsonPrefStore::GetTaskRunnerForFile(
          base::FilePath(chrome::kLocalStorePoolName),
          BrowserThread::GetBlockingPool());

  {
    TRACE_EVENT0("startup",
      "ChromeBrowserMainParts::PreCreateThreadsImpl:InitBrowswerProcessImpl");
    browser_process_.reset(new BrowserProcessImpl(local_state_task_runner.get(),
                                                  parsed_command_line()));
  }

  if (parsed_command_line().HasSwitch(switches::kEnableProfiling)) {
    TRACE_EVENT0("startup",
        "ChromeBrowserMainParts::PreCreateThreadsImpl:InitProfiling");
    // User wants to override default tracking status.
    std::string flag =
      parsed_command_line().GetSwitchValueASCII(switches::kEnableProfiling);
    // Default to basic profiling (no parent child support).
    tracked_objects::ThreadData::Status status =
          tracked_objects::ThreadData::PROFILING_ACTIVE;
    if (flag.compare("0") != 0)
      status = tracked_objects::ThreadData::DEACTIVATED;
    else if (flag.compare("child") != 0)
      status = tracked_objects::ThreadData::PROFILING_CHILDREN_ACTIVE;
    tracked_objects::ThreadData::InitializeAndSetTrackingStatus(status);
  }

  if (parsed_command_line().HasSwitch(switches::kProfilingOutputFile)) {
    tracking_objects_.set_output_file_path(
        parsed_command_line().GetSwitchValuePath(
            switches::kProfilingOutputFile));
  }

  local_state_ = InitializeLocalState(
      local_state_task_runner.get(), parsed_command_line());

#if !defined(OS_ANDROID)
  // These members must be initialized before returning from this function.
  master_prefs_.reset(new first_run::MasterPrefs);
  // Android doesn't use StartupBrowserCreator.
  browser_creator_.reset(new StartupBrowserCreator);
  // TODO(yfriedman): Refactor Android to re-use UMABrowsingActivityObserver
  chrome::UMABrowsingActivityObserver::Init();
#endif

#if !defined(OS_CHROMEOS)
  // Convert active labs into switches. This needs to be done before
  // ResourceBundle::InitSharedInstanceWithLocale as some loaded resources are
  // affected by experiment flags (--touch-optimized-ui in particular).
  // On ChromeOS system level flags are applied from the device settings from
  // the session manager.
  {
    TRACE_EVENT0("startup",
        "ChromeBrowserMainParts::PreCreateThreadsImpl:ConvertFlags");
    about_flags::PrefServiceFlagsStorage flags_storage_(
        g_browser_process->local_state());
    about_flags::ConvertFlagsToSwitches(&flags_storage_,
                                        CommandLine::ForCurrentProcess(),
                                        about_flags::kAddSentinels);
  }
#endif

  local_state_->UpdateCommandLinePrefStore(
      new CommandLinePrefStore(CommandLine::ForCurrentProcess()));

  // Reset the command line in the crash report details, since we may have
  // just changed it to include experiments.
  crash_keys::SetSwitchesFromCommandLine(CommandLine::ForCurrentProcess());

  // Mac starts it earlier in |PreMainMessageLoopStart()| (because it is
  // needed when loading the MainMenu.nib and the language doesn't depend on
  // anything since it comes from Cocoa.
#if defined(OS_MACOSX)
  std::string locale =
      parameters().ui_task ? "en-US" : l10n_util::GetLocaleOverride();
  browser_process_->SetApplicationLocale(locale);
#else
  const std::string locale =
      local_state_->GetString(prefs::kApplicationLocale);

  // On a POSIX OS other than ChromeOS, the parameter that is passed to the
  // method InitSharedInstance is ignored.

  TRACE_EVENT_BEGIN0("startup",
      "ChromeBrowserMainParts::PreCreateThreadsImpl:InitResourceBundle");
  const std::string loaded_locale =
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          locale, NULL, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  TRACE_EVENT_END0("startup",
      "ChromeBrowserMainParts::PreCreateThreadsImpl:InitResourceBundle");

  if (loaded_locale.empty() &&
      !parsed_command_line().HasSwitch(switches::kNoErrorDialogs)) {
    ShowMissingLocaleMessageBox();
    return chrome::RESULT_CODE_MISSING_DATA;
  }
  CHECK(!loaded_locale.empty()) << "Locale could not be found for " << locale;
  browser_process_->SetApplicationLocale(loaded_locale);

  base::FilePath resources_pack_path;
  PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
  {
    TRACE_EVENT0("startup",
        "ChromeBrowserMainParts::PreCreateThreadsImpl:AddDataPack");
    ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        resources_pack_path, ui::SCALE_FACTOR_NONE);
  }
#endif  // defined(OS_MACOSX)

  // Android does first run in Java instead of native.
  // Chrome OS has its own out-of-box-experience code.
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // On first run, we need to process the predictor preferences before the
  // browser's profile_manager object is created, but after ResourceBundle
  // is initialized.
  if (first_run::IsChromeFirstRun()) {
    first_run::ProcessMasterPreferencesResult pmp_result =
        first_run::ProcessMasterPreferences(user_data_dir_,
                                            master_prefs_.get());
    if (pmp_result == first_run::EULA_EXIT_NOW)
      return chrome::RESULT_CODE_EULA_REFUSED;

    if (!parsed_command_line().HasSwitch(switches::kApp) &&
        !parsed_command_line().HasSwitch(switches::kAppId) &&
        !parsed_command_line().HasSwitch(switches::kShowAppList)) {
      AddFirstRunNewTabs(browser_creator_.get(), master_prefs_->new_tabs);
    }

    // TODO(macourteau): refactor preferences that are copied from
    // master_preferences into local_state, as a "local_state" section in
    // master preferences. If possible, a generic solution would be preferred
    // over a copy one-by-one of specific preferences. Also see related TODO
    // in first_run.h.

    // Store the initial VariationsService seed in local state, if it exists
    // in master prefs.
    if (!master_prefs_->variations_seed.empty()) {
      local_state_->SetString(prefs::kVariationsSeed,
                              master_prefs_->variations_seed);
      if (!master_prefs_->variations_seed_signature.empty()) {
        local_state_->SetString(prefs::kVariationsSeedSignature,
                                master_prefs_->variations_seed_signature);
      }
      // Set the variation seed date to the current system time. If the user's
      // clock is incorrect, this may cause some field trial expiry checks to
      // not do the right thing until the next seed update from the server,
      // when this value will be updated.
      local_state_->SetInt64(prefs::kVariationsSeedDate,
                             base::Time::Now().ToInternalValue());
    }

    if (!master_prefs_->suppress_default_browser_prompt_for_version.empty()) {
      local_state_->SetString(
          prefs::kBrowserSuppressDefaultBrowserPrompt,
          master_prefs_->suppress_default_browser_prompt_for_version);
    }

    AppListService::Get(chrome::HOST_DESKTOP_TYPE_NATIVE)->HandleFirstRun();
  }
#endif

#if defined(OS_LINUX) || defined(OS_OPENBSD) || defined(OS_MACOSX)
  // Set the product channel for crash reports.
  base::debug::SetCrashKeyValue(crash_keys::kChannel,
      chrome::VersionInfo::GetVersionStringModifier());
#endif

  // Initialize tracking synchronizer system.
  tracking_synchronizer_ = new chrome_browser_metrics::TrackingSynchronizer();

#if defined(OS_MACOSX)
  // Get the Keychain API to register for distributed notifications on the main
  // thread, which has a proper CFRunloop, instead of later on the I/O thread,
  // which doesn't. This ensures those notifications will get delivered
  // properly. See issue 37766.
  // (Note that the callback mask here is empty. I don't want to register for
  // any callbacks, I just want to initialize the mechanism.)
  SecKeychainAddCallback(&KeychainCallback, 0, NULL);
#endif

#if defined(OS_CHROMEOS)
  // Must be done after g_browser_process is constructed, before
  // SetupMetricsAndFieldTrials().
  chromeos::CrosSettings::Initialize();
#endif

  // Now the command line has been mutated based on about:flags, we can setup
  // metrics and initialize field trials. The field trials are needed by
  // IOThread's initialization which happens in BrowserProcess:PreCreateThreads.
  SetupMetricsAndFieldTrials();

  // ChromeOS needs ResourceBundle::InitSharedInstance to be called before this.
  browser_process_->PreCreateThreads();

  return content::RESULT_CODE_NORMAL_EXIT;
}

void ChromeBrowserMainParts::PreMainMessageLoopRun() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreMainMessageLoopRun");
  result_code_ = PreMainMessageLoopRunImpl();

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreMainMessageLoopRun();
}

// PreMainMessageLoopRun calls these extra stages in the following order:
//  PreMainMessageLoopRunImpl()
//   ... initial setup, including browser_process_ setup.
//   PreProfileInit()
//   ... additional setup, including CreateProfile()
//   PostProfileInit()
//   ... additional setup
//   PreBrowserStart()
//   ... browser_creator_->Start (OR parameters().ui_task->Run())
//   PostBrowserStart()

void ChromeBrowserMainParts::PreProfileInit() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreProfileInit");

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreProfileInit();

#if !defined(OS_ANDROID)
  // Initialize the feedback uploader so it can setup notifications for profile
  // creation.
  feedback::FeedbackProfileObserver::Initialize();

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  // First check if any ephemeral profiles are left behind because of browser
  // crash and schedule them for deletion and then proceed with getting the set
  // of profiles to open.
  ProfileInfoCache& profile_cache = profile_manager->GetProfileInfoCache();
  size_t profiles_count = profile_cache.GetNumberOfProfiles();
  std::vector<base::FilePath> profiles_to_delete;
  for (size_t i = 0; i < profiles_count; ++i) {
    if (profile_cache.ProfileIsEphemeralAtIndex(i))
      profiles_to_delete.push_back(profile_cache.GetPathOfProfileAtIndex(i));
  }

  if (profiles_to_delete.size()) {
    for (size_t i = 0; i < profiles_to_delete.size(); ++i) {
      profile_manager->ScheduleProfileForDeletion(
          profiles_to_delete[i], ProfileManager::CreateCallback());
    }
    // Clean up stale profiles immediately after browser start.
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&ProfileManager::CleanUpStaleProfiles, profiles_to_delete));
  }
#endif  // OS_ANDROID
}

void ChromeBrowserMainParts::PostProfileInit() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostProfileInit");
  LaunchDevToolsHandlerIfNeeded(parsed_command_line());
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostProfileInit();
}

void ChromeBrowserMainParts::PreBrowserStart() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreBrowserStart");
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreBrowserStart();

  three_d_observer_.reset(new ThreeDAPIObserver());
}

void ChromeBrowserMainParts::PostBrowserStart() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostBrowserStart");
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostBrowserStart();
#if !defined(OS_ANDROID)
  // Allow ProcessSingleton to process messages.
  process_singleton_->Unlock();
#endif
#if defined(ENABLE_WEBRTC)
  // Set up a task to delete old WebRTC log files for all profiles. Use a delay
  // to reduce the impact on startup time.
  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&WebRtcLogUtil::DeleteOldWebRtcLogFilesForAllProfiles),
      base::TimeDelta::FromMinutes(1));
#endif
}

int ChromeBrowserMainParts::PreMainMessageLoopRunImpl() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreMainMessageLoopRunImpl");
  // Android updates the metrics service dynamically depending on whether the
  // application is in the foreground or not. Do not start here.
#if !defined(OS_ANDROID)
  // Now that the file thread has been started, start recording.
  StartMetricsRecording();
#endif

  // Create watchdog thread after creating all other threads because it will
  // watch the other threads and they must be running.
  browser_process_->watchdog_thread();

  // Do any initializating in the browser process that requires all threads
  // running.
  browser_process_->PreMainMessageLoopRun();

  // Record last shutdown time into a histogram.
  browser_shutdown::ReadLastShutdownInfo();

#if defined(OS_WIN)
  // On Windows, we use our startup as an opportunity to do upgrade/uninstall
  // tasks.  Those care whether the browser is already running.  On Linux/Mac,
  // upgrade/uninstall happen separately.
  bool already_running = browser_util::IsBrowserAlreadyRunning();

  // If the command line specifies 'uninstall' then we need to work here
  // unless we detect another chrome browser running.
  if (parsed_command_line().HasSwitch(switches::kUninstall)) {
    return DoUninstallTasks(already_running);
  }

  if (parsed_command_line().HasSwitch(switches::kHideIcons) ||
      parsed_command_line().HasSwitch(switches::kShowIcons)) {
    return ChromeBrowserMainPartsWin::HandleIconsCommands(
        parsed_command_line_);
  }

  ui::SelectFileDialog::SetFactory(new ChromeSelectFileDialogFactory(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
#endif

  if (parsed_command_line().HasSwitch(switches::kMakeDefaultBrowser)) {
    return ShellIntegration::SetAsDefaultBrowser() ?
        static_cast<int>(content::RESULT_CODE_NORMAL_EXIT) :
        static_cast<int>(chrome::RESULT_CODE_SHELL_INTEGRATION_FAILED);
  }

#if defined(USE_AURA)
  // Make sure aura::Env has been initialized.
  CHECK(aura::Env::GetInstance());
#endif

  // Android doesn't support extensions and doesn't implement ProcessSingleton.
#if !defined(OS_ANDROID)
  // If the command line specifies --pack-extension, attempt the pack extension
  // startup action and exit.
  if (parsed_command_line().HasSwitch(switches::kPackExtension)) {
    extensions::StartupHelper extension_startup_helper;
    if (extension_startup_helper.PackExtension(parsed_command_line()))
      return content::RESULT_CODE_NORMAL_EXIT;
    return chrome::RESULT_CODE_PACK_EXTENSION_ERROR;
  }

  // If we're being launched just to check the connector policy, we are
  // short-lived and don't want to be passing that switch off.
  bool pass_command_line = !parsed_command_line().HasSwitch(
      switches::kCheckCloudPrintConnectorPolicy);

  if (pass_command_line) {
    // When another process is running, use that process instead of starting a
    // new one. NotifyOtherProcess will currently give the other process up to
    // 20 seconds to respond. Note that this needs to be done before we attempt
    // to read the profile.
    notify_result_ = process_singleton_->NotifyOtherProcessOrCreate();
    switch (notify_result_) {
      case ProcessSingleton::PROCESS_NONE:
        // No process already running, fall through to starting a new one.
        break;

      case ProcessSingleton::PROCESS_NOTIFIED:
#if defined(OS_POSIX) && !defined(OS_MACOSX)
        printf("%s\n", base::SysWideToNativeMB(base::UTF16ToWide(
            l10n_util::GetStringUTF16(IDS_USED_EXISTING_BROWSER))).c_str());
#endif
        // Having a differentiated return type for testing allows for tests to
        // verify proper handling of some switches. When not testing, stick to
        // the standard Unix convention of returning zero when things went as
        // expected.
        if (parsed_command_line().HasSwitch(switches::kTestType))
          return chrome::RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED;
        return content::RESULT_CODE_NORMAL_EXIT;

      case ProcessSingleton::PROFILE_IN_USE:
        return chrome::RESULT_CODE_PROFILE_IN_USE;

      case ProcessSingleton::LOCK_ERROR:
        LOG(ERROR) << "Failed to create a ProcessSingleton for your profile "
                      "directory. This means that running multiple instances "
                      "would start multiple browser processes rather than "
                      "opening a new window in the existing process. Aborting "
                      "now to avoid profile corruption.";
        return chrome::RESULT_CODE_PROFILE_IN_USE;

      default:
        NOTREACHED();
    }
  }
#endif  // !defined(OS_ANDROID)

  // Handle special early return paths (which couldn't be processed even earlier
  // as they require the process singleton to be held) first.

  std::string try_chrome =
      parsed_command_line().GetSwitchValueASCII(switches::kTryChromeAgain);
  if (!try_chrome.empty()) {
#if defined(OS_WIN)
    // Setup.exe has determined that we need to run a retention experiment
    // and has lauched chrome to show the experiment UI. It is guaranteed that
    // no other Chrome is currently running as the process singleton was
    // sucessfully grabbed above.
    int try_chrome_int;
    base::StringToInt(try_chrome, &try_chrome_int);
    TryChromeDialogView::Result answer = TryChromeDialogView::Show(
        try_chrome_int,
        base::Bind(&ChromeProcessSingleton::SetActiveModalDialog,
                   base::Unretained(process_singleton_.get())));
    if (answer == TryChromeDialogView::NOT_NOW)
      return chrome::RESULT_CODE_NORMAL_EXIT_CANCEL;
    if (answer == TryChromeDialogView::UNINSTALL_CHROME)
      return chrome::RESULT_CODE_NORMAL_EXIT_EXP2;
    // At this point the user is willing to try chrome again.
    if (answer == TryChromeDialogView::TRY_CHROME_AS_DEFAULT) {
      // Only set in the unattended case, the interactive case is Windows 8.
      if (ShellIntegration::CanSetAsDefaultBrowser() ==
          ShellIntegration::SET_DEFAULT_UNATTENDED)
        ShellIntegration::SetAsDefaultBrowser();
    }
#else
    // We don't support retention experiments on Mac or Linux.
    return content::RESULT_CODE_NORMAL_EXIT;
#endif  // defined(OS_WIN)
  }

#if defined(OS_WIN)
  // Do the tasks if chrome has been upgraded while it was last running.
  if (!already_running && upgrade_util::DoUpgradeTasks(parsed_command_line()))
    return content::RESULT_CODE_NORMAL_EXIT;

  // Check if there is any machine level Chrome installed on the current
  // machine. If yes and the current Chrome process is user level, we do not
  // allow the user level Chrome to run. So we notify the user and uninstall
  // user level Chrome.
  // Note this check needs to happen here (after the process singleton was
  // obtained but before potentially creating the first run sentinel).
  if (ChromeBrowserMainPartsWin::CheckMachineLevelInstall())
    return chrome::RESULT_CODE_MACHINE_LEVEL_INSTALL_EXISTS;
#endif  // defined(OS_WIN)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (sxs_linux::ShouldMigrateUserDataDir())
    return sxs_linux::MigrateUserDataDir();
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

  // Desktop construction occurs here, (required before profile creation).
  PreProfileInit();

  // Profile creation ----------------------------------------------------------

  MetricsService::SetExecutionPhase(MetricsService::CREATE_PROFILE,
                                    g_browser_process->local_state());
  profile_ = CreatePrimaryProfile(parameters(),
                                  user_data_dir_,
                                  parsed_command_line());
  if (!profile_)
    return content::RESULT_CODE_NORMAL_EXIT;

#if !defined(OS_ANDROID)
  // The first run sentinel must be created after the process singleton was
  // grabbed and no early return paths were otherwise hit above.
  first_run::CreateSentinelIfNeeded();
#endif  // !defined(OS_ANDROID)

#if defined(ENABLE_BACKGROUND)
  // Autoload any profiles which are running background apps.
  // TODO(rlp): Do this on a separate thread. See http://crbug.com/99075.
  browser_process_->profile_manager()->AutoloadProfiles();
#endif
  // Post-profile init ---------------------------------------------------------

  TranslateService::Initialize();

  // Needs to be done before PostProfileInit, since login manager on CrOS is
  // called inside PostProfileInit.
  content::WebUIControllerFactory::RegisterFactory(
      ChromeWebUIControllerFactory::GetInstance());

  // NaClBrowserDelegateImpl is accessed inside PostProfileInit().
  // So make sure to create it before that.
#if !defined(DISABLE_NACL)
  NaClBrowserDelegateImpl* delegate =
      new NaClBrowserDelegateImpl(browser_process_->profile_manager());
  nacl::NaClBrowser::SetDelegate(delegate);
#endif

  // TODO(stevenjb): Move WIN and MACOSX specific code to appropriate Parts.
  // (requires supporting early exit).
  PostProfileInit();

  // Retrieve cached GL strings from local state and use them for GPU
  // blacklist decisions.
  if (g_browser_process->gl_string_manager())
    g_browser_process->gl_string_manager()->Initialize();

  // Create an instance of GpuModeManager to watch gpu mode pref change.
  g_browser_process->gpu_mode_manager();

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // Show the First Run UI if this is the first time Chrome has been run on
  // this computer, or we're being compelled to do so by a command line flag.
  // Note that this be done _after_ the PrefService is initialized and all
  // preferences are registered, since some of the code that the importer
  // touches reads preferences.
  if (first_run::IsChromeFirstRun()) {
    first_run::AutoImport(profile_,
                          master_prefs_->homepage_defined,
                          master_prefs_->do_import_items,
                          master_prefs_->dont_import_items,
                          master_prefs_->import_bookmarks_path);

    // Note: this can pop the first run consent dialog on linux.
    first_run::DoPostImportTasks(profile_,
                                 master_prefs_->make_chrome_default_for_user);

    if (!master_prefs_->suppress_first_run_default_browser_prompt) {
      browser_creator_->set_show_main_browser_window(
          !chrome::ShowFirstRunDefaultBrowserPrompt(profile_));
    } else {
      browser_creator_->set_is_default_browser_dialog_suppressed(true);
    }
  }
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_WIN)
  // Sets things up so that if we crash from this point on, a dialog will
  // popup asking the user to restart chrome. It is done this late to avoid
  // testing against a bunch of special cases that are taken care early on.
  ChromeBrowserMainPartsWin::PrepareRestartOnCrashEnviroment(
      parsed_command_line());

  // Registers Chrome with the Windows Restart Manager, which will restore the
  // Chrome session when the computer is restarted after a system update.
  // This could be run as late as WM_QUERYENDSESSION for system update reboots,
  // but should run on startup if extended to handle crashes/hangs/patches.
  // Also, better to run once here than once for each HWND's WM_QUERYENDSESSION.
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    ChromeBrowserMainPartsWin::RegisterApplicationRestart(
        parsed_command_line());
  }

  // Verify that the profile is not on a network share and if so prepare to show
  // notification to the user.
  if (NetworkProfileBubble::ShouldCheckNetworkProfile(profile_)) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        base::Bind(&NetworkProfileBubble::CheckNetworkProfile,
                   profile_->GetPath()));
  }
#endif  // OS_WIN

#if defined(ENABLE_RLZ) && !defined(OS_CHROMEOS)
  // Init the RLZ library. This just binds the dll and schedules a task on the
  // file thread to be run sometime later. If this is the first run we record
  // the installation event.
  PrefService* pref_service = profile_->GetPrefs();
  int ping_delay = first_run::IsChromeFirstRun() ? master_prefs_->ping_delay :
      pref_service->GetInteger(first_run::GetPingDelayPrefName().c_str());
  // Negative ping delay means to send ping immediately after a first search is
  // recorded.
  RLZTracker::InitRlzFromProfileDelayed(
      profile_, first_run::IsChromeFirstRun(), ping_delay < 0,
      base::TimeDelta::FromMilliseconds(abs(ping_delay)));
#endif  // defined(ENABLE_RLZ) && !defined(OS_CHROMEOS)

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(chrome_common_net::NetResourceProvider);

  // In unittest mode, this will do nothing.  In normal mode, this will create
  // the global IntranetRedirectDetector instance, which will promptly go to
  // sleep for seven seconds (to avoid slowing startup), and wake up afterwards
  // to see if it should do anything else.
  //
  // A simpler way of doing all this would be to have some function which could
  // give the time elapsed since startup, and simply have this object check that
  // when asked to initialize itself, but this doesn't seem to exist.
  //
  // This can't be created in the BrowserProcessImpl constructor because it
  // needs to read prefs that get set after that runs.
  browser_process_->intranet_redirect_detector();
  GoogleSearchCounter::RegisterForNotifications();

  if (parsed_command_line().HasSwitch(switches::kEnableSdchOverHttps)) {
    net::SdchManager::EnableSecureSchemeSupport(true);
  } else {
    // Check SDCH field trial.
    const char kSdchFieldTrialName[] = "SDCH";
    const char kEnabledAllGroupName[] = "EnabledAll";
    const char kEnabledHttpOnlyGroupName[] = "EnabledHttpOnly";
    const char kDisabledAllGroupName[] = "DisabledAll";

    base::StringPiece sdch_trial_group =
        base::FieldTrialList::FindFullName(kSdchFieldTrialName);
    if (sdch_trial_group.starts_with(kEnabledAllGroupName)) {
      net::SdchManager::EnableSecureSchemeSupport(true);
      net::SdchManager::EnableSdchSupport(true);
    } else if (sdch_trial_group.starts_with(kEnabledHttpOnlyGroupName)) {
      net::SdchManager::EnableSdchSupport(true);
    } else if (sdch_trial_group.starts_with(kDisabledAllGroupName)) {
      net::SdchManager::EnableSdchSupport(false);
    }
  }

#if defined(ENABLE_FULL_PRINTING) && !defined(OFFICIAL_BUILD)
  if (parsed_command_line().HasSwitch(switches::kDebugPrint)) {
    base::FilePath path =
        parsed_command_line().GetSwitchValuePath(switches::kDebugPrint);
    printing::PrintedDocument::set_debug_dump_path(path);
  }
#endif

  HandleTestParameters(parsed_command_line());
  browser_process_->metrics_service()->RecordBreakpadHasDebugger(
      base::debug::BeingDebugged());

  language_usage_metrics::LanguageUsageMetrics::RecordAcceptLanguages(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
  language_usage_metrics::LanguageUsageMetrics::RecordApplicationLanguage(
      browser_process_->GetApplicationLocale());

  // Start watching for hangs during startup. We disarm this hang detector when
  // ThreadWatcher takes over or when browser is shutdown or when
  // startup_watcher_ is deleted.
  MetricsService::SetExecutionPhase(MetricsService::STARTUP_TIMEBOMB_ARM,
                                    g_browser_process->local_state());
  startup_watcher_->Arm(base::TimeDelta::FromSeconds(300));

  // On mobile, need for clean shutdown arises only when the application comes
  // to foreground (i.e. MetricsService::OnAppEnterForeground is called).
  // http://crbug.com/179143
#if !defined(OS_ANDROID)
  // Start watching for a hang.
  MetricsService::LogNeedForCleanShutdown(g_browser_process->local_state());
#endif

#if defined(ENABLE_FULL_PRINTING)
  // Create the instance of the cloud print proxy service so that it can launch
  // the service process if needed. This is needed because the service process
  // might have shutdown because an update was available.
  // TODO(torne): this should maybe be done with
  // BrowserContextKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
  // instead?
  CloudPrintProxyServiceFactory::GetForProfile(profile_);
#endif

  // Start watching all browser threads for responsiveness.
  MetricsService::SetExecutionPhase(MetricsService::THREAD_WATCHER_START,
                                    g_browser_process->local_state());
  ThreadWatcherList::StartWatchingAll(parsed_command_line());

#if defined(OS_ANDROID)
  ThreadWatcherAndroid::RegisterApplicationStatusListener();
#endif

#if !defined(DISABLE_NACL)
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(nacl::NaClProcessHost::EarlyStartup));
#endif

  // Make sure initial prefs are recorded
  PrefMetricsService::Factory::GetForProfile(profile_);

  PreBrowserStart();

  // Instantiate the notification UI manager, as this triggers a perf timer
  // used to measure startup time. TODO(stevenjb): Figure out what is actually
  // triggering the timer and call that explicitly in the approprate place.
  // http://crbug.com/105065.
  browser_process_->notification_ui_manager();

  if (!parsed_command_line().HasSwitch(switches::kDisableComponentUpdate))
    RegisterComponentsForUpdate();

#if defined(OS_ANDROID)
  chrome_variations::VariationsService* variations_service =
      browser_process_->variations_service();
  if (variations_service) {
    variations_service->set_policy_pref_service(profile_->GetPrefs());
    variations_service->StartRepeatedVariationsSeedFetch();
  }
  translate::TranslateDownloadManager::RequestLanguageList(
      profile_->GetPrefs());

#else
  // Most general initialization is behind us, but opening a
  // tab and/or session restore and such is still to be done.
  base::TimeTicks browser_open_start = base::TimeTicks::Now();

  // We are in regular browser boot sequence. Open initial tabs and enter the
  // main message loop.
  int result_code;
#if defined(OS_CHROMEOS)
  // On ChromeOS multiple profiles doesn't apply, and will break if we load
  // them this early as the cryptohome hasn't yet been mounted (which happens
  // only once we log in.
  std::vector<Profile*> last_opened_profiles;
#else
  std::vector<Profile*> last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
#endif

  if (browser_creator_->Start(parsed_command_line(), base::FilePath(),
                              profile_, last_opened_profiles, &result_code)) {
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
    // Initialize autoupdate timer. Timer callback costs basically nothing
    // when browser is not in persistent mode, so it's OK to let it ride on
    // the main thread. This needs to be done here because we don't want
    // to start the timer when Chrome is run inside a test harness.
    browser_process_->StartAutoupdateTimer();
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    // On Linux, the running exe will be updated if an upgrade becomes
    // available while the browser is running.  We need to save the last
    // modified time of the exe, so we can compare to determine if there is
    // an upgrade while the browser is kept alive by a persistent extension.
    upgrade_util::SaveLastModifiedTimeOfExe();
#endif

    // Record now as the last successful chrome start.
    GoogleUpdateSettings::SetLastRunTime();

#if defined(OS_MACOSX)
    // Call Recycle() here as late as possible, before going into the loop
    // because Start() will add things to it while creating the main window.
    if (parameters().autorelease_pool)
      parameters().autorelease_pool->Recycle();
#endif

    base::TimeDelta delay = base::TimeTicks::Now() - browser_open_start;
    UMA_HISTOGRAM_LONG_TIMES_100("Startup.BrowserOpenTabs", delay);

    // If we're running tests (ui_task is non-null), then we don't want to
    // call RequestLanguageList or StartRepeatedVariationsSeedFetch or
    // RequestLanguageList
    if (parameters().ui_task == NULL) {
      // Request new variations seed information from server.
      chrome_variations::VariationsService* variations_service =
          browser_process_->variations_service();
      if (variations_service) {
        variations_service->StartRepeatedVariationsSeedFetch();

#if defined(OS_WIN)
        variations_service->StartGoogleUpdateRegistrySync();
#endif
      }

      translate::TranslateDownloadManager::RequestLanguageList(
          profile_->GetPrefs());
    }

    run_message_loop_ = true;
  } else {
    run_message_loop_ = false;
  }
  browser_creator_.reset();
#endif  // !defined(OS_ANDROID)

  performance_monitor::PerformanceMonitor::GetInstance()->Initialize();

  PostBrowserStart();

  if (parameters().ui_task) {
    // We end the startup timer here if we have parameters to run, because we
    // never start to run the main loop (where we normally stop the timer).
    startup_timer_->SignalStartupComplete(
        performance_monitor::StartupTimer::STARTUP_TEST);
    parameters().ui_task->Run();
    delete parameters().ui_task;
    run_message_loop_ = false;
  }
#if defined(OS_ANDROID)
  // We never run the C++ main loop on Android, since the UI thread message
  // loop is controlled by the OS, so this is as close as we can get to
  // the start of the main loop
  if (result_code_ <= 0) {
    RecordBrowserStartupTime();
  }
#endif
  return result_code_;
}

bool ChromeBrowserMainParts::MainMessageLoopRun(int* result_code) {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::MainMessageLoopRun");
#if defined(OS_ANDROID)
  // Chrome on Android does not use default MessageLoop. It has its own
  // Android specific MessageLoop
  NOTREACHED();
  return true;
#else
  // Set the result code set in PreMainMessageLoopRun or set above.
  *result_code = result_code_;
  if (!run_message_loop_)
    return true;  // Don't run the default message loop.

  // These should be invoked as close to the start of the browser's
  // UI thread message loop as possible to get a stable measurement
  // across versions.
  RecordBrowserStartupTime();
  startup_timer_->SignalStartupComplete(
      performance_monitor::StartupTimer::STARTUP_NORMAL);

  DCHECK(base::MessageLoopForUI::IsCurrent());
  base::RunLoop run_loop;

  performance_monitor::PerformanceMonitor::GetInstance()->StartGatherCycle();

  MetricsService::SetExecutionPhase(MetricsService::MAIN_MESSAGE_LOOP_RUN,
                                    g_browser_process->local_state());
  run_loop.Run();

  return true;
#endif
}

void ChromeBrowserMainParts::PostMainMessageLoopRun() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostMainMessageLoopRun");
#if defined(OS_ANDROID)
  // Chrome on Android does not use default MessageLoop. It has its own
  // Android specific MessageLoop
  NOTREACHED();
#else

  // Start watching for jank during shutdown. It gets disarmed when
  // |shutdown_watcher_| object is destructed.
  MetricsService::SetExecutionPhase(MetricsService::SHUTDOWN_TIMEBOMB_ARM,
                                    g_browser_process->local_state());
  shutdown_watcher_->Arm(base::TimeDelta::FromSeconds(300));

  // Disarm the startup hang detector time bomb if it is still Arm'ed.
  startup_watcher_->Disarm();

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostMainMessageLoopRun();

  // Some tests don't set parameters.ui_task, so they started translate
  // language fetch that was never completed so we need to cleanup here
  // otherwise it will be done by the destructor in a wrong thread.
  TranslateService::Shutdown(parameters().ui_task == NULL);

  if (notify_result_ == ProcessSingleton::PROCESS_NONE)
    process_singleton_->Cleanup();

  // Stop all tasks that might run on WatchDogThread.
  ThreadWatcherList::StopWatchingAll();

  browser_process_->metrics_service()->Stop();

  restart_last_session_ = browser_shutdown::ShutdownPreThreadsStop();
  browser_process_->StartTearDown();
#endif
}

void ChromeBrowserMainParts::PostDestroyThreads() {
#if defined(OS_ANDROID)
  // On Android, there is no quit/exit. So the browser's main message loop will
  // not finish.
  NOTREACHED();
#else
  browser_process_->PostDestroyThreads();
  // browser_shutdown takes care of deleting browser_process, so we need to
  // release it.
  ignore_result(browser_process_.release());
  browser_shutdown::ShutdownPostThreadsStop(restart_last_session_);
  master_prefs_.reset();
  process_singleton_.reset();

  // We need to do this check as late as possible, but due to modularity, this
  // may be the last point in Chrome.  This would be more effective if done at
  // a higher level on the stack, so that it is impossible for an early return
  // to bypass this code.  Perhaps we need a *final* hook that is called on all
  // paths from content/browser/browser_main.
  CHECK(MetricsService::UmaMetricsProperlyShutdown());

#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Shutdown();
#endif
#endif
}

// Public members:

void ChromeBrowserMainParts::AddParts(ChromeBrowserMainExtraParts* parts) {
  chrome_extra_parts_.push_back(parts);
}
