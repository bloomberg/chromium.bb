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
// INSTANT_TRIAL_ID_ON  = 3300123,
// INSTANT_TRIAL_ID_OFF = 3300124,
//
// If you programatically generate FieldTrials, you can still use a loop to
// create your IDs. Just be sure to reserve the range of IDs here with a clear
// comment.
//
// Ex:
// // Name: UMA-Uniformity-Trial-5-Percent
// // Range: 330000 - 3300099
// // The 5% Uniformity Trial. This is a reserved range.
// UNIFORMITY_TRIAL_5_PERCENT_ID_START = 330000,
// UNIFORMITY_TRIAL_5_PERCENT_ID_END   = 330099,
//
// Anything within the range of a uint32 should be castable to an ID, but
// please ensure that they are within the range of the min and max values.
enum VariationID {
  // Used to represent no associated Chrome variation ID.
  EMPTY_ID = 0,

  // The smallest possible Chrome Variation ID in the reserved range. The
  // first 10,000 values are reserved for internal variations infrastructure
  // use. Please do not use values in this range.
  MINIMIUM_ID = 3300000,

  // Name: UMA-Uniformity-Trial-1-Percent
  // Range: 3300000 - 3300099
  UNIFORMITY_1_PERCENT_BASE  = MINIMIUM_ID,
  UNIFORMITY_1_PERCENT_LIMIT = UNIFORMITY_1_PERCENT_BASE + 100,
  // Name: UMA-Uniformity-Trial-5-Percent
  // Range: 3300100 - 3300119
  UNIFORMITY_5_PERCENT_BASE  = UNIFORMITY_1_PERCENT_LIMIT,
  UNIFORMITY_5_PERCENT_LIMIT = UNIFORMITY_5_PERCENT_BASE + 20,
  // Name: UMA-Uniformity-Trial-10-Percent
  // Range: 3300120 - 3300129
  UNIFORMITY_10_PERCENT_BASE  = UNIFORMITY_5_PERCENT_LIMIT,
  UNIFORMITY_10_PERCENT_LIMIT = UNIFORMITY_10_PERCENT_BASE + 10,
  // Name: UMA-Uniformity-Trial-20-Percent
  // Range: 3300130 - 3300134
  UNIFORMITY_20_PERCENT_BASE  = UNIFORMITY_10_PERCENT_LIMIT,
  UNIFORMITY_20_PERCENT_LIMIT = UNIFORMITY_20_PERCENT_BASE + 5,
  // Name: UMA-Uniformity-Trial-50-Percent
  // Range: 3300135 - 3300136
  UNIFORMITY_50_PERCENT_BASE  = UNIFORMITY_20_PERCENT_LIMIT,
  UNIFORMITY_50_PERCENT_LIMIT = UNIFORMITY_50_PERCENT_BASE+ 2,

  // Name: UMA-Dynamic-Binary-Uniformity-Trial
  // The dynamic uniformity trial is only specified on the server, this is just
  // to reserve the id.
  DYNAMIC_UNIFORMITY_ID_DEFAULT = 3300137,
  DYNAMIC_UNIFORMITY_ID_GROUP_1 = 3300138,

  // Name: UMA-Session-Randomized-Uniformity-Trial-5-Percent
  // Range: 3300139 - 3300158
  // A uniformity trial used to compare one-time-randomized and
  // session-randomized FieldTrials.
  UNIFORMITY_SESSION_RANDOMIZED_5_PERCENT_BASE  = 3300139,
  UNIFORMITY_SESSION_RANDOMIZED_5_PERCENT_LIMIT =
      UNIFORMITY_SESSION_RANDOMIZED_5_PERCENT_BASE + 20,

  UNIFORMITY_TRIALS_MAX = 3300158,

  // Some values reserved for unit and integration tests.
  TEST_VALUE_A = 3300200,
  TEST_VALUE_B = 3300201,
  TEST_VALUE_C = 3300202,
  TEST_VALUE_D = 3300203,

  // USABLE IDs BEGIN HERE.
  //
  // The smallest possible Chrome Variation ID for use in real FieldTrials. If
  // you are defining variation IDs for your own FieldTrials, NEVER use a value
  // lower than this.
  MINIMUM_USER_ID = 3310000,

  // Add new variation IDs below.

  // Name: OmniboxSearchSuggest
  // Range: 3310000 - 3310019
  // Suggest (Autocomplete) field trial, 20 IDs.
  // Now retired.  But please don't reuse these IDs; they may taint
  // your experiment results.
  SUGGEST_ID_MIN = 3310000,
  SUGGEST_ID_MAX = 3310019,

  // Instant field trial.
  INSTANT_ID_CONTROL = 3310020,
  INSTANT_ID_SILENT  = 3310021,
  INSTANT_ID_HIDDEN  = 3310022,
  INSTANT_ID_SUGGEST = 3310023,
  INSTANT_ID_INSTANT = 3310024,

  // Instant dummy field trial.
  DUMMY_INSTANT_ID_DEFAULT      = 3310025,
  DUMMY_INSTANT_ID_CONTROL      = 3310026,
  DUMMY_INSTANT_ID_EXPERIMENT_1 = 3310027,
  DUMMY_INSTANT_ID_EXPERIMENT_2 = 3310028,
  DUMMY_INSTANT_ID_EXPERIMENT_3 = 3310049,

  // Name: OmniboxSearchSuggestStarted2012Q4
  // Range: 3310029 - 3310048
  // Suggest (Autocomplete) field trial, 20 IDs.  This differs from
  // the earlier omnibox suggest field trial in this file because
  // we created a new trial (with a new name) in order to shuffle IDs.
  // We assign new experiment IDs because it's a good practice not to
  // reuse experiment IDs.
  SUGGEST_TRIAL_STARTED_2012_Q4_ID_MIN = 3310029,
  SUGGEST_TRIAL_STARTED_2012_Q4_ID_MAX = 3310048,

  // Name: Instant channel and extended field trials.
  // Range: 3310050 - 3310059
  CHANNEL_INSTANT_ID_BETA            = 3310050,
  CHANNEL_INSTANT_ID_DEV             = 3310051,
  CHANNEL_INSTANT_ID_STABLE          = 3310052,
  EXTENDED_INSTANT_ID_CANARY_GROUP_1 = 3310053,
  EXTENDED_INSTANT_ID_CANARY_CONTROL = 3310054,
  EXTENDED_INSTANT_ID_DEV_GROUP_1    = 3310055,
  EXTENDED_INSTANT_ID_DEV_CONTROL    = 3310056,

  // Name: OmniboxSearchSuggestTrialStarted2013Q1
  // Range: 3310060 - 3310079
  // Suggest (Autocomplete) field trial, 20 IDs.  This differs from
  // the earlier omnibox suggest field trials in this file because
  // we created a new trial (with a new name) in order to shuffle IDs.
  // We assign new experiment IDs because it's a good practice not to
  // reuse experiment IDs.
  SUGGEST_TRIAL_STARTED_2013_Q1_ID_MIN = 3310060,
  SUGGEST_TRIAL_STARTED_2013_Q1_ID_MAX = 3310079,

  // Name: More IDs for the InstantExtended field trial.
  // Range: 3310080 - 3310085
  EXTENDED_INSTANT_ID_UNUSED_1         = 3310080,
  EXTENDED_INSTANT_ID_UNUSED_2         = 3310081,
  EXTENDED_INSTANT_ID_CANARY_CONTROL_2 = 3310082,
  EXTENDED_INSTANT_ID_DEV_CONTROL_2    = 3310083,
  EXTENDED_INSTANT_ID_CANARY_GROUP_2   = 3310084,
  EXTENDED_INSTANT_ID_DEV_GROUP_2      = 3310085,

  // Reserve 100 IDs to be used by autocomplete dynamic field trials.
  // The dynamic field trials are activated by a call to
  // OmniboxFieldTrial::ActivateDynamicFieldTrials.
  // For more details, see
  // chrome/browser/omnibox/omnibox_field_trial.{h,cc}.
  AUTOCOMPLETE_DYNAMIC_FIELD_TRIAL_ID_MIN = 3310086,
  AUTOCOMPLETE_DYANMIC_FIELD_TRIAL_ID_MAX = 3310185,

  // BookmarkPrompt field trial.
  BOOKMARK_PROMPT_TRIAL_DEFAULT = 3310186,
  BOOKMARK_PROMPT_TRIAL_CONTROL = 3310187,
  BOOKMARK_PROMPT_TRIAL_EXPERIMENT = 3310188,

  // iOS tour trial.
  IOS_TOUR_DEFAULT = 3310189,
  IOS_TOUR_EXPERIMENT = 3310190,

  // Name: SendFeedbackLinkLocation.
  // Field trial to test various locations, and strings
  // for submitting feedback.
  SEND_FEEDBACK_LINK_LOCATION_CONTROL = 3310200,
  SEND_FEEDBACK_LINK_LOCATION_CONTROL_CROS = 3310201,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_DEV = 3310202,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_STABLE = 3310203,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_BETA = 3310204,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_CROS_DEV = 3310205,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_CROS_STABLE = 3310206,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_CROS_BETA = 3310207,
  SEND_FEEDBACK_LINK_LOCATION_ALT_LOCATION_DEV = 3310208,
  SEND_FEEDBACK_LINK_LOCATION_ALT_LOCATION_STABLE = 3310209,
  SEND_FEEDBACK_LINK_LOCATION_ALT_LOCATION_BETA = 3310210,
  SEND_FEEDBACK_LINK_LOCATION_ALT_LOCATION_CROS_DEV = 3310211,
  SEND_FEEDBACK_LINK_LOCATION_ALT_LOCATION_CROS_STABLE = 3310212,
  SEND_FEEDBACK_LINK_LOCATION_ALT_LOCATION_CROS_BETA = 3310213,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_AND_LOCATION_DEV = 3310214,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_AND_LOCATION_STABLE = 3310215,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_AND_LOCATION_BETA = 3310216,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_AND_LOCATION_CROS_DEV = 3310217,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_AND_LOCATION_CROS_STABLE = 3310218,
  SEND_FEEDBACK_LINK_LOCATION_ALT_TEXT_AND_LOCATION_CROS_BETA = 3310219,
  SEND_FEEDBACK_LINK_LOCATION_DEFAULT = 3310249,

  // NEXT ID: When adding new IDs, please add them above this section, starting
  // with the value of NEXT_ID, and updating NEXT_ID to (end of your reserved
  // range) + 1.
  NEXT_ID = 3310250,

  // USABLE IDs END HERE.
  //
  // The largest possible Chrome variation ID in the reserved range. When
  // defining your variation IDs, DO NOT exceed this value - GWS will ignore
  // your experiment!
  MAXIMUM_ID = 3399999,
};

}  // namespace chrome_variations

#endif  // CHROME_COMMON_METRICS_VARIATIONS_VARIATION_IDS_H_
