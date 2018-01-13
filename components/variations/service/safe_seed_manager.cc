// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/safe_seed_manager.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/client_filterable_state.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_seed_store.h"

namespace variations {

SafeSeedManager::SafeSeedManager(bool did_previous_session_exit_cleanly,
                                 PrefService* local_state)
    : local_state_(local_state) {
  // Increment the crash streak if the previous session crashed.
  // Note that the streak is not cleared if the previous run didn’t crash.
  // Instead, it’s incremented on each crash until Chrome is able to
  // successfully fetch a new seed. This way, a seed update that mostly
  // destabilizes Chrome will still result in a fallback to safe mode.
  int num_crashes = local_state->GetInteger(prefs::kVariationsCrashStreak);
  if (!did_previous_session_exit_cleanly) {
    ++num_crashes;
    local_state->SetInteger(prefs::kVariationsCrashStreak, num_crashes);
  }

  // After three failures in a row -- either consistent crashes or consistent
  // failures to fetch the seed -- assume that the current seed is bad, and fall
  // back to the safe seed. However, ignore any number of failures if the
  // --force-fieldtrials flag is set, as this flag is only used by developers,
  // and there's no need to make the development process flakier.
  const int kMaxFailuresBeforeRevertingToSafeSeed = 3;
  int num_failures_to_fetch =
      local_state->GetInteger(prefs::kVariationsFailedToFetchSeedStreak);
  bool fall_back_to_safe_mode =
      (num_crashes >= kMaxFailuresBeforeRevertingToSafeSeed ||
       num_failures_to_fetch >= kMaxFailuresBeforeRevertingToSafeSeed) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kForceFieldTrials);
  UMA_HISTOGRAM_BOOLEAN("Variations.SafeMode.FellBackToSafeMode",
                        fall_back_to_safe_mode);
  base::UmaHistogramSparse("Variations.SafeMode.Streak.Crashes",
                           base::ClampToRange(num_crashes, 0, 100));
  base::UmaHistogramSparse("Variations.SafeMode.Streak.FetchFailures",
                           base::ClampToRange(num_failures_to_fetch, 0, 100));
}

SafeSeedManager::~SafeSeedManager() = default;

// static
void SafeSeedManager::RegisterPrefs(PrefRegistrySimple* registry) {
  // Prefs tracking failures along the way to fetching a seed.
  registry->RegisterIntegerPref(prefs::kVariationsCrashStreak, 0);
  registry->RegisterIntegerPref(prefs::kVariationsFailedToFetchSeedStreak, 0);
}

void SafeSeedManager::SetActiveSeedState(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    std::unique_ptr<ClientFilterableState> client_filterable_state) {
  DCHECK(!has_set_active_seed_state_);
  has_set_active_seed_state_ = true;

  active_seed_state_ = std::make_unique<ActiveSeedState>(
      seed_data, base64_seed_signature, std::move(client_filterable_state));
}

void SafeSeedManager::RecordFetchStarted() {
  // Pessimistically assume the fetch will fail. The failure streak will be
  // reset upon success.
  int num_failures_to_fetch =
      local_state_->GetInteger(prefs::kVariationsFailedToFetchSeedStreak);
  local_state_->SetInteger(prefs::kVariationsFailedToFetchSeedStreak,
                           num_failures_to_fetch + 1);
}

void SafeSeedManager::RecordSuccessfulFetch(VariationsSeedStore* seed_store) {
  // The first time a fetch succeeds for a given run of Chrome, save the active
  // seed+filter configuration as safe. Note that it's sufficient to do this
  // only on the first successful fetch because the active configuration does
  // not change while Chrome is running. Also, note that it's fine to do this
  // even if running in safe mode, as the saved seed in that case will just be
  // the existing safe seed.
  if (active_seed_state_) {
    // TODO(isherman): As an optimization, skip writing the seed if we're
    // already running in safe mode.
    seed_store->StoreSafeSeed(active_seed_state_->seed_data,
                              active_seed_state_->base64_seed_signature,
                              *active_seed_state_->client_filterable_state);

    // The active seed state is only needed for the first time this code path is
    // reached, so free up its memory once the data is no longer needed.
    active_seed_state_.reset();
  }

  // Note: It's important to clear the crash streak as well as the fetch
  // failures streak. Crashes that occur after a successful seed fetch do not
  // prevent updating to a new seed, and therefore do not necessitate falling
  // back to a safe seed.
  local_state_->SetInteger(prefs::kVariationsCrashStreak, 0);
  local_state_->SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 0);
}

SafeSeedManager::ActiveSeedState::ActiveSeedState(
    const std::string& seed_data,
    const std::string& base64_seed_signature,
    std::unique_ptr<ClientFilterableState> client_filterable_state)
    : seed_data(seed_data),
      base64_seed_signature(base64_seed_signature),
      client_filterable_state(std::move(client_filterable_state)) {}

SafeSeedManager::ActiveSeedState::~ActiveSeedState() = default;

}  // namespace variations
