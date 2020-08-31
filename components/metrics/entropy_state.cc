// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/entropy_state.h"

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/pref_service.h"

namespace metrics {

namespace {

// The argument used to generate a non-identifying entropy source. We want no
// more than 13 bits of entropy, so use this max to return a number in the range
// [0, 7999] as the entropy source (12.97 bits of entropy).
const int kMaxLowEntropySize = 8000;

// Generates a new non-identifying entropy source used to seed persistent
// activities. Using a NoDestructor so that the new low entropy source value
// will only be generated on first access. And thus, even though we may write
// the new low entropy source value to prefs multiple times, it stays the same
// value.
int GenerateLowEntropySource() {
  static const base::NoDestructor<int> low_entropy_source(
      [] { return base::RandInt(0, kMaxLowEntropySize - 1); }());
  return *low_entropy_source;
}

}  // namespace

EntropyState::EntropyState(PrefService* local_state)
    : local_state_(local_state) {}

// static
constexpr int EntropyState::kLowEntropySourceNotSet;

void EntropyState::ClearPrefs() {
  DCHECK_EQ(kLowEntropySourceNotSet, low_entropy_source_);
  local_state_->ClearPref(prefs::kMetricsLowEntropySource);
  local_state_->ClearPref(prefs::kMetricsOldLowEntropySource);
}

// static
void EntropyState::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kMetricsLowEntropySource,
                                kLowEntropySourceNotSet);
  registry->RegisterIntegerPref(prefs::kMetricsOldLowEntropySource,
                                kLowEntropySourceNotSet);
}

std::string EntropyState::GetHighEntropySource(
    const std::string& initial_client_id) {
  DCHECK(!initial_client_id.empty());
  // For metrics reporting-enabled users, we combine the client ID and low
  // entropy source to get the final entropy source.
  // This has two useful properties:
  //  1) It makes the entropy source less identifiable for parties that do not
  //     know the low entropy source.
  //  2) It makes the final entropy source resettable.

  // If this install has an old low entropy source, continue using it, to avoid
  // changing the group assignments of studies using high entropy. New installs
  // only have the new low entropy source. If the number of installs with old
  // sources ever becomes small enough (see UMA.LowEntropySourceValue), we could
  // remove it, and just use the new source here.
  int low_entropy_source = GetOldLowEntropySource();
  if (low_entropy_source == kLowEntropySourceNotSet)
    low_entropy_source = GetLowEntropySource();

  return initial_client_id + base::NumberToString(low_entropy_source);
}

int EntropyState::GetLowEntropySource() {
  UpdateLowEntropySources();
  return low_entropy_source_;
}

int EntropyState::GetOldLowEntropySource() {
  UpdateLowEntropySources();
  return old_low_entropy_source_;
}

void EntropyState::UpdateLowEntropySources() {
  // The default value for |low_entropy_source_| and the default pref value are
  // both |kLowEntropySourceNotSet|, which indicates the value has not been set.
  if (low_entropy_source_ != kLowEntropySourceNotSet)
    return;

  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  // Only try to load the value from prefs if the user did not request a reset.
  // Otherwise, skip to generating a new value. We would have already returned
  // if |low_entropy_source_| were set, ensuring we only do this reset on the
  // first call to UpdateLowEntropySources().
  if (!command_line->HasSwitch(switches::kResetVariationState)) {
    int new_pref = local_state_->GetInteger(prefs::kMetricsLowEntropySource);
    if (IsValidLowEntropySource(new_pref))
      low_entropy_source_ = new_pref;
    int old_pref = local_state_->GetInteger(prefs::kMetricsOldLowEntropySource);
    if (IsValidLowEntropySource(old_pref))
      old_low_entropy_source_ = old_pref;
  }

  // If the new source is missing or corrupt (or requested to be reset), then
  // (re)create it. Don't bother recreating the old source if it's corrupt,
  // because we only keep the old source around for consistency, and we can't
  // maintain a consistent value if we recreate it.
  if (low_entropy_source_ == kLowEntropySourceNotSet) {
    low_entropy_source_ = GenerateLowEntropySource();
    DCHECK(IsValidLowEntropySource(low_entropy_source_));
    local_state_->SetInteger(prefs::kMetricsLowEntropySource,
                             low_entropy_source_);
  }

  // If the old source was present but corrupt (or requested to be reset), then
  // we'll never use it again, so delete it.
  if (old_low_entropy_source_ == kLowEntropySourceNotSet &&
      local_state_->HasPrefPath(prefs::kMetricsOldLowEntropySource)) {
    local_state_->ClearPref(prefs::kMetricsOldLowEntropySource);
  }

  DCHECK_NE(low_entropy_source_, kLowEntropySourceNotSet);
  // TODO(crbug/1041710): Currently, these metrics might be recorded multiple
  // times but that shouldn't matter because we can workaround it by using count
  // unique user mode. Also, once we verify that we can persist
  // low_entropy_source to our system profile proto, These two metrics are
  // longer needed and should be removed.
  base::UmaHistogramSparse("UMA.LowEntropySource3Value", low_entropy_source_);
  if (old_low_entropy_source_ != kLowEntropySourceNotSet) {
    base::UmaHistogramSparse("UMA.LowEntropySourceValue",
                             old_low_entropy_source_);
  }
}

// static
bool EntropyState::IsValidLowEntropySource(int value) {
  return value >= 0 && value < kMaxLowEntropySize;
}

}  // namespace metrics
