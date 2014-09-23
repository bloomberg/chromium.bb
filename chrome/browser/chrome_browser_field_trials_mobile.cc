// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_mobile.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"

#if defined(OS_ANDROID)
#include "chrome/browser/prerender/prerender_field_trial.h"
#endif

namespace chrome {

namespace {

// Base function used by all data reduction proxy field trials. A trial is
// only conducted for installs that are in the "Enabled" group. They are always
// enabled on DEV and BETA channels, and for STABLE, a percentage will be
// controlled from the server. Until the percentage is learned from the server,
// a client-side configuration is used.
void DataCompressionProxyBaseFieldTrial(
    const char* trial_name,
    const base::FieldTrial::Probability enabled_group_probability,
    const base::FieldTrial::Probability total_probability) {
  const char kEnabled[] = "Enabled";
  const char kDisabled[] = "Disabled";

  // Find out if this is a stable channel.
  const bool kIsStableChannel =
      chrome::VersionInfo::GetChannel() == chrome::VersionInfo::CHANNEL_STABLE;

  // Experiment enabled until Jan 1, 2015. By default, disabled.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          trial_name,
          total_probability,
          kDisabled, 2015, 1, 1, base::FieldTrial::ONE_TIME_RANDOMIZED, NULL));

  // Non-stable channels will run with probability 1.
  trial->AppendGroup(
      kEnabled,
      kIsStableChannel ? enabled_group_probability : total_probability);

  trial->group();
}

void SetupDataCompressionProxyFieldTrials() {
  // Governs the rollout of the _promo_ for the compression proxy
  // independently from the rollout of compression proxy. The enabled
  // percentage is configured to be 10% = 100 / 1000 until overridden by the
  // server. When this trial is "Enabled," users get a promo, whereas
  // otherwise, compression is available without a promo.
  DataCompressionProxyBaseFieldTrial(
      "DataCompressionProxyPromoVisibility", 100, 1000);
}

}  // namespace

void SetupMobileFieldTrials(const CommandLine& parsed_command_line) {
  SetupDataCompressionProxyFieldTrials();
#if defined(OS_ANDROID)
  prerender::ConfigurePrerender(parsed_command_line);
#endif
}

}  // namespace chrome
