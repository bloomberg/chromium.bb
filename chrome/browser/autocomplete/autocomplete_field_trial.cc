// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_field_trial.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/string_number_conversions.h"
#include "chrome/common/metrics/experiments_helper.h"
#include "chrome/common/metrics/variation_ids.h"

namespace {

// Field trial names.
static const char kDisallowInlineHQPFieldTrialName[] =
    "OmniboxDisallowInlineHQP";
static const char kSuggestFieldTrialName[] = "OmniboxSearchSuggest";

// Field trial experiment probabilities.

// For inline History Quick Provider field trial, put 10% ( = 10/100 )
// of the users in the disallow-inline experiment group.
const base::FieldTrial::Probability kDisallowInlineHQPFieldTrialDivisor = 100;
const base::FieldTrial::Probability
    kDisallowInlineHQPFieldTrialExperimentFraction = 10;

// For the search suggestion field trial, divide the people in the
// trial into 20 equally-sized buckets.  The suggest provider backend
// will decide what behavior (if any) to change based on the group.
const int kSuggestFieldTrialNumberOfGroups = 20;

// Field trial IDs.
// Though they are not literally "const", they are set only once, in
// Activate() below.

// Field trial ID for the disallow-inline History Quick Provider
// experiment group.
int disallow_inline_hqp_experiment_group = 0;

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
    // Create inline History Quick Provider field trial.
    // Make it expire on November 8, 2012.
    scoped_refptr<base::FieldTrial> trial(
        base::FieldTrialList::FactoryGetFieldTrial(
        kDisallowInlineHQPFieldTrialName, kDisallowInlineHQPFieldTrialDivisor,
        "Standard", 2012, 11, 8, NULL));
    trial->UseOneTimeRandomization();
    disallow_inline_hqp_experiment_group = trial->AppendGroup("DisallowInline",
        kDisallowInlineHQPFieldTrialExperimentFraction);
  }

  // Create the suggest field trial (regardless of sticky-ness status, but
  // make it sticky if possible).
  // Make it expire on October 1, 2012.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
        kSuggestFieldTrialName, kSuggestFieldTrialNumberOfGroups,
        "0", 2012, 10, 1, NULL));
  if (base::FieldTrialList::IsOneTimeRandomizationEnabled())
    trial->UseOneTimeRandomization();

  // Mark this group in suggest requests to Google.
  experiments_helper::AssociateGoogleVariationID(
      kSuggestFieldTrialName, "0", chrome_variations::kSuggestIDMin);
  DCHECK_EQ(kSuggestFieldTrialNumberOfGroups,
      chrome_variations::kSuggestIDMax - chrome_variations::kSuggestIDMin + 1);

  // We've already created one group; now just need to create
  // kSuggestFieldTrialNumGroups - 1 more. Mark these groups in
  // suggest requests to Google.
  for (int i = 1; i < kSuggestFieldTrialNumberOfGroups; i++) {
    const std::string group_name = base::IntToString(i);
    trial->AppendGroup(group_name, 1);
    experiments_helper::AssociateGoogleVariationID(
        kSuggestFieldTrialName, group_name,
        static_cast<chrome_variations::ID>(
            chrome_variations::kSuggestIDMin + i));
  }
}

bool AutocompleteFieldTrial::InDisallowInlineHQPFieldTrial() {
  return base::FieldTrialList::TrialExists(kDisallowInlineHQPFieldTrialName);
}

bool AutocompleteFieldTrial::InDisallowInlineHQPFieldTrialExperimentGroup() {
  if (!base::FieldTrialList::TrialExists(kDisallowInlineHQPFieldTrialName))
    return false;

  // Return true if we're in the experiment group.
  const int group = base::FieldTrialList::FindValue(
      kDisallowInlineHQPFieldTrialName);
  return group == disallow_inline_hqp_experiment_group;
}

bool AutocompleteFieldTrial::InSuggestFieldTrial() {
  return base::FieldTrialList::TrialExists(kSuggestFieldTrialName);
}

std::string AutocompleteFieldTrial::GetSuggestGroupName() {
  return base::FieldTrialList::FindFullName(kSuggestFieldTrialName);
}

// Yes, this is roundabout.  It's easier to provide the group number as
// a string (simply by choosing group names appropriately) than provide
// it as an integer.  It might look more straightforward to use group ids
// for the group number with respect to suggest.  However, we don't want
// to assume that group ids are creates as 0, 1, 2, ... -- this isn't part
// of the field_trial.h specification.  Hence, we use the group names to
// get numbers that we know are 0, 1, 2, ...
int AutocompleteFieldTrial::GetSuggestGroupNameAsNumber() {
  int group_num;
  base::StringToInt(GetSuggestGroupName(), &group_num);
  return group_num;
}

int AutocompleteFieldTrial::GetSuggestNumberOfGroups() {
  return kSuggestFieldTrialNumberOfGroups;
}
