// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_state_manager.h"

#include "base/command_line.h"
#include "base/guid.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/cloned_install_detector.h"
#include "chrome/browser/metrics/machine_id_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/variations/caching_permuted_entropy_provider.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

namespace metrics {

namespace {

// The argument used to generate a non-identifying entropy source. We want no
// more than 13 bits of entropy, so use this max to return a number in the range
// [0, 7999] as the entropy source (12.97 bits of entropy).
const int kMaxLowEntropySize = 8000;

// Default prefs value for prefs::kMetricsLowEntropySource to indicate that the
// value has not yet been set.
const int kLowEntropySourceNotSet = -1;

// Generates a new non-identifying entropy source used to seed persistent
// activities.
int GenerateLowEntropySource() {
  return base::RandInt(0, kMaxLowEntropySize - 1);
}

}  // namespace

// static
bool MetricsStateManager::instance_exists_ = false;

MetricsStateManager::MetricsStateManager(PrefService* local_state)
    : local_state_(local_state),
      low_entropy_source_(kLowEntropySourceNotSet),
      entropy_source_returned_(ENTROPY_SOURCE_NONE) {
  ResetMetricsIDsIfNecessary();
  if (IsMetricsReportingEnabled())
    ForceClientIdCreation();

  DCHECK(!instance_exists_);
  instance_exists_ = true;
}

MetricsStateManager::~MetricsStateManager() {
  DCHECK(instance_exists_);
  instance_exists_ = false;
}

bool MetricsStateManager::IsMetricsReportingEnabled() {
  // If the user permits metrics reporting with the checkbox in the
  // prefs, we turn on recording.  We disable metrics completely for
  // non-official builds.  This can be forced with a flag.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableMetricsReportingForTesting))
    return true;

  // Disable metrics reporting when field trials are forced.
  if (command_line->HasSwitch(switches::kForceFieldTrials))
    return false;

  bool enabled = false;
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &enabled);
#else
  enabled = local_state_->GetBoolean(prefs::kMetricsReportingEnabled);
#endif  // #if defined(OS_CHROMEOS)
#endif  // defined(GOOGLE_CHROME_BUILD)
  return enabled;
}

void MetricsStateManager::ForceClientIdCreation() {
  if (!client_id_.empty())
    return;

  client_id_ = local_state_->GetString(prefs::kMetricsClientID);
  if (!client_id_.empty())
    return;

  client_id_ = base::GenerateGUID();
  local_state_->SetString(prefs::kMetricsClientID, client_id_);

  if (local_state_->GetString(prefs::kMetricsOldClientID).empty()) {
    // Record the timestamp of when the user opted in to UMA.
    local_state_->SetInt64(prefs::kMetricsReportingEnabledTimestamp,
                           base::Time::Now().ToTimeT());
  } else {
    UMA_HISTOGRAM_BOOLEAN("UMA.ClientIdMigrated", true);
  }
  local_state_->ClearPref(prefs::kMetricsOldClientID);
}

void MetricsStateManager::CheckForClonedInstall(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!cloned_install_detector_);

  MachineIdProvider* provider = MachineIdProvider::CreateInstance();
  if (!provider)
    return;

  cloned_install_detector_.reset(new ClonedInstallDetector(provider));
  cloned_install_detector_->CheckForClonedInstall(local_state_, task_runner);
}

scoped_ptr<const base::FieldTrial::EntropyProvider>
MetricsStateManager::CreateEntropyProvider() {
  // For metrics reporting-enabled users, we combine the client ID and low
  // entropy source to get the final entropy source. Otherwise, only use the low
  // entropy source.
  // This has two useful properties:
  //  1) It makes the entropy source less identifiable for parties that do not
  //     know the low entropy source.
  //  2) It makes the final entropy source resettable.
  const int low_entropy_source_value = GetLowEntropySource();
  UMA_HISTOGRAM_SPARSE_SLOWLY("UMA.LowEntropySourceValue",
                              low_entropy_source_value);
  if (IsMetricsReportingEnabled()) {
    if (entropy_source_returned_ == ENTROPY_SOURCE_NONE)
      entropy_source_returned_ = ENTROPY_SOURCE_HIGH;
    DCHECK_EQ(ENTROPY_SOURCE_HIGH, entropy_source_returned_);
    const std::string high_entropy_source =
        client_id_ + base::IntToString(low_entropy_source_value);
    return scoped_ptr<const base::FieldTrial::EntropyProvider>(
        new SHA1EntropyProvider(high_entropy_source));
  }

  if (entropy_source_returned_ == ENTROPY_SOURCE_NONE)
    entropy_source_returned_ = ENTROPY_SOURCE_LOW;
  DCHECK_EQ(ENTROPY_SOURCE_LOW, entropy_source_returned_);

#if defined(OS_ANDROID) || defined(OS_IOS)
  return scoped_ptr<const base::FieldTrial::EntropyProvider>(
      new CachingPermutedEntropyProvider(local_state_,
                                         low_entropy_source_value,
                                         kMaxLowEntropySize));
#else
  return scoped_ptr<const base::FieldTrial::EntropyProvider>(
      new PermutedEntropyProvider(low_entropy_source_value,
                                  kMaxLowEntropySize));
#endif
}

// static
scoped_ptr<MetricsStateManager> MetricsStateManager::Create(
    PrefService* local_state) {
  scoped_ptr<MetricsStateManager> result;
  // Note: |instance_exists_| is updated in the constructor and destructor.
  if (!instance_exists_)
    result.reset(new MetricsStateManager(local_state));
  return result.Pass();
}

// static
void MetricsStateManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kMetricsResetIds, false);
  registry->RegisterStringPref(prefs::kMetricsClientID, std::string());
  registry->RegisterInt64Pref(prefs::kMetricsReportingEnabledTimestamp, 0);
  registry->RegisterIntegerPref(prefs::kMetricsLowEntropySource,
                                kLowEntropySourceNotSet);

  ClonedInstallDetector::RegisterPrefs(registry);
  CachingPermutedEntropyProvider::RegisterPrefs(registry);

  // TODO(asvitkine): Remove these once a couple of releases have passed.
  // http://crbug.com/357704
  registry->RegisterStringPref(prefs::kMetricsOldClientID, std::string());
  registry->RegisterIntegerPref(prefs::kMetricsOldLowEntropySource, 0);
}

int MetricsStateManager::GetLowEntropySource() {
  // Note that the default value for the low entropy source and the default pref
  // value are both kLowEntropySourceNotSet, which is used to identify if the
  // value has been set or not.
  if (low_entropy_source_ != kLowEntropySourceNotSet)
    return low_entropy_source_;

  const CommandLine* command_line(CommandLine::ForCurrentProcess());
  // Only try to load the value from prefs if the user did not request a reset.
  // Otherwise, skip to generating a new value.
  if (!command_line->HasSwitch(switches::kResetVariationState)) {
    int value = local_state_->GetInteger(prefs::kMetricsLowEntropySource);
    // If the value is outside the [0, kMaxLowEntropySize) range, re-generate
    // it below.
    if (value >= 0 && value < kMaxLowEntropySize) {
      low_entropy_source_ = value;
      UMA_HISTOGRAM_BOOLEAN("UMA.GeneratedLowEntropySource", false);
      return low_entropy_source_;
    }
  }

  UMA_HISTOGRAM_BOOLEAN("UMA.GeneratedLowEntropySource", true);
  low_entropy_source_ = GenerateLowEntropySource();
  local_state_->SetInteger(prefs::kMetricsLowEntropySource,
                           low_entropy_source_);
  local_state_->ClearPref(prefs::kMetricsOldLowEntropySource);
  metrics::CachingPermutedEntropyProvider::ClearCache(local_state_);

  return low_entropy_source_;
}

void MetricsStateManager::ResetMetricsIDsIfNecessary() {
  if (!local_state_->GetBoolean(prefs::kMetricsResetIds))
    return;

  UMA_HISTOGRAM_BOOLEAN("UMA.MetricsIDsReset", true);

  DCHECK(client_id_.empty());
  DCHECK_EQ(kLowEntropySourceNotSet, low_entropy_source_);

  local_state_->ClearPref(prefs::kMetricsClientID);
  local_state_->ClearPref(prefs::kMetricsLowEntropySource);
  local_state_->ClearPref(prefs::kMetricsResetIds);
}

}  // namespace metrics
