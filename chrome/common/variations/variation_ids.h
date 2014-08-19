// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_VARIATIONS_VARIATION_IDS_H_
#define CHROME_COMMON_VARIATIONS_VARIATION_IDS_H_

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
// // Range: 330000 - 3300019
// // The 5% Uniformity Trial. This is a reserved range.
// UNIFORMITY_TRIAL_5_PERCENT_ID_BASE = 330000,
// UNIFORMITY_TRIAL_5_PERCENT_ID_LIMIT =
//     UNIFORMITY_TRIAL_5_PERCENT_ID_BASE + 20,
//
// Anything within the range of a uint32 should be castable to an ID, but
// please ensure that they are within the range of the min and max values.
enum ReservedVariationID {
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
  UNIFORMITY_50_PERCENT_LIMIT = UNIFORMITY_50_PERCENT_BASE + 2,

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

  // Name: UMA-Dynamic-Uniformity-Trial
  // Range: 3300159 - 3300165
  UNIFORMITY_DYNAMIC_TRIAL_BASE = 3300159,
  UNIFORMITY_DYNAMIC_TRIAL_LIMIT = UNIFORMITY_DYNAMIC_TRIAL_BASE + 6,

  // Some values reserved for unit and integration tests.
  // Range: 3300159 - 3300299
  TEST_VALUE_BASE = 3300200,
  TEST_VALUE_LIMIT = TEST_VALUE_BASE + 100,

  // USABLE IDs BEGIN HERE.
  //
  // The smallest possible Chrome Variation ID for use in real FieldTrials. If
  // you are defining variation IDs for your own FieldTrials, NEVER use a value
  // lower than this.
  MINIMUM_USER_ID = 3310000,

  // Add new variation IDs below.

  // DEPRECATED - DO NOT USE
  // Name: OmniboxSearchSuggest
  // Range: 3310000 - 3310019
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

  // DEPRECATED - DO NOT USE
  // Name: OmniboxSearchSuggestStarted2012Q4
  // Range: 3310029 - 3310048
  // Now retired.  But please don't reuse these IDs; they may taint
  // your experiment results.
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

  // DEPRECATED - DO NOT USE
  // Name: OmniboxSearchSuggestTrialStarted2013Q1
  // Range: 3310060 - 3310079
  // Now retired.  But please don't reuse these IDs; they may taint
  // your experiment results.
  SUGGEST_TRIAL_STARTED_2013_Q1_ID_MIN = 3310060,
  SUGGEST_TRIAL_STARTED_2013_Q1_ID_MAX = 3310079,

  // Name: More IDs for the InstantExtended field trial.
  // Range: 3310080 - 3310085
  EXTENDED_INSTANT_ID_UNUSED_1            = 3310080,
  EXTENDED_INSTANT_ID_UNUSED_2            = 3310081,
  EXTENDED_INSTANT_ID_CANARY_CONTROL_2    = 3310082,
  EXTENDED_INSTANT_ID_DEV_CONTROL_2       = 3310083,
  EXTENDED_INSTANT_ID_CANARY_GROUP_2      = 3310084,
  EXTENDED_INSTANT_ID_DEV_GROUP_2         = 3310085,
  EXTENDED_INSTANT_ID_BETA_CONTROL_1      = 3310250,
  EXTENDED_INSTANT_ID_BETA_CONTROL_2      = 3310251,
  EXTENDED_INSTANT_ID_BETA_GROUP_1        = 3310252,
  EXTENDED_INSTANT_ID_BETA_GROUP_2        = 3310253,
  EXTENDED_INSTANT_ID_DEV_FRESH_CONTROL_1 = 3310254,
  EXTENDED_INSTANT_ID_DEV_FRESH_CONTROL_2 = 3310255,
  EXTENDED_INSTANT_ID_DEV_FRESH_GROUP_1   = 3310256,
  EXTENDED_INSTANT_ID_DEV_FRESH_GROUP_2   = 3310257,
  EXTENDED_INSTANT_ID_DEV_HOLDBACK        = 3310258,
  EXTENDED_INSTANT_ID_DEV_OFFLINE_1       = 3310259,
  EXTENDED_INSTANT_ID_DEV_OFFLINE_2       = 3310260,
  EXTENDED_INSTANT_ID_DEV_INSTANT_1       = 3310261,
  EXTENDED_INSTANT_ID_DEV_INSTANT_2       = 3310262,
  // Reserve a contiguous chunk of IDs for Instant Extended.
  EXTENDED_INSTANT_RANGE_ID_MIN           = 3310265,
  EXTENDED_INSTANT_RANGE_ID_MAX           = 3310365,
  EXTENDED_INSTANT_RANGE2_ID_MIN          = 3310368,
  EXTENDED_INSTANT_RANGE2_ID_MAX          = 3310868,
  EXTENDED_INSTANT_RANGE3_ID_MIN          = 3310871,
  EXTENDED_INSTANT_RANGE3_ID_MAX          = 3311870,

  // Reserve 100 IDs to be used by autocomplete dynamic field trials.
  // The dynamic field trials are activated by a call to
  // OmniboxFieldTrial::ActivateDynamicFieldTrials.
  // For more details, see
  // chrome/browser/omnibox/omnibox_field_trial.{h,cc}.
  AUTOCOMPLETE_DYNAMIC_FIELD_TRIAL_ID_MIN = 3310086,
  AUTOCOMPLETE_DYNAMIC_FIELD_TRIAL_ID_MAX = 3310185,

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

  // OmniboxStopTimer field trial.
  OMNIBOX_STOP_TIMER_CONTROL = 3310263,
  OMNIBOX_STOP_TIMER_EXPERIMENT = 3310264,

  // ShowAppLauncherPromo field trial
  SHOW_APP_LAUNCHER_PROMO_UNTIL_DISMISSED = 3310366,
  SHOW_APP_LAUNCHER_PROMO_RESET_PREF = 3310367,

  // CookieRetentionPriorityStudy field trial.
  COOKIE_RETENTION_PRIORITY_STUDY_EXPERIMENT_OFF = 3310869,
  COOKIE_RETENTION_PRIORITY_STUDY_EXPERIMENT_ON = 3310870,

  // QUIC field trial.
  QUIC_FIELD_TRIAL_ID_MIN = 3311871,
  QUIC_FIELD_TRIAL_ID_MAX = 3311920,

  // Android Native NTP trial.
  // Range: 3311921 - 3311940
  ANDROID_DEV_BETA_NATIVE_NTP_CONTROL_0 = 3311921,
  ANDROID_DEV_BETA_NATIVE_NTP_CONTROL_1 = 3311922,
  ANDROID_DEV_BETA_NATIVE_NTP_TWO_URL_BARS = 3311923,
  ANDROID_DEV_BETA_NATIVE_NTP_ONE_URL_BAR = 3311924,
  ANDROID_DEV_BETA_TABLET_NATIVE_NTP_CONTROL = 3311925,
  ANDROID_DEV_BETA_TABLET_NATIVE_NTP_TWO_URL_BARS = 3311926,
  ANDROID_STABLE_NATIVE_NTP_CONTROL_0 = 3311927,
  ANDROID_STABLE_NATIVE_NTP_CONTROL_1 = 3311928,
  ANDROID_STABLE_NATIVE_NTP_TWO_URL_BARS = 3311929,
  ANDROID_STABLE_NATIVE_NTP_ONE_URL_BAR = 3311930,
  ANDROID_STABLE_TABLET_NATIVE_NTP_CONTROL_0 = 3311931,
  ANDROID_STABLE_TABLET_NATIVE_NTP_TWO_URL_BARS = 3311932,
  ANDROID_STABLE_TABLET_NATIVE_NTP_CONTROL_1 = 3311933,
  ANDROID_M36_DEV_BETA_NATIVE_NTP_ONE_URL_BAR = 3311934,
  ANDROID_M36_DEV_BETA_TABLET_NATIVE_NTP_TWO_URL_BARS = 3311935,
  ANDROID_M36_STABLE_NATIVE_NTP_ONE_URL_BAR = 3311936,
  ANDROID_M36_STABLE_TABLET_NATIVE_NTP_TWO_URL_BARS = 3311937,

  // WebGLDebugRendererInfo trial.
  WEBGL_DEBUG_RENDERER_INFO_ENABLED = 3311941,
  WEBGL_DEBUG_RENDERER_INFO_CONTROL = 3311942,
  WEBGL_DEBUG_RENDERER_INFO_DISABLED = 3311943,

  NEW_USER_MANAGEMENT_ENABLED = 3311944,
  NEW_USER_MANAGEMENT_CONTROL = 3311945,
  NEW_USER_MANAGEMENT_DISABLED = 3311946,

  // Reserve 100 more IDs to be used by autocomplete dynamic field trials.
  // The dynamic field trials are activated by a call to
  // OmniboxFieldTrial::ActivateDynamicFieldTrials.
  // For more details, see
  // chrome/browser/omnibox/omnibox_field_trial.{h,cc}.
  AUTOCOMPLETE_DYNAMIC_FIELD_TRIAL_RANGE2_ID_MIN = 3311947,
  AUTOCOMPLETE_DYNAMIC_FIELD_TRIAL_RANGE2_ID_MAX = 3312046,

  // DEPRECATED - DO NOT USE
  // Name: IOSPhoneNewNTP
  // Range: 3312100 - 3312103, 3312112 - 3312113
  // Now retired.  But please don't reuse these IDs; they may taint
  // your experiment results.
  IOS_PHONE_NEW_NTP_2014_Q1_ID_MIN = 3312100,
  IOS_PHONE_NEW_NTP_2014_Q1_ID_MAX = 3312103,
  IOS_PHONE_NEW_NTP_2014_Q1_ID2_MIN = 3312112,
  IOS_PHONE_NEW_NTP_2014_Q1_ID2_MAX = 3312113,

  // iOS Phone New NTP trial.
  // Range: 3312047 - 3312050 (Beta); 3312100 - 3312103 (Stable)
  //        3312112 - 3312113 (Stable); 3312372 (Stable)
  IOS_PHONE_NEW_NTP_OMNIBOX_HINT_BETA = 3312047,
  IOS_PHONE_NEW_NTP_CONTROL_1_BETA = 3312048,
  IOS_PHONE_NEW_NTP_FAKEBOX_HINT_BETA = 3312049,
  IOS_PHONE_NEW_NTP_CONTROL_2_BETA = 3312050,
  IOS_PHONE_NEW_NTP_OMNIBOX_HINT_STABLE = 3312114,
  IOS_PHONE_NEW_NTP_CONTROL_1_STABLE = 3312115,
  IOS_PHONE_NEW_NTP_FAKEBOX_HINT_STABLE = 3312116,
  IOS_PHONE_NEW_NTP_CONTROL_2_STABLE = 3312117,
  IOS_PHONE_NEW_NTP_HOLDBACK_STABLE = 3312372,

  // iOS Tablet New NTP trial.
  // Range: 3312104 - 3312107 (Beta); 3312108 - 3312111 (Stable)
  //        3312373 (Stable)
  IOS_TABLET_NEW_NTP_OMNIBOX_HINT_BETA = 3312104,
  IOS_TABLET_NEW_NTP_CONTROL_1_BETA = 3312105,
  IOS_TABLET_NEW_NTP_FAKEBOX_HINT_BETA = 3312106,
  IOS_TABLET_NEW_NTP_CONTROL_2_BETA = 3312107,
  IOS_TABLET_NEW_NTP_OMNIBOX_HINT_STABLE = 3312108,
  IOS_TABLET_NEW_NTP_CONTROL_1_STABLE = 3312109,
  IOS_TABLET_NEW_NTP_FAKEBOX_HINT_STABLE = 3312110,
  IOS_TABLET_NEW_NTP_CONTROL_2_STABLE = 3312111,
  IOS_TABLET_NEW_NTP_HOLDBACK_STABLE = 3312373,

  // ExtensionInstallPrompt field trial.
  EXTENSION_INSTALL_PROMPT_EXPERIMENT_ID_MIN = 3312051,
  EXTENSION_INSTALL_PROMPT_EXPERIMENT_ID_MAX = 3312099,

  // <link rel=prefetch> field trial.
  LINK_REL_PREFETCH_ENABLED_1 = 3312118,
  LINK_REL_PREFETCH_ENABLED_2 = 3312119,
  LINK_REL_PREFETCH_DISABLED_1 = 3312120,
  LINK_REL_PREFETCH_DISABLED_2 = 3312121,

  // Reserve 200 more IDs to be used by autocomplete dynamic field trials.
  // The dynamic field trials are activated by a call to
  // OmniboxFieldTrial::ActivateDynamicFieldTrials.
  // For more details, see
  // chrome/browser/omnibox/omnibox_field_trial.{h,cc}.
  AUTOCOMPLETE_DYNAMIC_FIELD_TRIAL_RANGE3_ID_MIN = 3312122,
  AUTOCOMPLETE_DYNAMIC_FIELD_TRIAL_RANGE3_ID_MAX = 3312321,

  // Instant search clicks field trial.
  INSTANT_SEARCH_CLICKS_FIELD_TRIAL_ID_MIN = 3312322,
  INSTANT_SEARCH_CLICKS_FIELD_TRIAL_ID_MAX = 3312371,

  // iOS Phone modal dialog field trial.
  IOS_PHONE_NTP_MODAL_DIALOG_ENABLED_1 = 3312374,
  IOS_PHONE_NTP_MODAL_DIALOG_ENABLED_2 = 3312375,
  IOS_PHONE_NTP_MODAL_DIALOG_CONTROL_1 = 3312376,
  IOS_PHONE_NTP_MODAL_DIALOG_CONTROL_2 = 3312377,

  // Field trials can be queried directly from webrtc via webrtc::field_trial.
  // Thus new trials are added without explicit CLs in chromium repository.
  // This is the range of ids reserved for those trials.
  WEBRTC_FIELD_TRIAL_RANGE_ID_MIN = 3312378,
  WEBRTC_FIELD_TRIAL_RANGE_ID_MAX = 3312477,

  // SDCH compression field trial.
  SDCH_ENABLED_ALL = 3312478,
  SDCH_ENABLED_HTTP_ONLY = 3312479,

  // NEXT ID: When adding new IDs, please add them above this section, starting
  // with the value of NEXT_ID, and updating NEXT_ID to (end of your reserved
  // range) + 1.
  NEXT_ID = 3312480,

  // USABLE IDs END HERE.
  //
  // The largest possible Chrome variation ID in the reserved range. When
  // defining your variation IDs, DO NOT exceed this value - GWS will ignore
  // your experiment!
  MAXIMUM_ID = 3399999,
};

}  // namespace chrome_variations

#endif  // CHROME_COMMON_VARIATIONS_VARIATION_IDS_H_
