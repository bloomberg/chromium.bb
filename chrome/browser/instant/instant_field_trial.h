// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_FIELD_TRIAL_H_
#define CHROME_BROWSER_INSTANT_INSTANT_FIELD_TRIAL_H_

#include <string>

#include "base/basictypes.h"

class Profile;

// This class manages the Instant field trial.
//
// If a user (profile) has an explicit preference for Instant, having disabled
// or enabled it in the Preferences page, or by having a group policy override,
// the field trial is INACTIVE for them. There is no change in behaviour. Their
// Instant preference is respected. Incognito profiles are also INACTIVE.
//
// The following mutually exclusive groups each select a small random sample of
// the remaining users. Instant is enabled with preloading of the search engine
// homepage for all the experiment groups. It remains disabled, as is default,
// for the CONTROL group.
//
// INSTANT: Queries are issued as the user types. Predicted query is inline
//     autocompleted into the omnibox. Result previews are shown.
//
// SUGGEST: Same as INSTANT, without the previews.
//
// HIDDEN: Same as SUGGEST, without the inline autocompletion.
//
// SILENT: Same as HIDDEN, without issuing queries as the user types. The query
//     is sent only after the user presses <Enter>.
//
// CONTROL: Instant is disabled.
//
// Users not chosen into any of the above groups are INACTIVE.
class InstantFieldTrial {
 public:
  enum Group {
    INACTIVE,
    INSTANT,
    SUGGEST,
    HIDDEN,
    SILENT,
    CONTROL
  };

  // Activate the field trial. Before this call, all calls to GetGroup will
  // return INACTIVE. *** MUST NOT BE CALLED MORE THAN ONCE. ***
  static void Activate();

  // Return the field trial group this profile belongs to.
  static Group GetGroup(Profile* profile);

  // Check if the user is in any of the experiment groups.
  static bool IsInstantExperiment(Profile* profile);

  // Check if the user is in the SUGGEST, HIDDEN or SILENT groups.
  static bool IsHiddenExperiment(Profile* profile);

  // Check if the user is in the SILENT group.
  static bool IsSilentExperiment(Profile* profile);

  // Returns a string describing the user's group. Can be added to histogram
  // names, to split histograms by field trial groups.
  static std::string GetGroupName(Profile* profile);

  // Returns a string denoting the user's group, for adding as a URL param.
  static std::string GetGroupAsUrlParam(Profile* profile);

  // Returns whether the Instant suggested text should be autocompleted inline
  // into the omnibox.
  static bool ShouldSetSuggestedText(Profile* profile);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InstantFieldTrial);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_FIELD_TRIAL_H_
