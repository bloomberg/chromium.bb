// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_METRICS_VARIATION_IDS_H_
#define CHROME_COMMON_METRICS_VARIATION_IDS_H_
#pragma once

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
// properties, reserve an ID by declaring them below. Please add a short
// description of the associate FieldTrial.
//
// Ex:
// // The Omnibox Instant Trial.
// kInstantTrialOn  = 3300123,
// kInstantTrialOff = 3300124,
//
// If you programatically generate FieldTrials, you can still use a loop to
// create your IDs. Just be sure to reserve the range of IDs here with a clear
// comment.
//
// Ex:
// // The 5% Uniformity Trial. This is a reserved range.
// kUniformityTrial5PercentStart = 330000,
// kUniformirtTrial5PercentEnd   = 330099,
//
// Anything within the range of a uint32 should be castable to an ID, but
// please ensure that they are within the range of the min and max values.
enum ID {
  // Used to represent no associated Chrome variation ID.
  kEmptyID = 0,

  // The smallest possible Chrome Variation ID in the reserved range. The
  // first 10,000 values are reserved for internal variations infrastructure
  // use. Please do not use values in this range.
  kMinimumID = 3300000,

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

  // USABLE IDs END HERE.
  //
  // The largest possible Chrome variation ID in the reserved range. When
  // defining your variation IDs, DO NOT exceed this value.
  kMaximumID = 3399999,
};

}  // namespace chrome_variations

#endif  // CHROME_COMMON_METRICS_VARIATION_IDS_H_
