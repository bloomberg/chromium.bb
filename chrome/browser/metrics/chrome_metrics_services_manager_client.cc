// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"
#include "chrome/browser/metrics/variations/ui_string_overrider_factory.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/install_static/install_util.h"
#include "components/crash/content/app/crashpad.h"
#endif  // OS_WIN

#if defined(OS_CHROMEOS)
#include "chromeos/settings/cros_settings_names.h"
#endif  // defined(OS_CHROMEOS)

namespace {

#if defined(OS_WIN)
// Type for the function pointer to enable and disable crash reporting on
// windows. Needed because the function is loaded from chrome_elf.
typedef void (*SetUploadConsentPointer)(bool);

// The name of the function used to set the uploads enabled state in
// components/crash/content/app/crashpad.cc. This is used to call the function
// exported by the chrome_elf dll.
const char kCrashpadUpdateConsentFunctionName[] = "SetUploadConsentImpl";
#endif  // OS_WIN

// Name of the variations param that defines the sampling rate.
const char kRateParamName[] = "sampling_rate_per_mille";

// Metrics reporting feature. This feature, along with user consent, controls if
// recording and reporting are enabled. If the feature is enabled, but no
// consent is given, then there will be no recording or reporting.
const base::Feature kMetricsReportingFeature{"MetricsReporting",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Posts |GoogleUpdateSettings::StoreMetricsClientInfo| on blocking pool thread
// because it needs access to IO and cannot work from UI thread.
void PostStoreMetricsClientInfo(const metrics::ClientInfo& client_info) {
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BACKGROUND},
      base::BindOnce(&GoogleUpdateSettings::StoreMetricsClientInfo,
                     client_info));
}

// Appends a group to the sampling controlling |trial|. The group will be
// associated with a variation param for reporting sampling |rate| in per mille.
void AppendSamplingTrialGroup(const std::string& group_name,
                              int rate,
                              base::FieldTrial* trial) {
  std::map<std::string, std::string> params = {
      {kRateParamName, base::IntToString(rate)}};
  variations::AssociateVariationParams(trial->trial_name(), group_name, params);
  trial->AppendGroup(group_name, rate);
}

// Only clients that were given an opt-out metrics-reporting consent flow are
// eligible for sampling.
bool IsClientEligibleForSampling() {
  return metrics::GetMetricsReportingDefaultState(
             g_browser_process->local_state()) ==
         metrics::EnableMetricsDefault::OPT_OUT;
}

#if defined(OS_CHROMEOS)
// Callback to update the metrics reporting state when the Chrome OS metrics
// reporting setting changes.
void OnCrosMetricsReportingSettingChange() {
  bool enable_metrics = false;
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &enable_metrics);
  ChangeMetricsReportingState(enable_metrics);
}
#endif

}  // namespace


class ChromeMetricsServicesManagerClient::ChromeEnabledStateProvider
    : public metrics::EnabledStateProvider {
 public:
  ChromeEnabledStateProvider() {}
  ~ChromeEnabledStateProvider() override {}

  bool IsConsentGiven() override {
    return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  }

  bool IsReportingEnabled() override {
    return IsConsentGiven() &&
           ChromeMetricsServicesManagerClient::IsClientInSample();
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeEnabledStateProvider);
};

ChromeMetricsServicesManagerClient::ChromeMetricsServicesManagerClient(
    PrefService* local_state)
    : enabled_state_provider_(new ChromeEnabledStateProvider()),
      local_state_(local_state) {
  DCHECK(local_state);

#if defined(OS_CHROMEOS)
  cros_settings_observer_ = chromeos::CrosSettings::Get()->AddSettingsObserver(
      chromeos::kStatsReportingPref,
      base::Bind(&OnCrosMetricsReportingSettingChange));
  // Invoke the callback once initially to set the metrics reporting state.
  OnCrosMetricsReportingSettingChange();
#endif
}

ChromeMetricsServicesManagerClient::~ChromeMetricsServicesManagerClient() {}

// static
void ChromeMetricsServicesManagerClient::CreateFallbackSamplingTrial(
    version_info::Channel channel,
    base::FeatureList* feature_list) {
  // The trial name must be kept in sync with the server config controlling
  // sampling. If they don't match, then clients will be shuffled into different
  // groups when the server config takes over from the fallback trial.
  static const char kTrialName[] = "MetricsAndCrashSampling";
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kTrialName, 1000, "Default", base::FieldTrialList::kNoExpirationYear,
          1, 1, base::FieldTrial::ONE_TIME_RANDOMIZED, nullptr));

  // On all channels except stable, we sample out at a minimal rate to ensure
  // the code paths are exercised in the wild before hitting stable.
  int sampled_in_rate = 990;
  int sampled_out_rate = 10;
  if (channel == version_info::Channel::STABLE) {
    sampled_in_rate = 100;
    sampled_out_rate = 900;
  }

  // Like the trial name, the order that these two groups are added to the trial
  // must be kept in sync with the order that they appear in the server config.

  // 100 per-mille sampling rate group.
  static const char kInSampleGroup[] = "InReportingSample";
  AppendSamplingTrialGroup(kInSampleGroup, sampled_in_rate, trial.get());

  // 900 per-mille sampled out.
  static const char kSampledOutGroup[] = "OutOfReportingSample";
  AppendSamplingTrialGroup(kSampledOutGroup, sampled_out_rate, trial.get());

  // Setup the feature.
  const std::string& group_name = trial->GetGroupNameWithoutActivation();
  feature_list->RegisterFieldTrialOverride(
      kMetricsReportingFeature.name,
      group_name == kSampledOutGroup
          ? base::FeatureList::OVERRIDE_DISABLE_FEATURE
          : base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial.get());
}

// static
bool ChromeMetricsServicesManagerClient::IsClientInSample() {
  // Only some clients are eligible for sampling. Clients that aren't eligible
  // will always be considered "in sample". In this case, we don't want the
  // feature state queried, because we don't want the field trial that controls
  // sampling to be reported as active.
  if (!IsClientEligibleForSampling())
    return true;

  return base::FeatureList::IsEnabled(kMetricsReportingFeature);
}

// static
bool ChromeMetricsServicesManagerClient::GetSamplingRatePerMille(int* rate) {
  // The population that is NOT eligible for sampling in considered "in sample",
  // but does not have a defined sample rate.
  if (!IsClientEligibleForSampling())
    return false;

  std::string rate_str = variations::GetVariationParamValueByFeature(
      kMetricsReportingFeature, kRateParamName);
  if (rate_str.empty())
    return false;

  if (!base::StringToInt(rate_str, rate) || *rate > 1000)
    return false;

  return true;
}

std::unique_ptr<rappor::RapporServiceImpl>
ChromeMetricsServicesManagerClient::CreateRapporServiceImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::MakeUnique<rappor::RapporServiceImpl>(
      local_state_, base::Bind(&chrome::IsIncognitoSessionActive));
}

std::unique_ptr<variations::VariationsService>
ChromeMetricsServicesManagerClient::CreateVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return variations::VariationsService::Create(
      base::MakeUnique<ChromeVariationsServiceClient>(), local_state_,
      GetMetricsStateManager(), switches::kDisableBackgroundNetworking,
      chrome_variations::CreateUIStringOverrider());
}

std::unique_ptr<metrics::MetricsServiceClient>
ChromeMetricsServicesManagerClient::CreateMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ChromeMetricsServiceClient::Create(GetMetricsStateManager());
}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
ChromeMetricsServicesManagerClient::CreateEntropyProvider() {
  return GetMetricsStateManager()->CreateDefaultEntropyProvider();
}

net::URLRequestContextGetter*
ChromeMetricsServicesManagerClient::GetURLRequestContext() {
  return g_browser_process->system_request_context();
}

bool ChromeMetricsServicesManagerClient::IsMetricsReportingEnabled() {
  return enabled_state_provider_->IsReportingEnabled();
}

#if defined(OS_WIN)
void ChromeMetricsServicesManagerClient::UpdateRunningServices(
    bool may_record,
    bool may_upload) {
  // First, set the registry value so that Crashpad will have the sampling state
  // now and for subsequent runs.
  install_static::SetCollectStatsInSample(IsClientInSample());

  // Next, get Crashpad to pick up the sampling state for this session.

  // The crash reporting is handled by chrome_elf.dll.
  HMODULE elf_module = GetModuleHandle(chrome::kChromeElfDllName);
  static SetUploadConsentPointer set_upload_consent =
      reinterpret_cast<SetUploadConsentPointer>(
          GetProcAddress(elf_module, kCrashpadUpdateConsentFunctionName));

  if (set_upload_consent) {
    // Crashpad will use the kRegUsageStatsInSample registry value to apply
    // sampling correctly, but may_record already reflects the sampling state.
    // This isn't a problem though, since they will be consistent.
    set_upload_consent(may_record && may_upload);
  }
}
#endif  // defined(OS_WIN)

metrics::MetricsStateManager*
ChromeMetricsServicesManagerClient::GetMetricsStateManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!metrics_state_manager_) {
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_, enabled_state_provider_.get(),
        base::Bind(&PostStoreMetricsClientInfo),
        base::Bind(&GoogleUpdateSettings::LoadMetricsClientInfo));
  }
  return metrics_state_manager_.get();
}

bool ChromeMetricsServicesManagerClient::IsMetricsReportingForceEnabled() {
  return ChromeMetricsServiceClient::IsMetricsReportingForceEnabled();
}

bool ChromeMetricsServicesManagerClient::IsIncognitoSessionActive() {
  return chrome::IsIncognitoSessionActive();
}
