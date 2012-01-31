// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_field_trial.h"

#include <string>

#include "base/metrics/field_trial.h"

namespace {

// Field trial names.
static const char kAggressiveHUPFieldTrialName[] =
    "OmniboxAggressiveHistoryURLProvider";

// Field trial experiment probabilities.

// For aggressive History URL Provider field trial, put 50% ( = 50/100 )
// of the users in the aggressive experiment group.
const base::FieldTrial::Probability kAggressiveHUPFieldTrialDivisor = 100;
const base::FieldTrial::Probability
    kAggressiveHUPFieldTrialExperimentFraction = 50;

// Field trial IDs.
// Though they are not literally "const", they are set only once, in
// Activate() below.

// Field trial ID for the aggressive History URL Provider experiment group.
int aggressive_hup_experiment_group = 0;

}


void AutocompleteFieldTrial::Activate() {
  // Because users tend to use omnibox without attention to it--habits
  // get ingrained, users tend to learn that a particular suggestion is
  // at a particular spot in the drop-down--we're going to make these
  // field trials sticky.  We want users to stay in them once assigned
  // so they have a better experience and also so we don't get weird
  // effects as omnibox ranking keeps changing and users learn they can't
  // trust the omnibox.  Hence, to create the field trials we require
  // that field trials can be made sticky.
  if (base::FieldTrialList::IsOneTimeRandomizationEnabled()) {  // sticky trials
    // Create aggressive History URL Provider field trial.
    // Make it expire on August 1, 2012.
    scoped_refptr<base::FieldTrial> trial(new base::FieldTrial(
        kAggressiveHUPFieldTrialName, kAggressiveHUPFieldTrialDivisor,
        "Standard", 2012, 8, 1));
    trial->UseOneTimeRandomization();
    aggressive_hup_experiment_group = trial->AppendGroup("Aggressive",
        kAggressiveHUPFieldTrialExperimentFraction);
  }
}

bool AutocompleteFieldTrial::InAggressiveHUPFieldTrial() {
  return base::FieldTrialList::TrialExists(kAggressiveHUPFieldTrialName);
}

bool AutocompleteFieldTrial::InAggressiveHUPFieldTrialExperimentGroup() {
  if (!base::FieldTrialList::TrialExists(kAggressiveHUPFieldTrialName))
    return false;

  // Return true if we're in the aggressive experiment group.
  const int group = base::FieldTrialList::FindValue(
      kAggressiveHUPFieldTrialName);
  return group == aggressive_hup_experiment_group;
}
