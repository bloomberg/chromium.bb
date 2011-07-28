// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_FIELD_TRIAL_H_
#define CHROME_BROWSER_INSTANT_INSTANT_FIELD_TRIAL_H_

#include <string>

#include "base/basictypes.h"

class Profile;

// This class manages the Instant field trial. Each user is in exactly one of
// two field trial groups: Control or Experiment.
// - Control and Experiment are all the other users, i.e., those who have never
//   touched the Preferences option. Some percentage of these users are chosen
//   into the Experiment group and get Instant enabled automatically. The rest
//   fall into the Control group; for them, Instant remains disabled by default.
// - Control and Experiment are further split into two subgroups each, in order
//   to detect bias between them (when analyzing metrics). The subgroups are
//   treated identically for all other purposes.
//
// There is a possibility that the field trial is not active.
// - Inactive users are those who have played with the Instant option in the
//   Preferences page, or those for whom group policy provides an override, or
//   those with an incognito profile, etc. The field trial is inactive for such
//   users, so their Instant preference setting is respected. The field trial is
//   initially inactive so testing is not affected by the field trial.
class InstantFieldTrial {
 public:
  enum Group {
    INACTIVE,
    CONTROL1,
    CONTROL2,
    EXPERIMENT1,
    EXPERIMENT2,
  };

  // Activate the field trial. Before this call, all calls to GetGroup will
  // return INACTIVE.
  static void Activate();

  // Return the field trial group this profile belongs to.
  static Group GetGroup(Profile* profile);

  // Check if the group is either of the two experiment subgroups.
  static bool IsExperimentGroup(Profile* profile);

  // Returns a string describing the user's group. Can be added to histogram
  // names, to split histograms by field trial groups.
  static std::string GetGroupName(Profile* profile);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InstantFieldTrial);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_FIELD_TRIAL_H_
