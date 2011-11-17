// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
// the remaining users. Instant is enabled with preloading for the EXPERIMENT
// groups. It remains disabled, as is default, for the CONTROL groups.
//
// INSTANT_EXPERIMENT: Queries are issued as the user types, and previews are
//     shown. If the user hasn't opted to send metrics (UMA) data, they are
//     bounced back to INACTIVE.
//
// HIDDEN_EXPERIMENT: Queries are issued as the user types, but no preview is
//     shown until they press <Enter>. If the user hasn't opted to send metrics
//     (UMA) data, they are bounced back to INACTIVE. Suggestions obtained from
//     Instant are not propagated back to the omnibox.
//
// SILENT_EXPERIMENT: No queries are issued until the user presses <Enter>. No
//     previews are shown. The user is not required to send metrics (UMA) data.
//
// SUGGEST_EXPERIMENT: Same as HIDDEN, except that the Instant suggestions are
//     autocompleted inline into the omnibox.
//
// UMA_CONTROL: Instant is disabled. If the user hasn't opted to send metrics
//     (UMA) data, they are bounced back to INACTIVE.
//
// ALL_CONTROL: Instant is disabled. The user is not required to send metrics
//     (UMA) data.
//
// Users not chosen into any of the above groups are INACTIVE.
//
// Each non-INACTIVE group is split into two equal subgroups, to detect bias
// between them when analyzing metrics. The subgroups are denoted by "_A" and
// "_B" suffixes, and are treated identically for all other purposes.
class InstantFieldTrial {
 public:
  enum Group {
    INACTIVE,

    INSTANT_EXPERIMENT_A,
    INSTANT_EXPERIMENT_B,

    HIDDEN_EXPERIMENT_A,
    HIDDEN_EXPERIMENT_B,

    SILENT_EXPERIMENT_A,
    SILENT_EXPERIMENT_B,

    SUGGEST_EXPERIMENT_A,
    SUGGEST_EXPERIMENT_B,

    UMA_CONTROL_A,
    UMA_CONTROL_B,

    ALL_CONTROL_A,
    ALL_CONTROL_B,
  };

  // Activate the field trial. Before this call, all calls to GetGroup will
  // return INACTIVE. *** MUST NOT BE CALLED MORE THAN ONCE. ***
  static void Activate();

  // Return the field trial group this profile belongs to.
  static Group GetGroup(Profile* profile);

  // Check if the user is in any of the EXPERIMENT groups.
  static bool IsInstantExperiment(Profile* profile);

  // Check if the user is in the HIDDEN, SILENT or SUGGEST EXPERIMENT groups.
  static bool IsHiddenExperiment(Profile* profile);

  // Check if the user is in the SILENT EXPERIMENT group.
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
