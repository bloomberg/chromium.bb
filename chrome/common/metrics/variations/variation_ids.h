// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_VARIATIONS_VARIATION_IDS_H_
#define CHROME_COMMON_METRICS_VARIATIONS_VARIATION_IDS_H_

namespace chrome_variations {

// A list of Chrome Variation IDs. These IDs are associated with FieldTrials
// for re-identification and analysis on Google servers.
// These enums are to be used with the experiments_helper ID associoation API.
//
// The IDs are defined as part of an enum to prevent re-use. When adding your
// own IDs, please respect the reserved IDs of other groups, as well as the
// global range of permitted values.
//
// When you want to create a FieldTrial that needs to be recognized by Google
// properties, reserve an ID by declaring them below. Please start with the name
// of the FieldTrial followed a short description.
//
// Ex:
// // Name: Instant-Field-Trial
// // The Omnibox Instant Trial.
// kInstantTrialOn  = 3300123,
// kInstantTrialOff = 3300124,
//
// If you programatically generate FieldTrials, you can still use a loop to
// create your IDs. Just be sure to reserve the range of IDs here with a clear
// comment.
//
// Ex:
// // Name: UMA-Uniformity-Trial-5-Percent
// // Range: 330000 - 3300099
// // The 5% Uniformity Trial. This is a reserved range.
// kUniformityTrial5PercentStart = 330000,
// kUniformirtTrial5PercentEnd   = 330099,
//
// Anything within the range of a uint32 should be castable to an ID, but
// please ensure that they are within the range of the min and max values.
enum VariationID {
  // Used to represent no associated Chrome variation ID.
  kEmptyID = 0,

  // The smallest possible Chrome Variation ID in the reserved range. The
  // first 10,000 values are reserved for internal variations infrastructure
  // use. Please do not use values in this range.
  kMinimumID = 3300000,

  // Name: UMA-Uniformity-Trial-1-Percent
  // Range: 3300000 - 3300099
  kUniformity1PercentBase   = kMinimumID,
  kUniformity1PercentLimit  = kUniformity1PercentBase + 100,
  // Name: UMA-Uniformity-Trial-5-Percent
  // Range: 3300100 - 3300119
  kUniformity5PercentBase   = kUniformity1PercentLimit,
  kUniformity5PercentLimit  = kUniformity5PercentBase + 20,
  // Name: UMA-Uniformity-Trial-10-Percent
  // Range: 3300120 - 3300129
  kUniformity10PercentBase  = kUniformity5PercentLimit,
  kUniformity10PercentLimit = kUniformity10PercentBase + 10,
  // Name: UMA-Uniformity-Trial-20-Percent
  // Range: 3300130 - 3300134
  kUniformity20PercentBase  = kUniformity10PercentLimit,
  kUniformity20PercentLimit = kUniformity20PercentBase + 5,
  // Name: UMA-Uniformity-Trial-50-Percent
  // Range: 3300135 - 3300136
  kUniformity50PercentBase  = kUniformity20PercentLimit,
  kUniformity50PercentLimit = kUniformity50PercentBase + 2,

  // Name: UMA-Dynamic-Binary-Uniformity-Trial
  // The dynamic uniformity trial is only specified on the server, this is just
  // to reserve the id.
  kDynamicUniformityDefault = 3300137,
  kDynamicUniformityGroup01 = 3300138,

  // Name: UMA-Session-Randomized-Uniformity-Trial-5-Percent
  // Range: 3300139 - 3300158
  // A uniformity trial used to compare one-time-randomized and
  // session-randomized FieldTrials.
  kUniformitySessionRandomized5PercentBase  = 3300139,
  kUniformitySessionRandomized5PercentLimit =
      kUniformitySessionRandomized5PercentBase + 20,

  kUniformityTrialsMax      = 3300158,

  // Some values reserved for unit and integration tests.
  kTestValueA = 3300200,
  kTestValueB = 3300201,

  // USABLE IDs BEGIN HERE.
  //
  // The smallest possible Chrome Variation ID for use in real FieldTrials. If
  // you are defining variation IDs for your own FieldTrials, NEVER use a value
  // lower than this.
  kMinimumUserID = 3310000,

  // Add new variation IDs below.

  // Name: OmniboxSearchSuggest
  // Range: 3310000 - 3310019
  // Suggest (Autocomplete) field trial, 20 IDs.
  // Now retired.  But please don't reuse these IDs; they may taint
  // your experiment results.
  kSuggestIDMin = 3310000,
  kSuggestIDMax = 3310019,

  // Instant field trial.
  kInstantIDControl = 3310020,
  kInstantIDSilent  = 3310021,
  kInstantIDHidden  = 3310022,
  kInstantIDSuggest = 3310023,
  kInstantIDInstant = 3310024,

  // Instant dummy field trial.
  kDummyInstantIDDefault         = 3310025,
  kDummyInstantIDControl         = 3310026,
  kDummyInstantIDExperimentOne   = 3310027,
  kDummyInstantIDExperimentTwo   = 3310028,
  kDummyInstantIDExperimentThree = 3310049,

  // Name: OmniboxSearchSuggestStarted2012Q4
  // Range: 3310029 - 3310048
  // Suggest (Autocomplete) field trial, 20 IDs.  This differs from
  // the earlier omnibox suggest field trial in this file because
  // we created a new trial (with a new name) in order to shuffle IDs.
  // We assign new experiment IDs because it's a good practice not to
  // reuse experiment IDs.
  kSuggestTrialStarted2012Q4IDMin = 3310029,
  kSuggestTrialStarted2012Q4IDMax = 3310048,

  // Name: Instant channel and extended field trials.
  // Range: 3310050 - 3310059
  kChannelInstantIDBeta            = 3310050,
  kChannelInstantIDDev             = 3310051,
  kChannelInstantIDStable          = 3310052,
  kExtendedInstantIDCanaryGroupOne = 3310053,
  kExtendedInstantIDCanaryControl  = 3310054,
  kExtendedInstantIDDevGroupOne    = 3310055,
  kExtendedInstantIDDevControl     = 3310056,

  // NEXT ID: When adding new IDs, please add them above this section, starting
  // with the value of kNextID, and updating kNextID to (end of your reserved
  // range) + 1.
  kNextID = 3310060,


  // USABLE IDs END HERE.
  //
  // The largest possible Chrome variation ID in the reserved range. When
  // defining your variation IDs, DO NOT exceed this value.
  kMaximumID = 3399999,
};

}  // namespace chrome_variations

#endif  // CHROME_COMMON_METRICS_VARIATIONS_VARIATION_IDS_H_
