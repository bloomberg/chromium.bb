// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
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
#include "components/rappor/rappor_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"

namespace {

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
  content::BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE,
      base::Bind(&GoogleUpdateSettings::StoreMetricsClientInfo, client_info));
}

// Only clients that were given an opt-out metrics-reporting consent flow are
// eligible for sampling.
bool IsClientEligibleForSampling() {
  return metrics::GetMetricsReportingDefaultState(
             g_browser_process->local_state()) ==
         metrics::EnableMetricsDefault::OPT_OUT;
}

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

  SetupMetricsStateForChromeOS();
}

ChromeMetricsServicesManagerClient::~ChromeMetricsServicesManagerClient() {}

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

std::unique_ptr<rappor::RapporService>
ChromeMetricsServicesManagerClient::CreateRapporService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::WrapUnique(new rappor::RapporService(
      local_state_, base::Bind(&chrome::IsIncognitoSessionActive)));
}

std::unique_ptr<variations::VariationsService>
ChromeMetricsServicesManagerClient::CreateVariationsService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return variations::VariationsService::Create(
      base::WrapUnique(new ChromeVariationsServiceClient()), local_state_,
      GetMetricsStateManager(), switches::kDisableBackgroundNetworking,
      chrome_variations::CreateUIStringOverrider());
}

std::unique_ptr<metrics::MetricsServiceClient>
ChromeMetricsServicesManagerClient::CreateMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return ChromeMetricsServiceClient::Create(GetMetricsStateManager(),
                                            local_state_);
}

net::URLRequestContextGetter*
ChromeMetricsServicesManagerClient::GetURLRequestContext() {
  return g_browser_process->system_request_context();
}

bool ChromeMetricsServicesManagerClient::IsSafeBrowsingEnabled(
    const base::Closure& on_update_callback) {
  // Start listening for updates to SB service state. This is done here instead
  // of in the constructor to avoid errors from trying to instantiate SB
  // service before the IO thread exists.
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_state_subscription_ && sb_service) {
    // It is safe to pass the callback received from the
    // MetricsServicesManager here since the MetricsServicesManager owns
    // this object, which owns the sb_state_subscription_, which owns the
    // pointer to the MetricsServicesManager.
    sb_state_subscription_ =
        sb_service->RegisterStateCallback(on_update_callback);
  }

  return sb_service && sb_service->enabled_by_prefs();
}

bool ChromeMetricsServicesManagerClient::IsMetricsReportingEnabled() {
  return enabled_state_provider_->IsReportingEnabled();
}

bool ChromeMetricsServicesManagerClient::OnlyDoMetricsRecording() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  return cmdline->HasSwitch(switches::kMetricsRecordingOnly) ||
         cmdline->HasSwitch(switches::kEnableBenchmarking);
}

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
