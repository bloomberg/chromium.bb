// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_main_parts.h"

#include "base/base_switches.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/time/default_tick_clock.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/language_usage_metrics/language_usage_metrics.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/profiler/ios/ios_tracking_synchronizer_delegate.h"
#include "components/metrics/profiler/tracking_synchronizer.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_http_header_provider.h"
#include "ios/chrome/browser/about_flags.h"
#include "ios/chrome/browser/application_context_impl.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/install_time_util.h"
#include "ios/chrome/browser/ios_chrome_field_trials.h"
#include "ios/chrome/browser/metrics/field_trial_synchronizer.h"
#include "ios/chrome/browser/open_from_clipboard/create_clipboard_recent_content.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/translate/translate_service_ios.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/web/public/web_thread.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_stream_factory.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(ENABLE_RLZ)
#include "components/rlz/rlz_tracker.h"
#include "ios/chrome/browser/rlz/rlz_tracker_delegate_impl.h"
#endif

IOSChromeMainParts::IOSChromeMainParts(
    const base::CommandLine& parsed_command_line)
    : parsed_command_line_(parsed_command_line), local_state_(nullptr) {
  // Chrome disallows cookies by default. All code paths that want to use
  // cookies need to go through one of Chrome's URLRequestContexts which have
  // a ChromeNetworkDelegate attached that selectively allows cookies again.
  net::URLRequest::SetDefaultCookiePolicyToBlock();
}

IOSChromeMainParts::~IOSChromeMainParts() {}

void IOSChromeMainParts::PreMainMessageLoopStart() {
  l10n_util::OverrideLocaleWithCocoaLocale();
  const std::string loaded_locale =
      ResourceBundle::InitSharedInstanceWithLocale(
          std::string(), nullptr, ResourceBundle::LOAD_COMMON_RESOURCES);
  CHECK(!loaded_locale.empty());

  base::FilePath resources_pack_path;
  PathService::Get(ios::FILE_RESOURCES_PACK, &resources_pack_path);
  ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::SCALE_FACTOR_100P);
}

void IOSChromeMainParts::PreCreateThreads() {
  base::FilePath local_state_path;
  CHECK(PathService::Get(ios::FILE_LOCAL_STATE, &local_state_path));
  scoped_refptr<base::SequencedTaskRunner> local_state_task_runner =
      JsonPrefStore::GetTaskRunnerForFile(local_state_path,
                                          web::WebThread::GetBlockingPool());
  application_context_.reset(new ApplicationContextImpl(
      local_state_task_runner.get(), parsed_command_line_,
      l10n_util::GetLocaleOverride()));
  DCHECK_EQ(application_context_.get(), GetApplicationContext());

  // Check the first run state early; this must be done before IO is disallowed
  // so that later calls can use the cached value. (The return value is ignored
  // because this is only to trigger the internal lookup and caching for later
  // use.)
  FirstRun::IsChromeFirstRun();

  // Initialize local state.
  local_state_ = application_context_->GetLocalState();
  DCHECK(local_state_);

  flags_ui::PrefServiceFlagsStorage flags_storage_(
      application_context_->GetLocalState());
  ConvertFlagsToSwitches(&flags_storage_,
                         base::CommandLine::ForCurrentProcess());

  // Initialize tracking synchronizer system.
  tracking_synchronizer_ = new metrics::TrackingSynchronizer(
      make_scoped_ptr(new base::DefaultTickClock()),
      base::Bind(&metrics::IOSTrackingSynchronizerDelegate::Create));

  // Now the command line has been mutated based on about:flags, we can setup
  // metrics and initialize field trials that are needed by IOSChromeIOThread's
  // initialization which happens in ApplicationContext:PreCreateThreads.
  SetUpMetricsAndFieldTrials();

  // Initialize FieldTrialSynchronizer system.
  field_trial_synchronizer_.reset(new ios::FieldTrialSynchronizer);

  application_context_->PreCreateThreads();
}

void IOSChromeMainParts::PreMainMessageLoopRun() {
  // Now that the file thread has been started, start recording.
  StartMetricsRecording();

  application_context_->PreMainMessageLoopRun();

  // ContentSettingsPattern need to be initialized before creating the
  // ChromeBrowserState.
  ContentSettingsPattern::SetNonWildcardDomainNonPortScheme(
      kDummyExtensionScheme);

  // Ensure ClipboadRecentContentIOS is created.
  ClipboardRecentContent::SetInstance(
      CreateClipboardRecentContentIOS().release());

  // Ensure that the browser state is initialized.
  ios::GetChromeBrowserProvider()->AssertBrowserContextKeyedFactoriesBuilt();
  ios::ChromeBrowserStateManager* browser_state_manager =
      application_context_->GetChromeBrowserStateManager();
  ios::ChromeBrowserState* last_used_browser_state =
      browser_state_manager->GetLastUsedBrowserState();

#if defined(ENABLE_RLZ)
  // Init the RLZ library. This just schedules a task on the file thread to be
  // run sometime later. If this is the first run we record the installation
  // event.
  int ping_delay = last_used_browser_state->GetPrefs()->GetInteger(
      FirstRun::GetPingDelayPrefName());
  // Negative ping delay means to send ping immediately after a first search is
  // recorded.
  rlz::RLZTracker::SetRlzDelegate(make_scoped_ptr(new RLZTrackerDelegateImpl));
  rlz::RLZTracker::InitRlzDelayed(
      FirstRun::IsChromeFirstRun(), ping_delay < 0,
      base::TimeDelta::FromMilliseconds(abs(ping_delay)),
      RLZTrackerDelegateImpl::IsGoogleDefaultSearch(last_used_browser_state),
      RLZTrackerDelegateImpl::IsGoogleHomepage(last_used_browser_state),
      RLZTrackerDelegateImpl::IsGoogleInStartpages(last_used_browser_state));
#endif  // defined(ENABLE_RLZ)

  TranslateServiceIOS::Initialize();
  language_usage_metrics::LanguageUsageMetrics::RecordAcceptLanguages(
      last_used_browser_state->GetPrefs()->GetString(prefs::kAcceptLanguages));
  language_usage_metrics::LanguageUsageMetrics::RecordApplicationLanguage(
      application_context_->GetApplicationLocale());

  // Request new variations seed information from server.
  variations::VariationsService* variations_service =
      application_context_->GetVariationsService();
  if (variations_service) {
    variations_service->set_policy_pref_service(
        last_used_browser_state->GetPrefs());
    variations_service->StartRepeatedVariationsSeedFetch();
  }

  translate::TranslateDownloadManager::RequestLanguageList(
      last_used_browser_state->GetPrefs());
}

void IOSChromeMainParts::PostMainMessageLoopRun() {
  TranslateServiceIOS::Shutdown();
  application_context_->StartTearDown();
}

void IOSChromeMainParts::PostDestroyThreads() {
  application_context_->PostDestroyThreads();
}

// This will be called after the command-line has been mutated by about:flags
void IOSChromeMainParts::SetUpMetricsAndFieldTrials() {
  // Must initialize metrics after labs have been converted into switches,
  // but before field trials are set up (so that client ID is available for
  // one-time randomized field trials).
  metrics::MetricsService* metrics = application_context_->GetMetricsService();

  // Initialize FieldTrialList to support FieldTrials that use one-time
  // randomization.
  field_trial_list_.reset(
      new base::FieldTrialList(metrics->CreateEntropyProvider().release()));

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  // Ensure any field trials specified on the command line are initialized.
  // Also stop the metrics service so that we don't pollute UMA.
  if (command_line->HasSwitch(switches::kForceFieldTrials)) {
    // Create field trials without activating them, so that this behaves in a
    // consistent manner with field trials created from the server.
    bool result = base::FieldTrialList::CreateTrialsFromString(
        command_line->GetSwitchValueASCII(switches::kForceFieldTrials),
        base::FieldTrialList::DONT_ACTIVATE_TRIALS, std::set<std::string>());
    CHECK(result) << "Invalid --" << switches::kForceFieldTrials
                  << " list specified.";
  }
  if (command_line->HasSwitch(switches::kIOSForceVariationIds)) {
    // Create default variation ids which will always be included in the
    // X-Client-Data request header.
    variations::VariationsHttpHeaderProvider* provider =
        variations::VariationsHttpHeaderProvider::GetInstance();
    bool result = provider->SetDefaultVariationIds(
        command_line->GetSwitchValueASCII(switches::kIOSForceVariationIds));
    CHECK(result) << "Invalid --" << switches::kIOSForceVariationIds
                  << " list specified.";
  }
  scoped_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      command_line->GetSwitchValueASCII(switches::kEnableIOSFeatures),
      command_line->GetSwitchValueASCII(switches::kDisableIOSFeatures));

  variations::VariationsService* variations_service =
      application_context_->GetVariationsService();
  if (variations_service)
    variations_service->CreateTrialsFromSeed(feature_list.get());

  base::FeatureList::SetInstance(std::move(feature_list));

  SetupFieldTrials(parsed_command_line_,
                   base::Time::FromTimeT(metrics->GetInstallDate()));

  // Now that field trials have been created, initializes metrics recording.
  metrics->InitializeMetricsRecordingState();
}

void IOSChromeMainParts::StartMetricsRecording() {
  bool wifiOnly = local_state_->GetBoolean(prefs::kMetricsReportingWifiOnly);
  bool isConnectionCellular = net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType());
  bool mayUpload = !wifiOnly || !isConnectionCellular;

  application_context_->GetMetricsServicesManager()->UpdateUploadPermissions(
      mayUpload);
}
