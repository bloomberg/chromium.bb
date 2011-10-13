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
// the remaining users:
//
// INSTANT_EXPERIMENT: Instant is enabled, but only for search. Instant is also
//     preloaded. If the user hasn't opted to send metrics (UMA) data, they are
//     bounced back to INACTIVE.
//
// INSTANT_CONTROL: Instant remains disabled, as is default. If the user hasn't
//     opted to send metrics (UMA) data, they are bounced back to INACTIVE.
//
// HIDDEN_EXPERIMENT: Instant is enabled in "search only" mode, but queries are
//     issued only when the user presses <Enter>. No previews are shown.
//
// HIDDEN_CONTROL: Instant remains disabled, as is default.
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

    INSTANT_CONTROL_A,
    INSTANT_CONTROL_B,
    INSTANT_EXPERIMENT_A,
    INSTANT_EXPERIMENT_B,

    HIDDEN_CONTROL_A,
    HIDDEN_CONTROL_B,
    HIDDEN_EXPERIMENT_A,
    HIDDEN_EXPERIMENT_B,
  };

  // Activate the field trial. Before this call, all calls to GetGroup will
  // return INACTIVE. *** MUST NOT BE CALLED MORE THAN ONCE. ***
  static void Activate();

  // Return the field trial group this profile belongs to.
  static Group GetGroup(Profile* profile);

  // Check if the user is in one of the EXPERIMENT groups.
  static bool IsExperimentGroup(Profile* profile);

  // Check if the user is in the INSTANT_EXPERIMENT group.
  static bool IsInstantExperiment(Profile* profile);

  // Check if the user is in the HIDDEN_EXPERIMENT group.
  static bool IsHiddenExperiment(Profile* profile);

  // Returns a string describing the user's group. Can be added to histogram
  // names, to split histograms by field trial groups.
  static std::string GetGroupName(Profile* profile);

 // Returns a string denoting the user's group, for adding as a URL param.
 static std::string GetGroupAsUrlParam(Profile* profile);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InstantFieldTrial);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_FIELD_TRIAL_H_
