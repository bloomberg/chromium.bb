// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/debugger.h"
#include "base/debug/leak_annotations.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop_current.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/hang_watcher.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/active_use_util.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chrome_browser_field_trials.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/component_updater/registration.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/webrtc_log_util.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/expired_histograms_array.h"
#include "chrome/browser/metrics/renderer_uptime_tracker.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/nacl_host/nacl_browser_delegate_impl.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/performance_monitor/process_monitor.h"
#include "chrome/browser/performance_monitor/system_monitor.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/pref_metrics_service.h"
#include "chrome/browser/process_singleton.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/chrome_serialized_navigation_driver.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/site_isolation/site_isolation_policy.h"
#include "chrome/browser/startup_data.h"
#include "chrome/browser/tracing/background_tracing_field_trial.h"
#include "chrome/browser/tracing/trace_event_system_stats_monitor.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/javascript_dialogs/chrome_javascript_app_modal_dialog_view_factory.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/browser/ui/startup/bad_flags_prompt.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/uma_browsing_activity_observer.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/media/media_resource_provider.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiler/stack_sampling_configuration.h"
#include "chrome/common/profiler/thread_profiler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/device_event_log/device_event_log.h"
#include "components/embedder_support/switches.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/google/core/common/google_util.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_experiments.h"
#include "components/language_usage_metrics/language_usage_metrics.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/metrics/expired_histogram_util.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/common/buildflags.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_store.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_crash_keys.h"
#include "components/variations/variations_http_header_provider.h"
#include "components/variations/variations_switches.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/system_connector.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/profiling.h"
#include "device/vr/buildflags/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/localized_strings.h"
#include "media/media_buildflags.h"
#include "net/base/net_module.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_stream_factory.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "third_party/blink/public/common/experiments/memory_ablation_experiment.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/metrics/thread_watcher_android.h"
#include "chrome/browser/ui/page_info/chrome_page_info_client.h"
#include "ui/base/resource/resource_bundle_android.h"
#else
#include "chrome/browser/resource_coordinator/tab_activity_watcher.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/usb/web_usb_detector.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/policy/chrome_browser_cloud_management_controller.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/stats_reporting_controller.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/arc/metrics/stability_metrics_manager.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "chrome/browser/first_run/upgrade_util_linux.h"
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

#if defined(OS_LINUX)
#include "components/crash/core/app/breakpad_linux.h"
#include "components/crash/core/app/crashpad.h"
#endif

#if defined(OS_MACOSX)
#include <Security/Security.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/mac/keystone_glue.h"
#endif  // defined(OS_MACOSX)

// TODO(port): several win-only methods have been pulled out of this, but
// BrowserMain() as a whole needs to be broken apart so that it's usable by
// other platforms. For now, it's just a stub. This is a serious work in
// progress and should not be taken as an indication of a real refactoring.

#if defined(OS_WIN)
#include "base/trace_event/trace_event_etw_export_win.h"
#include "base/win/win_util.h"
#include "chrome/browser/chrome_browser_main_win.h"
#include "chrome/browser/first_run/upgrade_util_win.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/ui/views/try_chrome_dialog_win/try_chrome_dialog.h"
#include "chrome/browser/win/browser_util.h"
#include "chrome/browser/win/chrome_select_file_dialog_factory.h"
#include "chrome/browser/win/parental_controls.h"
#include "chrome/install_static/install_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile_activity_metrics_recorder.h"
#endif

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/startup_helper.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/components/javascript_dialog_extensions_client/javascript_dialog_extension_client_impl.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/browser/nacl_process_host.h"
#endif  // BUILDFLAG(ENABLE_NACL)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_info_handler.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/plugins/plugin_prefs.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)
#include "printing/printed_document.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && defined(OS_WIN)
#include "components/printing/browser/printer_capabilities.h"
#include "printing/backend/win_helper.h"
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OS_CHROMEOS)
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service_factory.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "components/rlz/rlz_tracker.h"
#endif  // BUILDFLAG(ENABLE_RLZ)

#if BUILDFLAG(ENABLE_VR) && defined(OS_WIN)
#include "chrome/browser/vr/consent/xr_session_request_consent_manager_impl.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

using content::BrowserThread;

namespace {

#if !defined(OS_ANDROID)
// Holds the RunLoop for the non-Android MainMessageLoopRun() to Run().
base::RunLoop* g_run_loop = nullptr;
#endif

// This function provides some ways to test crash and assertion handling
// behavior of the program.
void HandleTestParameters(const base::CommandLine& command_line) {
  // This parameter causes a null pointer crash (crash reporter trigger).
  if (command_line.HasSwitch(switches::kBrowserCrashTest)) {
    int* bad_pointer = NULL;
    *bad_pointer = 0;
  }
}

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
void AddFirstRunNewTabs(StartupBrowserCreator* browser_creator,
                        const std::vector<GURL>& new_tabs) {
  for (auto it = new_tabs.begin(); it != new_tabs.end(); ++it) {
    if (it->is_valid())
      browser_creator->AddFirstRunTab(*it);
  }
}
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

// Initializes the primary profile, possibly doing some user prompting to pick
// a fallback profile. Returns the newly created profile, or NULL if startup
// should not continue.
Profile* CreatePrimaryProfile(const content::MainFunctionParams& parameters,
                              const base::FilePath& user_data_dir,
                              const base::CommandLine& parsed_command_line) {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::CreateProfile")

  base::Time start = base::Time::Now();

  bool set_last_used_profile = false;

// If the browser is launched due to activation on Windows native
// notification, the profile id encoded in the notification launch id should
// be chosen over all others.
#if defined(OS_WIN)
  std::string last_used_profile_id =
      NotificationLaunchId::GetNotificationLaunchProfileId(parsed_command_line);
  if (!last_used_profile_id.empty()) {
    profiles::SetLastUsedProfile(last_used_profile_id);
    set_last_used_profile = true;
  }
#endif  // defined(OS_WIN)

  bool profile_dir_specified =
      profiles::IsMultipleProfilesEnabled() &&
      parsed_command_line.HasSwitch(switches::kProfileDirectory);
  if (!set_last_used_profile && profile_dir_specified) {
    profiles::SetLastUsedProfile(
        parsed_command_line.GetSwitchValueASCII(switches::kProfileDirectory));
    set_last_used_profile = true;
  }

  if (set_last_used_profile) {
    // Clear kProfilesLastActive since the user only wants to launch a specific
    // profile.
    ListPrefUpdate update(g_browser_process->local_state(),
                          prefs::kProfilesLastActive);
    base::ListValue* profile_list = update.Get();
    profile_list->Clear();
  }

  Profile* profile = nullptr;
#if defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // On ChromeOS and Android the ProfileManager will use the same path as the
  // one we got passed. CreateInitialProfile will therefore use the correct path
  // automatically.
  DCHECK_EQ(user_data_dir.value(),
            g_browser_process->profile_manager()->user_data_dir().value());
  profile = ProfileManager::CreateInitialProfile();

  // TODO(port): fix this. See comments near the definition of |user_data_dir|.
  // It is better to CHECK-fail here than it is to silently exit because of
  // missing code in the above test.
  CHECK(profile) << "Cannot get default profile.";

#else
  profile = GetStartupProfile(user_data_dir, parsed_command_line);

  if (!profile && !set_last_used_profile)
    profile = GetFallbackStartupProfile();

  if (!profile) {
    ProfileErrorType error_type =
        profile_dir_specified ? ProfileErrorType::CREATE_FAILURE_SPECIFIED
                              : ProfileErrorType::CREATE_FAILURE_ALL;

    // TODO(lwchkg): What diagnostics do you want to include in the feedback
    // report when an error occurs?
    ShowProfileErrorDialog(error_type, IDS_COULDNT_STARTUP_PROFILE_ERROR,
                           "Error creating primary profile.");
    return nullptr;
  }
#endif  // defined(OS_CHROMEOS) || defined(OS_ANDROID)

  UMA_HISTOGRAM_LONG_TIMES(
      "Startup.CreateFirstProfile", base::Time::Now() - start);
  return profile;
}

#if defined(OS_MACOSX)
OSStatus KeychainCallback(SecKeychainEvent keychain_event,
                          SecKeychainCallbackInfo* info, void* context) {
  return noErr;
}
#endif  // defined(OS_MACOSX)

#if !defined(OS_ANDROID)
void ProcessSingletonNotificationCallbackImpl(
    const base::CommandLine& command_line,
    const base::FilePath& current_directory) {
  // Drop the request if the browser process is already shutting down.
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return;

  g_browser_process->platform_part()->PlatformSpecificCommandLineProcessing(
      command_line);

  base::FilePath user_data_dir =
      g_browser_process->profile_manager()->user_data_dir();
  base::FilePath startup_profile_dir =
      GetStartupProfilePath(user_data_dir, command_line);

  StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
      command_line, current_directory, startup_profile_dir);

  // Record now as the last successful chrome start.
  if (ShouldRecordActiveUse(command_line))
    GoogleUpdateSettings::SetLastRunTime();
}

bool ProcessSingletonNotificationCallback(
    const base::CommandLine& command_line,
    const base::FilePath& current_directory) {
  // Drop the request if the browser process is already shutting down.
  // Note that we're going to post an async task below. Even if the browser
  // process isn't shutting down right now, it could be by the time the task
  // starts running. So, an additional check needs to happen when it starts.
  // But regardless of any future check, there is no reason to post the task
  // now if we know we're already shutting down.
  if (!g_browser_process || g_browser_process->IsShuttingDown())
    return false;

  // In order to handle this request on Windows, there is platform specific
  // code in browser_finder.cc that requires making outbound COM calls to
  // cross-apartment shell objects (via IVirtualDesktopManager). That is not
  // allowed within a SendMessage handler, which this function is a part of.
  // So, we post a task to asynchronously finish the command line processing.
  return base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ProcessSingletonNotificationCallbackImpl,
                                command_line, current_directory));
}
#endif  // !defined(OS_ANDROID)

class ScopedMainMessageLoopRunEvent {
 public:
  ScopedMainMessageLoopRunEvent() {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "toplevel", "ChromeBrowserMainParts::MainMessageLoopRun", this);
  }
  ~ScopedMainMessageLoopRunEvent() {
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "toplevel", "ChromeBrowserMainParts::MainMessageLoopRun", this);
  }
};

}  // namespace

// BrowserMainParts ------------------------------------------------------------

ChromeBrowserMainParts::ChromeBrowserMainParts(
    const content::MainFunctionParams& parameters,
    StartupData* startup_data)
    : parameters_(parameters),
      parsed_command_line_(parameters.command_line),
      result_code_(service_manager::RESULT_CODE_NORMAL_EXIT),
      should_call_pre_main_loop_start_startup_on_variations_service_(
          !parameters.ui_task),
      profile_(NULL),
      run_message_loop_(true),
      startup_data_(startup_data) {
  DCHECK(startup_data_);
  // If we're running tests (ui_task is non-null).
  if (parameters.ui_task)
    browser_defaults::enable_help_app = false;

#if !defined(OS_ANDROID)
  shutdown_watcher_ = std::make_unique<ShutdownWatcherHelper>();
#endif  // !defined(OS_ANDROID)
}

ChromeBrowserMainParts::~ChromeBrowserMainParts() {
  for (int i = static_cast<int>(chrome_extra_parts_.size())-1; i >= 0; --i)
    delete chrome_extra_parts_[i];
  chrome_extra_parts_.clear();
}

void ChromeBrowserMainParts::SetupMetrics() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::SetupMetrics");
  metrics::MetricsService* metrics = browser_process_->metrics_service();
  metrics->synthetic_trial_registry()->AddSyntheticTrialObserver(
      variations::VariationsHttpHeaderProvider::GetInstance());
  metrics->synthetic_trial_registry()->AddSyntheticTrialObserver(
      variations::SyntheticTrialsActiveGroupIdProvider::GetInstance());
  // Now that field trials have been created, initializes metrics recording.
  metrics->InitializeMetricsRecordingState();

  startup_data_->chrome_feature_list_creator()
      ->browser_field_trials()
      ->RegisterSyntheticTrials();
}

// static
void ChromeBrowserMainParts::StartMetricsRecording() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::StartMetricsRecording");

#if defined(OS_ANDROID)
  // Android updates the metrics service dynamically depending on whether the
  // application is in the foreground or not. Do not start here unless
  // kUmaBackgroundSessions is enabled.
  if (!base::FeatureList::IsEnabled(chrome::android::kUmaBackgroundSessions))
    return;
#endif

  g_browser_process->metrics_service()->CheckForClonedInstall();

#if defined(OS_WIN)
  // The last live timestamp is used to assess whether a browser crash occurred
  // due to a full system crash. Update the last live timestamp on a slow
  // schedule to get the bast possible accuracy for the assessment.
  g_browser_process->metrics_service()->StartUpdatingLastLiveTimestamp();
#endif

  // Register a synthetic field trial for the sampling profiler configuration
  // that was already chosen.
  std::string trial_name, group_name;
  if (StackSamplingConfiguration::Get()->GetSyntheticFieldTrial(&trial_name,
                                                                &group_name)) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(trial_name,
                                                              group_name);
  }

  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(true);
}

void ChromeBrowserMainParts::RecordBrowserStartupTime() {
  // Don't record any metrics if UI was displayed before this point e.g.
  // warning dialogs or browser was started in background mode.
  if (startup_metric_utils::WasMainWindowStartupInterrupted())
    return;

  bool is_first_run = false;
#if !defined(OS_ANDROID)
  // On Android, first run is handled in Java code, and the C++ side of Chrome
  // doesn't know if this is the first run. This will cause some inaccuracy in
  // the UMA statistics, but this should be minor (first runs are rare).
  is_first_run = first_run::IsChromeFirstRun();
#endif  // defined(OS_ANDROID)

  // Record collected startup metrics.
  startup_metric_utils::RecordBrowserMainMessageLoopStart(
      base::TimeTicks::Now(), is_first_run);
}

void ChromeBrowserMainParts::SetupOriginTrialsCommandLine(
    PrefService* local_state) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(embedder_support::kOriginTrialPublicKey)) {
    std::string new_public_key =
        local_state->GetString(prefs::kOriginTrialPublicKey);
    if (!new_public_key.empty()) {
      command_line->AppendSwitchASCII(
          embedder_support::kOriginTrialPublicKey,
          local_state->GetString(prefs::kOriginTrialPublicKey));
    }
  }
  if (!command_line->HasSwitch(
          embedder_support::kOriginTrialDisabledFeatures)) {
    const base::ListValue* override_disabled_feature_list =
        local_state->GetList(prefs::kOriginTrialDisabledFeatures);
    if (override_disabled_feature_list) {
      std::vector<base::StringPiece> disabled_features;
      base::StringPiece disabled_feature;
      for (const auto& item : *override_disabled_feature_list) {
        if (item.GetAsString(&disabled_feature)) {
          disabled_features.push_back(disabled_feature);
        }
      }
      if (!disabled_features.empty()) {
        const std::string override_disabled_features =
            base::JoinString(disabled_features, "|");
        command_line->AppendSwitchASCII(
            embedder_support::kOriginTrialDisabledFeatures,
            override_disabled_features);
      }
    }
  }
  if (!command_line->HasSwitch(embedder_support::kOriginTrialDisabledTokens)) {
    const base::ListValue* disabled_token_list =
        local_state->GetList(prefs::kOriginTrialDisabledTokens);
    if (disabled_token_list) {
      std::vector<base::StringPiece> disabled_tokens;
      base::StringPiece disabled_token;
      for (const auto& item : *disabled_token_list) {
        if (item.GetAsString(&disabled_token)) {
          disabled_tokens.push_back(disabled_token);
        }
      }
      if (!disabled_tokens.empty()) {
        const std::string disabled_token_switch =
            base::JoinString(disabled_tokens, "|");
        command_line->AppendSwitchASCII(
            embedder_support::kOriginTrialDisabledTokens,
            disabled_token_switch);
      }
    }
  }
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

int ChromeBrowserMainParts::PreEarlyInitialization() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreEarlyInitialization");
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreEarlyInitialization();

  // Create BrowserProcess in PreEarlyInitialization() so that we can load
  // field trials (and all it depends upon).
  browser_process_ = std::make_unique<BrowserProcessImpl>(startup_data_);

  bool failed_to_load_resource_bundle = false;
  const int load_local_state_result =
      OnLocalStateLoaded(&failed_to_load_resource_bundle);

  // Reuses the MetricsServicesManager and GetMetricsServicesManagerClient
  // instances created in the FeatureListCreator so they won't be created
  // again.
  auto* chrome_feature_list_creator =
      startup_data_->chrome_feature_list_creator();
  browser_process_->SetMetricsServices(
      chrome_feature_list_creator->TakeMetricsServicesManager(),
      chrome_feature_list_creator->GetMetricsServicesManagerClient());

  if (load_local_state_result == chrome::RESULT_CODE_MISSING_DATA &&
      failed_to_load_resource_bundle) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kNoErrorDialogs)) {
      return chrome::RESULT_CODE_MISSING_DATA;
    }
    // Continue on and show error later (once UI has been initialized and main
    // message loop is running).
    return service_manager::RESULT_CODE_NORMAL_EXIT;
  }
  return load_local_state_result;
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

#if !defined(OS_ANDROID)
  // Initialize the upgrade detector here after ChromeBrowserMainPartsChromeos
  // has had a chance to connect the DBus services.
  UpgradeDetector::GetInstance()->Init();
#endif

  ThreadProfiler::SetMainThreadTaskRunner(base::ThreadTaskRunnerHandle::Get());

  system_monitor_ = performance_monitor::SystemMonitor::Create();

  // TODO(sebmarchand): Allow this to be created earlier if startup tracing is
  // enabled.
  trace_event_system_stats_monitor_ =
      std::make_unique<tracing::TraceEventSystemStatsMonitor>();

  // device_event_log must be initialized after the message loop. Calls to
  // {DEVICE}_LOG prior to here will only be logged with VLOG. Some
  // platforms (e.g. chromeos) may have already initialized this.
  if (!device_event_log::IsInitialized())
    device_event_log::Initialize(0 /* default max entries */);

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostMainMessageLoopStart();
}

int ChromeBrowserMainParts::PreCreateThreads() {
  // IMPORTANT
  // Calls in this function should not post tasks or create threads as
  // components used to handle those tasks are not yet available. This work
  // should be deferred to PreMainMessageLoopRunImpl.

  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreCreateThreads");
  result_code_ = PreCreateThreadsImpl();

  if (result_code_ == service_manager::RESULT_CODE_NORMAL_EXIT) {
    // These members must be initialized before exiting this function normally.
#if !defined(OS_ANDROID)
    DCHECK(browser_creator_.get());
#endif
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
    DCHECK(master_prefs_.get());
#endif

    for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
      chrome_extra_parts_[i]->PreCreateThreads();
  }

  // Create an instance of GpuModeManager to watch gpu mode pref change.
  g_browser_process->gpu_mode_manager();

  return result_code_;
}

int ChromeBrowserMainParts::OnLocalStateLoaded(
    bool* failed_to_load_resource_bundle) {
  *failed_to_load_resource_bundle = false;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_))
    return chrome::RESULT_CODE_MISSING_DATA;

#if defined(OS_WIN)
  if (first_run::IsChromeFirstRun()) {
    bool stats_default;
    if (GoogleUpdateSettings::GetCollectStatsConsentDefault(&stats_default)) {
      // |stats_default| == true means that the default state of consent for the
      // product at the time of install was to report usage statistics, meaning
      // "opt-out".
      metrics::RecordMetricsReportingDefaultState(
          browser_process_->local_state(),
          stats_default ? metrics::EnableMetricsDefault::OPT_OUT
                        : metrics::EnableMetricsDefault::OPT_IN);
    }
  }
#endif  // defined(OS_WIN)

  std::string locale =
      startup_data_->chrome_feature_list_creator()->actual_locale();
  if (locale.empty()) {
    *failed_to_load_resource_bundle = true;
    return chrome::RESULT_CODE_MISSING_DATA;
  }
  browser_process_->SetApplicationLocale(locale);

  const int apply_first_run_result = ApplyFirstRunPrefs();
  if (apply_first_run_result != service_manager::RESULT_CODE_NORMAL_EXIT)
    return apply_first_run_result;

  SetupOriginTrialsCommandLine(browser_process_->local_state());

  metrics::EnableExpiryChecker(chrome_metrics::kExpiredHistogramsHashes,
                               chrome_metrics::kNumExpiredHistograms);

  return service_manager::RESULT_CODE_NORMAL_EXIT;
}

int ChromeBrowserMainParts::ApplyFirstRunPrefs() {
// Android does first run in Java instead of native.
// Chrome OS has its own out-of-box-experience code.
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  master_prefs_ = std::make_unique<first_run::MasterPrefs>();

  std::unique_ptr<installer::MasterPreferences> installer_master_prefs =
      startup_data_->chrome_feature_list_creator()->TakeMasterPrefs();
  if (!installer_master_prefs)
    return service_manager::RESULT_CODE_NORMAL_EXIT;

  // On first run, we need to process the predictor preferences before the
  // browser's profile_manager object is created, but after ResourceBundle
  // is initialized.
  first_run::ProcessMasterPreferencesResult pmp_result =
      first_run::ProcessMasterPreferences(user_data_dir_,
                                          std::move(installer_master_prefs),
                                          master_prefs_.get());
  if (pmp_result == first_run::EULA_EXIT_NOW)
    return chrome::RESULT_CODE_EULA_REFUSED;

  // TODO(macourteau): refactor preferences that are copied from
  // master_preferences into local_state, as a "local_state" section in
  // master preferences. If possible, a generic solution would be preferred
  // over a copy one-by-one of specific preferences. Also see related TODO
  // in first_run.h.

  PrefService* local_state = g_browser_process->local_state();
  if (!master_prefs_->suppress_default_browser_prompt_for_version.empty()) {
    local_state->SetString(
        prefs::kBrowserSuppressDefaultBrowserPrompt,
        master_prefs_->suppress_default_browser_prompt_for_version);
  }

#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  return service_manager::RESULT_CODE_NORMAL_EXIT;
}

int ChromeBrowserMainParts::PreCreateThreadsImpl() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreCreateThreadsImpl")
  run_message_loop_ = false;

  if (browser_process_->GetApplicationLocale().empty()) {
    ShowMissingLocaleMessageBox();
    return chrome::RESULT_CODE_MISSING_DATA;
  }

#if !defined(OS_ANDROID)
  chrome::MaybeShowInvalidUserDataDirWarningDialog();
#endif  // !defined(OS_ANDROID)

  DCHECK(!user_data_dir_.empty());

  // Force MediaCaptureDevicesDispatcher to be created on UI thread.
  MediaCaptureDevicesDispatcher::GetInstance();

  // Android's first run is done in Java instead of native.
#if !defined(OS_ANDROID)
  process_singleton_.reset(new ChromeProcessSingleton(
      user_data_dir_, base::Bind(&ProcessSingletonNotificationCallback)));

  // Cache first run state early.
  first_run::IsChromeFirstRun();

#endif  // !defined(OS_ANDROID)

  PrefService* local_state = browser_process_->local_state();

#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Initialize(local_state);
  chromeos::StatsReportingController::Initialize(local_state);
  arc::StabilityMetricsManager::Initialize(local_state);
#endif  // defined(OS_CHROMEOS)

  {
    TRACE_EVENT0(
        "startup",
        "ChromeBrowserMainParts::PreCreateThreadsImpl:InitBrowserProcessImpl");
    browser_process_->Init();
  }

#if !defined(OS_ANDROID)
  // Create the RunLoop for MainMessageLoopRun() to use, and pass a copy of
  // its QuitClosure to the BrowserProcessImpl to call when it is time to exit.
  DCHECK(!g_run_loop);
  g_run_loop = new base::RunLoop;

  // These members must be initialized before returning from this function.
  // Android doesn't use StartupBrowserCreator.
  browser_creator_.reset(new StartupBrowserCreator);
  // TODO(yfriedman): Refactor Android to re-use UMABrowsingActivityObserver
  chrome::UMABrowsingActivityObserver::Init();
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)
  // This is needed to enable ETW exporting. This is only relevant for the
  // browser process, as other processes enable it separately.
  base::trace_event::TraceEventETWExport::EnableETWExport();
#endif  // OS_WIN

  // Reset the command line in the crash report details, since we may have
  // just changed it to include experiments.
  crash_keys::SetCrashKeysFromCommandLine(
      *base::CommandLine::ForCurrentProcess());

  browser_process_->browser_policy_connector()->OnResourceBundleCreated();

// Android does first run in Java instead of native.
// Chrome OS has its own out-of-box-experience code.
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  if (first_run::IsChromeFirstRun()) {
    if (!parsed_command_line().HasSwitch(switches::kApp) &&
        !parsed_command_line().HasSwitch(switches::kAppId) &&
        !parsed_command_line().HasSwitch(switches::kShowAppList)) {
      AddFirstRunNewTabs(browser_creator_.get(), master_prefs_->new_tabs);
    }

#if defined(OS_MACOSX) || defined(OS_LINUX)
    // Create directory for user-level Native Messaging manifest files. This
    // makes it less likely that the directory will be created by third-party
    // software with incorrect owner or permission. See crbug.com/725513 .
    base::FilePath user_native_messaging_dir;
    CHECK(base::PathService::Get(chrome::DIR_USER_NATIVE_MESSAGING,
                                 &user_native_messaging_dir));
    if (!base::PathExists(user_native_messaging_dir))
      base::CreateDirectory(user_native_messaging_dir);
#endif  // defined(OS_MACOSX) || defined(OS_LINUX)
  }
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_LINUX) || defined(OS_OPENBSD)
  // Set the product channel for crash reports.
  if (!crash_reporter::IsCrashpadEnabled()) {
    breakpad::SetChannelCrashKey(chrome::GetChannelName());
  }
#endif  // defined(OS_LINUX) || defined(OS_OPENBSD)

#if defined(OS_MACOSX)
  // Get the Keychain API to register for distributed notifications on the main
  // thread, which has a proper CFRunloop, instead of later on the I/O thread,
  // which doesn't. This ensures those notifications will get delivered
  // properly. See issue 37766.
  // (Note that the callback mask here is empty. I don't want to register for
  // any callbacks, I just want to initialize the mechanism.)
  SecKeychainAddCallback(&KeychainCallback, 0, NULL);
#endif  // defined(OS_MACOSX)

#if BUILDFLAG(ENABLE_VR) && defined(OS_WIN)
  vr::XRSessionRequestConsentManager::SetInstance(
      new vr::XRSessionRequestConsentManagerImpl());
#endif  // BUILDFLAG(ENABLE_VR) && OS_WIN

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
  metrics::DesktopSessionDurationTracker::Initialize();
  ProfileActivityMetricsRecorder::Initialize();
#endif
  metrics::RendererUptimeTracker::Initialize();

  // Add Site Isolation switches as dictated by policy.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (local_state->GetBoolean(prefs::kSitePerProcess) &&
      SiteIsolationPolicy::IsEnterprisePolicyApplicable() &&
      !command_line->HasSwitch(switches::kSitePerProcess)) {
    command_line->AppendSwitch(switches::kSitePerProcess);
  }
  // IsolateOrigins policy is taken care of through SiteIsolationPrefsObserver
  // (constructed and owned by BrowserProcessImpl).

#if defined(OS_ANDROID)
  // The admin should also be able to use these policies to force Site Isolation
  // off (on Android; using enterprise policies to disable Site Isolation is not
  // supported on other platforms).  Note that disabling either SitePerProcess
  // or IsolateOrigins via policy will disable both types of isolation.
  if ((local_state->IsManagedPreference(prefs::kSitePerProcess) &&
       !local_state->GetBoolean(prefs::kSitePerProcess)) ||
      (local_state->IsManagedPreference(prefs::kIsolateOrigins) &&
       local_state->GetString(prefs::kIsolateOrigins).empty())) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableSiteIsolationForPolicy);
  }
#endif

  // ChromeOS needs ui::ResourceBundle::InitSharedInstance to be called before
  // this.
  browser_process_->PreCreateThreads(parsed_command_line());

  // This must occur in PreCreateThreads() because it initializes global state
  // which is then read by all threads without synchronization. It must be after
  // browser_process_->PreCreateThreads() as that instantiates the IOThread
  // which is used in SetupMetrics().
  SetupMetrics();

  return service_manager::RESULT_CODE_NORMAL_EXIT;
}

void ChromeBrowserMainParts::PostCreateThreads() {
  // This task should be posted after the IO thread starts, and prior to the
  // base version of the function being invoked. It is functionally okay to post
  // this task in method ChromeBrowserMainParts::BrowserThreadsStarted() which
  // we also need to add in this class, and call this method at the very top of
  // BrowserMainLoop::InitializeMainThread(). PostCreateThreads is preferred to
  // BrowserThreadsStarted as it matches the PreCreateThreads and CreateThreads
  // stages.
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&ThreadProfiler::StartOnChildThread,
                                metrics::CallStackProfileParams::IO_THREAD));
// Sampling multiple threads might cause overhead on Android and we don't want
// to enable it unless the data is needed.
#if !defined(OS_ANDROID)
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&tracing::TracingSamplerProfiler::CreateOnChildThread));
#endif

  tracing::SetupBackgroundTracingFieldTrial();

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostCreateThreads();
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
  // Ephemeral profiles may have been left behind if the browser crashed.
  g_browser_process->profile_manager()->CleanUpEphemeralProfiles();
  // Files of deleted profiles can also be left behind after a crash.
  g_browser_process->profile_manager()->CleanUpDeletedProfiles();
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  javascript_dialog_extensions_client::InstallClient();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  InstallChromeJavaScriptAppModalDialogViewFactory();
}

void ChromeBrowserMainParts::PostProfileInit() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostProfileInit");

  g_browser_process->CreateDevToolsProtocolHandler();
  if (parsed_command_line().HasSwitch(::switches::kAutoOpenDevToolsForTabs))
    g_browser_process->CreateDevToolsAutoOpener();

  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostProfileInit();
}

void ChromeBrowserMainParts::PreBrowserStart() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreBrowserStart");
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PreBrowserStart();

#if !defined(OS_ANDROID)
  // Start the tab manager here so that we give the most amount of time for the
  // other services to start up before we start adjusting the oom priority.
  g_browser_process->GetTabManager()->Start();
#endif

  // The RulesetService will make the filtering rules available to renderers
  // immediately after its construction, provided that the rules are already
  // available at no cost in an indexed format. This enables activating
  // subresource filtering, if needed, also for page loads on start-up.
  g_browser_process->subresource_filter_ruleset_service();
}

void ChromeBrowserMainParts::PostBrowserStart() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostBrowserStart");
  for (size_t i = 0; i < chrome_extra_parts_.size(); ++i)
    chrome_extra_parts_[i]->PostBrowserStart();
#if !defined(OS_ANDROID)
  // Allow ProcessSingleton to process messages.
  process_singleton_->Unlock();
#endif  // !defined(OS_ANDROID)
  // Set up a task to delete old WebRTC log files for all profiles. Use a delay
  // to reduce the impact on startup time.
  base::PostDelayedTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&WebRtcLogUtil::DeleteOldWebRtcLogFilesForAllProfiles),
      base::TimeDelta::FromMinutes(1));

#if !defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kWebUsb)) {
    web_usb_detector_.reset(new WebUsbDetector());
    base::PostTask(
        FROM_HERE,
        {content::BrowserThread::UI, base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&WebUsbDetector::Initialize,
                       base::Unretained(web_usb_detector_.get())));
  }
  if (base::FeatureList::IsEnabled(features::kTabMetricsLogging)) {
    // Initialize the TabActivityWatcher to begin logging tab activity events.
    resource_coordinator::TabActivityWatcher::GetInstance();
  }
#endif

  // At this point, StartupBrowserCreator::Start has run creating initial
  // browser windows and tabs, but no progress has been made in loading
  // content as the main message loop hasn't started processing tasks yet.
  // We setup to observe to the initial page load here to defer running
  // task posted via PostAfterStartupTask until its complete.
  AfterStartupTaskUtils::StartMonitoringStartup();
}

int ChromeBrowserMainParts::PreMainMessageLoopRunImpl() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PreMainMessageLoopRunImpl");

  SCOPED_UMA_HISTOGRAM_LONG_TIMER("Startup.PreMainMessageLoopRunImplLongTime");

  // Can't be in SetupFieldTrials() because it needs a task runner.
  blink::MemoryAblationExperiment::MaybeStart(
      base::CreateSingleThreadTaskRunner({BrowserThread::IO}));

#if defined(OS_WIN)
  // Windows parental controls calls can be slow, so we do an early init here
  // that calculates this value off of the UI thread.
  InitializeWinParentalControls();
#endif

  // Now that the file thread has been started, start metrics.
  StartMetricsRecording();

  if (!base::debug::BeingDebugged()) {
    // Create watchdog thread after creating all other threads because it will
    // watch the other threads and they must be running.
    browser_process_->watchdog_thread();
  }

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

  ui::SelectFileDialog::SetFactory(new ChromeSelectFileDialogFactory());
#endif  // defined(OS_WIN)

  if (parsed_command_line().HasSwitch(switches::kMakeDefaultBrowser)) {
    bool is_managed = g_browser_process->local_state()->IsManagedPreference(
        prefs::kDefaultBrowserSettingEnabled);
    if (is_managed && !g_browser_process->local_state()->GetBoolean(
        prefs::kDefaultBrowserSettingEnabled)) {
      return static_cast<int>(chrome::RESULT_CODE_ACTION_DISALLOWED_BY_POLICY);
    }

    return shell_integration::SetAsDefaultBrowser()
               ? static_cast<int>(service_manager::RESULT_CODE_NORMAL_EXIT)
               : static_cast<int>(chrome::RESULT_CODE_SHELL_INTEGRATION_FAILED);
  }

#if defined(USE_AURA)
  // Make sure aura::Env has been initialized.
  CHECK(aura::Env::GetInstance());
#endif  // defined(USE_AURA)

  // Android doesn't support extensions and doesn't implement ProcessSingleton.
#if !defined(OS_ANDROID)
  // If the command line specifies --pack-extension, attempt the pack extension
  // startup action and exit.
  if (parsed_command_line().HasSwitch(switches::kPackExtension)) {
    extensions::StartupHelper extension_startup_helper;
    if (extension_startup_helper.PackExtension(parsed_command_line()))
      return service_manager::RESULT_CODE_NORMAL_EXIT;
    return chrome::RESULT_CODE_PACK_EXTENSION_ERROR;
  }

  // When another process is running, use that process instead of starting a
  // new one. NotifyOtherProcess will currently give the other process up to
  // 20 seconds to respond. Note that this needs to be done before we attempt
  // to read the profile.
  notify_result_ = process_singleton_->NotifyOtherProcessOrCreate();
  UMA_HISTOGRAM_ENUMERATION("Chrome.ProcessSingleton.NotifyResult",
                            notify_result_,
                            ProcessSingleton::kNumNotifyResults);
  switch (notify_result_) {
    case ProcessSingleton::PROCESS_NONE:
      // No process already running, fall through to starting a new one.
      g_browser_process->platform_part()->PlatformSpecificCommandLineProcessing(
          *base::CommandLine::ForCurrentProcess());
      break;

    case ProcessSingleton::PROCESS_NOTIFIED:
      printf("%s\n", base::SysWideToNativeMB(
                         base::UTF16ToWide(l10n_util::GetStringUTF16(
                             IDS_USED_EXISTING_BROWSER)))
                         .c_str());

      // Having a differentiated return type for testing allows for tests to
      // verify proper handling of some switches. When not testing, stick to
      // the standard Unix convention of returning zero when things went as
      // expected.
      if (parsed_command_line().HasSwitch(switches::kTestType))
        return chrome::RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED;
      return service_manager::RESULT_CODE_NORMAL_EXIT;

    case ProcessSingleton::PROFILE_IN_USE:
      return chrome::RESULT_CODE_PROFILE_IN_USE;

    case ProcessSingleton::LOCK_ERROR:
      LOG(ERROR) << "Failed to create a ProcessSingleton for your profile "
                    "directory. This means that running multiple instances "
                    "would start multiple browser processes rather than "
                    "opening a new window in the existing process. Aborting "
                    "now to avoid profile corruption.";
      return chrome::RESULT_CODE_PROFILE_IN_USE;
  }

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
  // Begin relaunch processing immediately if User Data migration is required
  // to handle a version downgrade.
  if (downgrade_manager_.PrepareUserDataDirectoryForCurrentVersion(
          user_data_dir_)) {
    return chrome::RESULT_CODE_DOWNGRADE_AND_RELAUNCH;
  }
  downgrade_manager_.UpdateLastVersion(user_data_dir_);
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  // Initialize the chrome browser cloud management controller controller after
  // the browser process singleton is acquired to remove race conditions where
  // multiple browser processes start simultaneously.  The main
  // initialization of browser_policy_connector is performed inside
  // PreMainMessageLoopRun() so that policies can be applied as soon as
  // possible.
  //
  // Note that this protects against multiple browser process starts in
  // the same user data dir and not multiple starts across user data dirs.
  browser_process_->browser_policy_connector()
      ->chrome_browser_cloud_management_controller()
      ->Init(browser_process_->local_state(),
             browser_process_->system_network_context_manager()
                 ->GetSharedURLLoaderFactory());

  // Wait for the chrome browser cloud management enrollment to finish.
  // If no enrollment is needed, this function returns immediately.
  // Abort the launch process if the enrollment fails.
  if (!browser_process_->browser_policy_connector()
           ->chrome_browser_cloud_management_controller()
           ->WaitUntilPolicyEnrollmentFinished()) {
    return chrome::RESULT_CODE_CLOUD_POLICY_ENROLLMENT_FAILED;
  }
#endif

  // Handle special early return paths (which couldn't be processed even earlier
  // as they require the process singleton to be held) first.

  std::string try_chrome =
      parsed_command_line().GetSwitchValueASCII(switches::kTryChromeAgain);

  // The TryChromeDialog may be aborted by a rendezvous from another browser
  // process (e.g., a launch via Chrome's taskbar icon or some such). In this
  // case, browser startup should continue without processing the original
  // command line (the one with --try-chrome-again), but rather with the command
  // line from the other process (handled in
  // ProcessSingletonNotificationCallback thanks to the ProcessSingleton). This
  // variable is cleared in that particular case, leading to a bypass of the
  // StartupBrowserCreator.
  bool process_command_line = true;
  if (!try_chrome.empty()) {
#if defined(OS_WIN)
    // Setup.exe has determined that we need to run a retention experiment
    // and has lauched chrome to show the experiment UI. It is guaranteed that
    // no other Chrome is currently running as the process singleton was
    // successfully grabbed above.
    int try_chrome_int;
    base::StringToInt(try_chrome, &try_chrome_int);
    TryChromeDialog::Result answer = TryChromeDialog::Show(
        try_chrome_int,
        base::Bind(&ChromeProcessSingleton::SetModalDialogNotificationHandler,
                   base::Unretained(process_singleton_.get())));
    switch (answer) {
      case TryChromeDialog::NOT_NOW:
        return chrome::RESULT_CODE_NORMAL_EXIT_CANCEL;
      case TryChromeDialog::OPEN_CHROME_WELCOME:
        browser_creator_->set_welcome_back_page(true);
        break;
      case TryChromeDialog::OPEN_CHROME_DEFAULT:
        break;
      case TryChromeDialog::OPEN_CHROME_DEFER:
        process_command_line = false;
        break;
    }
#else
    // We don't support retention experiments on Mac or Linux.
    return service_manager::RESULT_CODE_NORMAL_EXIT;
#endif  // defined(OS_WIN)
  }
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)
  // Do the tasks if chrome has been upgraded while it was last running.
  if (!already_running && upgrade_util::DoUpgradeTasks(parsed_command_line()))
    return service_manager::RESULT_CODE_NORMAL_EXIT;

  // Check if there is any machine level Chrome installed on the current
  // machine. If yes and the current Chrome process is user level, we do not
  // allow the user level Chrome to run. So we notify the user and uninstall
  // user level Chrome.
  // Note this check needs to happen here (after the process singleton was
  // obtained but before potentially creating the first run sentinel).
  if (ChromeBrowserMainPartsWin::CheckMachineLevelInstall())
    return chrome::RESULT_CODE_MACHINE_LEVEL_INSTALL_EXISTS;
#endif  // defined(OS_WIN)

  // Desktop construction occurs here, (required before profile creation).
  PreProfileInit();

  // This step is costly and is already measured in Startup.CreateFirstProfile
  // and more directly Profile.CreateAndInitializeProfile.
  profile_ = CreatePrimaryProfile(parameters(),
                                  user_data_dir_,
                                  parsed_command_line());
  if (!profile_)
    return service_manager::RESULT_CODE_NORMAL_EXIT;

#if !defined(OS_ANDROID)
  // The first run sentinel must be created after the process singleton was
  // grabbed and no early return paths were otherwise hit above.
  first_run::CreateSentinelIfNeeded();
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  // Autoload any profiles which are running background apps.
  // TODO(rlp): Do this on a separate thread. See http://crbug.com/99075.
  browser_process_->profile_manager()->AutoloadProfiles();
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
  // Post-profile init ---------------------------------------------------------

  TranslateService::Initialize();
  if (base::FeatureList::IsEnabled(features::kGeoLanguage) ||
      base::FeatureList::IsEnabled(language::kExplicitLanguageAsk) ||
      language::GetOverrideLanguageModel() ==
          language::OverrideLanguageModel::GEO) {
    language::GeoLanguageProvider::GetInstance()->StartUp(
        browser_process_->local_state());
  }

  // Needs to be done before PostProfileInit, since login manager on CrOS is
  // called inside PostProfileInit.
  content::WebUIControllerFactory::RegisterFactory(
      ChromeWebUIControllerFactory::GetInstance());

#if defined(OS_ANDROID)
  page_info::SetPageInfoClient(new ChromePageInfoClient());
#endif

#if BUILDFLAG(ENABLE_NACL)
  // NaClBrowserDelegateImpl is accessed inside PostProfileInit().
  // So make sure to create it before that.
  nacl::NaClBrowser::SetDelegate(std::make_unique<NaClBrowserDelegateImpl>(
      browser_process_->profile_manager()));
#endif  // BUILDFLAG(ENABLE_NACL)

  // TODO(stevenjb): Move WIN and MACOSX specific code to appropriate Parts.
  // (requires supporting early exit).
  PostProfileInit();

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // Execute first run specific code after the PrefService has been initialized
  // and preferences have been registered since some of the import code depends
  // on preferences.
  if (first_run::IsChromeFirstRun()) {
    first_run::AutoImport(profile_, master_prefs_->import_bookmarks_path);

    // Note: This can pop-up the first run consent dialog on Linux & Mac.
    first_run::DoPostImportTasks(profile_,
                                 master_prefs_->make_chrome_default_for_user);

    // The first run dialog is modal, and spins a RunLoop, which could receive
    // a SIGTERM, and call chrome::AttemptExit(). Exit cleanly in that case.
    if (browser_shutdown::IsTryingToQuit())
      return service_manager::RESULT_CODE_NORMAL_EXIT;
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
  ChromeBrowserMainPartsWin::RegisterApplicationRestart(parsed_command_line());

  // Verify that the profile is not on a network share and if so prepare to show
  // notification to the user.
  if (NetworkProfileBubble::ShouldCheckNetworkProfile(profile_)) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&NetworkProfileBubble::CheckNetworkProfile,
                       profile_->GetPath()));
  }
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_RLZ) && !defined(OS_CHROMEOS)
  // Init the RLZ library. This just binds the dll and schedules a task on the
  // file thread to be run sometime later. If this is the first run we record
  // the installation event.
  int ping_delay =
      profile_->GetPrefs()->GetInteger(prefs::kRlzPingDelaySeconds);
  // Negative ping delay means to send ping immediately after a first search is
  // recorded.
  rlz::RLZTracker::SetRlzDelegate(
      base::WrapUnique(new ChromeRLZTrackerDelegate));
  rlz::RLZTracker::InitRlzDelayed(
      first_run::IsChromeFirstRun(), ping_delay < 0,
      base::TimeDelta::FromSeconds(abs(ping_delay)),
      ChromeRLZTrackerDelegate::IsGoogleDefaultSearch(profile_),
      ChromeRLZTrackerDelegate::IsGoogleHomepage(profile_),
      ChromeRLZTrackerDelegate::IsGoogleInStartpages(profile_));
#endif  // BUILDFLAG(ENABLE_RLZ) && !defined(OS_CHROMEOS)

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(ChromeNetResourceProvider);
  media::SetLocalizedStringProvider(ChromeMediaLocalizedStringProvider);

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

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)
  if (parsed_command_line().HasSwitch(switches::kDebugPrint)) {
    base::FilePath path =
        parsed_command_line().GetSwitchValuePath(switches::kDebugPrint);
    if (!path.empty())
      printing::PrintedDocument::SetDebugDumpPath(path);
  }
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OFFICIAL_BUILD)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && defined(OS_WIN)
  printing::SetGetDisplayNameFunction(&printing::GetUserFriendlyName);
#endif

  HandleTestParameters(parsed_command_line());
  browser_process_->metrics_service()->RecordBreakpadHasDebugger(
      base::debug::BeingDebugged());

  language_usage_metrics::LanguageUsageMetrics::RecordAcceptLanguages(
      profile_->GetPrefs()->GetString(language::prefs::kAcceptLanguages));
  language_usage_metrics::LanguageUsageMetrics::RecordApplicationLanguage(
      browser_process_->GetApplicationLocale());

// On mobile, need for clean shutdown arises only when the application comes
// to foreground (i.e. MetricsService::OnAppEnterForeground is called).
// http://crbug.com/179143
#if !defined(OS_ANDROID)
  // Start watching for a hang.
  browser_process_->metrics_service()->LogNeedForCleanShutdown();
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW) && !defined(OS_CHROMEOS)
  // Create the instance of the cloud print proxy service so that it can launch
  // the service process if needed. This is needed because the service process
  // might have shutdown because an update was available.
  // TODO(torne): this should maybe be done with
  // BrowserContextKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
  // instead?
  CloudPrintProxyServiceFactory::GetForProfile(profile_);
#endif

  // Two different types of hang detection cannot run at the same time or they
  // would interfere with each other.
  if (!base::FeatureList::IsEnabled(base::HangWatcher::kEnableHangWatcher)) {
    // Start watching all browser threads for responsiveness.
    ThreadWatcherList::StartWatchingAll(parsed_command_line());
  }

  // This has to come before the first GetInstance() call. PreBrowserStart()
  // seems like a reasonable place to put this, except on Android,
  // OfflinePageInfoHandler::Register() below calls GetInstance().
  // TODO(thestig): See if the Android code below can be moved to later.
  sessions::ContentSerializedNavigationDriver::SetInstance(
      ChromeSerializedNavigationDriver::GetInstance());

#if defined(OS_ANDROID)
  ThreadWatcherAndroid::RegisterApplicationStatusListener();
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageInfoHandler::Register();
#endif

#if BUILDFLAG(ENABLE_NACL)
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(nacl::NaClProcessHost::EarlyStartup));
#endif  // BUILDFLAG(ENABLE_NACL)

  // Make sure initial prefs are recorded
  PrefMetricsService::Factory::GetForProfile(profile_);

  PreBrowserStart();

  if (!parsed_command_line().HasSwitch(switches::kDisableComponentUpdate)) {
    component_updater::RegisterComponentsForUpdate(profile_->IsOffTheRecord(),
                                                   profile_->GetPrefs());
  }

  variations::VariationsService* variations_service =
      browser_process_->variations_service();
  if (should_call_pre_main_loop_start_startup_on_variations_service_)
    variations_service->PerformPreMainMessageLoopStartup();

#if defined(OS_ANDROID)
  // Just initialize the policy prefs service here. Variations seed fetching
  // will be initialized when the app enters foreground mode.
  variations_service->set_policy_pref_service(profile_->GetPrefs());

#else
  // We are in regular browser boot sequence. Open initial tabs and enter the
  // main message loop.
  std::vector<Profile*> last_opened_profiles;
#if !defined(OS_CHROMEOS)
  // On ChromeOS multiple profiles doesn't apply, and will break if we load
  // them this early as the cryptohome hasn't yet been mounted (which happens
  // only once we log in).
  last_opened_profiles =
      g_browser_process->profile_manager()->GetLastOpenedProfiles();
#endif  // defined(OS_CHROMEOS)

  // This step is costly and is already measured in
  // Startup.StartupBrowserCreator_Start.
  // See the comment above for an explanation of |process_command_line|.
  const bool started =
      !process_command_line ||
      browser_creator_->Start(parsed_command_line(), base::FilePath(), profile_,
                              last_opened_profiles);
  if (started) {
#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))
    // Initialize autoupdate timer. Timer callback costs basically nothing
    // when browser is not in persistent mode, so it's OK to let it ride on
    // the main thread. This needs to be done here because we don't want
    // to start the timer when Chrome is run inside a test harness.
    browser_process_->StartAutoupdateTimer();
#endif  // defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS))

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    // On Linux, the running exe will be updated if an upgrade becomes
    // available while the browser is running.  We need to save the last
    // modified time of the exe, so we can compare to determine if there is
    // an upgrade while the browser is kept alive by a persistent extension.
    upgrade_util::SaveLastModifiedTimeOfExe();
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

    // Record now as the last successful chrome start.
    if (ShouldRecordActiveUse(parsed_command_line()))
      GoogleUpdateSettings::SetLastRunTime();

#if defined(OS_MACOSX)
    // Call Recycle() here as late as possible, before going into the loop
    // because Start() will add things to it while creating the main window.
    if (parameters().autorelease_pool)
      parameters().autorelease_pool->Recycle();
#endif  // defined(OS_MACOSX)

    // Transfer ownership of the browser's lifetime to the BrowserProcess.
    browser_process_->SetQuitClosure(g_run_loop->QuitWhenIdleClosure());
    DCHECK(!run_message_loop_);
    run_message_loop_ = true;
  }
  browser_creator_.reset();
#endif  // !defined(OS_ANDROID)

  PostBrowserStart();

  // The ui_task can be injected by tests to replace the main message loop.
  // In that case we Run() it here, and set a flag to avoid running the main
  // message loop later, as the test will do so as needed from the |ui_task|.
  if (parameters().ui_task) {
    std::move(*parameters().ui_task).Run();
    delete parameters().ui_task;
    run_message_loop_ = false;
  }

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
  // Clean up old user data directory, snapshots and disk cache directory.
  downgrade_manager_.DeleteMovedUserDataSoon(user_data_dir_);
#endif

#if defined(OS_ANDROID)
  // We never run the C++ main loop on Android, since the UI thread message
  // loop is controlled by the OS, so this is as close as we can get to
  // the start of the main loop.
  if (result_code_ <= 0) {
    RecordBrowserStartupTime();
  }
#endif  // defined(OS_ANDROID)

  return result_code_;
}

bool ChromeBrowserMainParts::MainMessageLoopRun(int* result_code) {
  // Trace the entry and exit of this method. We don't use the TRACE_EVENT0
  // macro because the tracing infrastructure doesn't expect a synchronous event
  // around the main loop of a thread.
  ScopedMainMessageLoopRunEvent scoped_main_message_loop_run_event;
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

  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  performance_monitor::ProcessMonitor::GetInstance()->StartGatherCycle();

  g_run_loop->Run();

  return true;
#endif  // defined(OS_ANDROID)
}

void ChromeBrowserMainParts::PostMainMessageLoopRun() {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::PostMainMessageLoopRun");
#if defined(OS_ANDROID)
  // Chrome on Android does not use default MessageLoop. It has its own
  // Android specific MessageLoop
  NOTREACHED();
#else
  // Shutdown the UpgradeDetector here before ChromeBrowserMainPartsChromeos
  // disconnects DBus services in its PostDestroyThreads.
  UpgradeDetector::GetInstance()->Shutdown();

  // Start watching for jank during shutdown. It gets disarmed when
  // |shutdown_watcher_| object is destructed.
  shutdown_watcher_->Arm(base::TimeDelta::FromSeconds(300));

  web_usb_detector_.reset();

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
#endif  // defined(OS_ANDROID)
}

void ChromeBrowserMainParts::PostDestroyThreads() {
#if defined(OS_ANDROID)
  // On Android, there is no quit/exit. So the browser's main message loop will
  // not finish.
  NOTREACHED();
#else
  browser_shutdown::RestartMode restart_mode =
      browser_shutdown::RestartMode::kNoRestart;

  if (restart_last_session_) {
    restart_mode = browser_shutdown::RestartMode::kRestartLastSession;

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
    if (BackgroundModeManager::should_restart_in_background())
      restart_mode = browser_shutdown::RestartMode::kRestartInBackground;
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
  }

  browser_process_->PostDestroyThreads();
  // browser_shutdown takes care of deleting browser_process, so we need to
  // release it.
  ignore_result(browser_process_.release());

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
  if (result_code_ == chrome::RESULT_CODE_DOWNGRADE_AND_RELAUNCH) {
    // Process a pending User Data downgrade before restarting.
    downgrade_manager_.ProcessDowngrade(user_data_dir_);

    // It's impossible for there to also be a user-driven relaunch since the
    // browser never fully starts in this case.
    DCHECK(!restart_last_session_);
    restart_mode = browser_shutdown::RestartMode::kRestartThisSession;
  }
#endif  // BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)

  browser_shutdown::ShutdownPostThreadsStop(restart_mode);

#if !defined(OS_CHROMEOS)
  master_prefs_.reset();
#endif  // !defined(OS_CHROMEOS)

  process_singleton_.reset();
  device_event_log::Shutdown();

  // We need to do this check as late as possible, but due to modularity, this
  // may be the last point in Chrome.  This would be more effective if done at
  // a higher level on the stack, so that it is impossible for an early return
  // to bypass this code.  Perhaps we need a *final* hook that is called on all
  // paths from content/browser/browser_main.
  CHECK(metrics::MetricsService::UmaMetricsProperlyShutdown());

#if defined(OS_CHROMEOS)
  arc::StabilityMetricsManager::Shutdown();
  chromeos::StatsReportingController::Shutdown();
  chromeos::CrosSettings::Shutdown();
#endif  // defined(OS_CHROMEOS)
#endif  // defined(OS_ANDROID)
}

// Public members:

void ChromeBrowserMainParts::AddParts(ChromeBrowserMainExtraParts* parts) {
  chrome_extra_parts_.push_back(parts);
}

#if !defined(OS_ANDROID)
// static
std::unique_ptr<base::RunLoop> ChromeBrowserMainParts::TakeRunLoopForTest() {
  auto run_loop = base::WrapUnique<base::RunLoop>(g_run_loop);
  g_run_loop = nullptr;
  return run_loop;
}
#endif
