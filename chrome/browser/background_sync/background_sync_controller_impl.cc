// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_controller_impl.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/background_sync_parameters.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/background_sync_launcher_android.h"
#endif

namespace {

// Default min time gap between two periodic sync events for a given
// Periodic Background Sync registration.
constexpr base::TimeDelta kMinGapBetweenPeriodicSyncEvents =
    base::TimeDelta::FromHours(12);

}  // namespace

// static
const char BackgroundSyncControllerImpl::kFieldTrialName[] = "BackgroundSync";
const char BackgroundSyncControllerImpl::kDisabledParameterName[] = "disabled";
const char BackgroundSyncControllerImpl::kMaxAttemptsParameterName[] =
    "max_sync_attempts";
const char BackgroundSyncControllerImpl::kInitialRetryParameterName[] =
    "initial_retry_delay_sec";
const char BackgroundSyncControllerImpl::kRetryDelayFactorParameterName[] =
    "retry_delay_factor";
const char BackgroundSyncControllerImpl::kMinSyncRecoveryTimeName[] =
    "min_recovery_time_sec";
const char BackgroundSyncControllerImpl::kMaxSyncEventDurationName[] =
    "max_sync_event_duration_sec";

BackgroundSyncControllerImpl::BackgroundSyncControllerImpl(Profile* profile)
    : profile_(profile) {}

BackgroundSyncControllerImpl::~BackgroundSyncControllerImpl() = default;

void BackgroundSyncControllerImpl::GetParameterOverrides(
    content::BackgroundSyncParameters* parameters) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if defined(OS_ANDROID)
  if (BackgroundSyncLauncherAndroid::ShouldDisableBackgroundSync()) {
    parameters->disable = true;
  }
#endif

  std::map<std::string, std::string> field_params;
  if (!variations::GetVariationParams(kFieldTrialName, &field_params))
    return;

  if (base::LowerCaseEqualsASCII(field_params[kDisabledParameterName],
                                 "true")) {
    parameters->disable = true;
  }

  if (base::ContainsKey(field_params, kMaxAttemptsParameterName)) {
    int max_attempts;
    if (base::StringToInt(field_params[kMaxAttemptsParameterName],
                          &max_attempts)) {
      parameters->max_sync_attempts = max_attempts;
    }
  }

  if (base::ContainsKey(field_params, kInitialRetryParameterName)) {
    int initial_retry_delay_sec;
    if (base::StringToInt(field_params[kInitialRetryParameterName],
                          &initial_retry_delay_sec)) {
      parameters->initial_retry_delay =
          base::TimeDelta::FromSeconds(initial_retry_delay_sec);
    }
  }

  if (base::ContainsKey(field_params, kRetryDelayFactorParameterName)) {
    int retry_delay_factor;
    if (base::StringToInt(field_params[kRetryDelayFactorParameterName],
                          &retry_delay_factor)) {
      parameters->retry_delay_factor = retry_delay_factor;
    }
  }

  if (base::ContainsKey(field_params, kMinSyncRecoveryTimeName)) {
    int min_sync_recovery_time_sec;
    if (base::StringToInt(field_params[kMinSyncRecoveryTimeName],
                          &min_sync_recovery_time_sec)) {
      parameters->min_sync_recovery_time =
          base::TimeDelta::FromSeconds(min_sync_recovery_time_sec);
    }
  }

  if (base::ContainsKey(field_params, kMaxSyncEventDurationName)) {
    int max_sync_event_duration_sec;
    if (base::StringToInt(field_params[kMaxSyncEventDurationName],
                          &max_sync_event_duration_sec)) {
      parameters->max_sync_event_duration =
          base::TimeDelta::FromSeconds(max_sync_event_duration_sec);
    }
  }

  return;
}

void BackgroundSyncControllerImpl::NotifyBackgroundSyncRegistered(
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (profile_->IsOffTheRecord())
    return;

  rappor::SampleDomainAndRegistryFromGURL(GetRapporServiceImpl(),
                                          "BackgroundSync.Register.Origin",
                                          origin.GetURL());
}

void BackgroundSyncControllerImpl::RunInBackground() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (profile_->IsOffTheRecord())
    return;
#if defined(OS_ANDROID)
  BackgroundSyncLauncherAndroid::LaunchBrowserIfStopped();
#endif
}

base::TimeDelta BackgroundSyncControllerImpl::GetNextEventDelay(
    int64_t min_interval,
    int num_attempts,
    blink::mojom::BackgroundSyncType sync_type,
    content::BackgroundSyncParameters* parameters) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(parameters);

  if (!num_attempts) {
    // First attempt.
    switch (sync_type) {
      case blink::mojom::BackgroundSyncType::ONE_SHOT:
        return base::TimeDelta();
      case blink::mojom::BackgroundSyncType::PERIODIC:
        // TODO(crbug.com/925297): Integrate with site engagement data.
        int site_engagement_factor = 1;
        int64_t effective_gap_ms =
            site_engagement_factor *
            kMinGapBetweenPeriodicSyncEvents.InMilliseconds();
        return base::TimeDelta::FromMilliseconds(
            std::max(min_interval, effective_gap_ms));
    }
  }

  // After a sync event has been fired.
  DCHECK_LT(num_attempts, parameters->max_sync_attempts);
  return parameters->initial_retry_delay *
         pow(parameters->retry_delay_factor, num_attempts - 1);
}

rappor::RapporServiceImpl*
BackgroundSyncControllerImpl::GetRapporServiceImpl() {
  return g_browser_process->rappor_service();
}
