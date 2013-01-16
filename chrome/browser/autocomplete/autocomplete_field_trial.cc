// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_field_trial.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/string_number_conversions.h"
#include "chrome/common/metrics/metrics_util.h"
#include "chrome/common/metrics/variations/variation_ids.h"
#include "chrome/common/metrics/variations/variations_util.h"

namespace {

// Field trial names.
static const char kDisallowInlineHQPFieldTrialName[] =
    "OmniboxDisallowInlineHQP";
// Because we regularly change the name of the suggest field trial in
// order to shuffle users among groups, we use the date the current trial
// was created as part of the name.
static const char kSuggestFieldTrialStarted2013Q1Name[] =
    "OmniboxSearchSuggestTrialStarted2013Q1";
static const char kHQPNewScoringFieldTrialName[] =
    "OmniboxHQPNewScoringMax1400";
static const char kHUPCullRedirectsFieldTrialName[] = "OmniboxHUPCullRedirects";
static const char kHUPCreateShorterMatchFieldTrialName[] =
    "OmniboxHUPCreateShorterMatch";
static const char kHQPReplaceHUPScoringFieldTrialName[] =
    "OmniboxHQPReplaceHUPRearrangeNumComponents";
static const char kHQPOnlyCountMatchesAtWordBoundariesFieldTrialName[] =
    "OmniboxHQPOnlyCountMatchesAtWordBoundaries";

// Field trial experiment probabilities.

// For inline History Quick Provider field trial, put 0% ( = 0/100 )
// of the users in the disallow-inline experiment group.
const base::FieldTrial::Probability kDisallowInlineHQPFieldTrialDivisor = 100;
const base::FieldTrial::Probability
    kDisallowInlineHQPFieldTrialExperimentFraction = 0;

// For the search suggestion field trial, divide the people in the
// trial into 20 equally-sized buckets.  The suggest provider backend
// will decide what behavior (if any) to change based on the group.
const int kSuggestFieldTrialNumberOfGroups = 20;

// For History Quick Provider new scoring field trial, put 25% ( = 25/100 )
// of the users in the new scoring experiment group.
const base::FieldTrial::Probability kHQPNewScoringFieldTrialDivisor = 100;
const base::FieldTrial::Probability
    kHQPNewScoringFieldTrialExperimentFraction = 25;

// For HistoryURL provider cull redirects field trial, put 0% ( = 0/100 )
// of the users in the don't-cull-redirects experiment group.
// TODO(mpearson): Remove this field trial and the code it uses once I'm
// sure it's no longer needed.
const base::FieldTrial::Probability kHUPCullRedirectsFieldTrialDivisor = 100;
const base::FieldTrial::Probability
    kHUPCullRedirectsFieldTrialExperimentFraction = 0;

// For HistoryURL provider create shorter match field trial, put 0%
// ( = 25/100 ) of the users in the don't-create-a-shorter-match
// experiment group.
// TODO(mpearson): Remove this field trial and the code it uses once I'm
// sure it's no longer needed.
const base::FieldTrial::Probability
    kHUPCreateShorterMatchFieldTrialDivisor = 100;
const base::FieldTrial::Probability
    kHUPCreateShorterMatchFieldTrialExperimentFraction = 0;

// For the field trial that removes searching/scoring URLs from
// HistoryURL provider and adds a HistoryURL-provider-like scoring
// mode to HistoryQuick provider, put 25% ( = 25/100 ) of the users in
// the experiment group.
const base::FieldTrial::Probability
    kHQPReplaceHUPScoringFieldTrialDivisor = 100;
const base::FieldTrial::Probability
    kHQPReplaceHUPScoringFieldTrialExperimentFraction = 25;

// For the field trial that ignores all mid-term matches in HistoryQuick
// provider, put 25% ( = 25/100 ) of the users in the experiment group.
const base::FieldTrial::Probability
    kHQPOnlyCountMatchesAtWordBoundariesFieldTrialDivisor = 100;
const base::FieldTrial::Probability
    kHQPOnlyCountMatchesAtWordBoundariesFieldTrialExperimentFraction = 25;


// Field trial IDs.
// Though they are not literally "const", they are set only once, in
// Activate() below.

// Field trial ID for the disallow-inline History Quick Provider
// experiment group.
int disallow_inline_hqp_experiment_group = 0;

// Field trial ID for the History Quick Provider new scoring experiment group.
int hqp_new_scoring_experiment_group = 0;

// Field trial ID for the HistoryURL provider cull redirects experiment group.
int hup_dont_cull_redirects_experiment_group = 0;

// Field trial ID for the HistoryURL provider create shorter match
// experiment group.
int hup_dont_create_shorter_match_experiment_group = 0;

// Field trial ID for the HistoryQuick provider replaces HistoryURL provider
// experiment group.
int hqp_replace_hup_scoring_experiment_group = 0;

// Field trial ID for the HistoryQuick provider only count matches at
// word boundaries experiment group.
int hqp_only_count_matches_at_word_boundaries_experiment_group = 0;

}


void AutocompleteFieldTrial::Activate() {
  // Create inline History Quick Provider field trial.
  // Make it expire on November 8, 2012.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
      kDisallowInlineHQPFieldTrialName, kDisallowInlineHQPFieldTrialDivisor,
      "Standard", 2012, 11, 8, NULL));
  // Because users tend to use omnibox without attention to it--habits
  // get ingrained, users tend to learn that a particular suggestion is
  // at a particular spot in the drop-down--we're going to make these
  // field trials sticky.  We want users to stay in them once assigned
  // so they have a better experience and also so we don't get weird
  // effects as omnibox ranking keeps changing and users learn they can't
  // trust the omnibox.
  trial->UseOneTimeRandomization();
  disallow_inline_hqp_experiment_group = trial->AppendGroup("DisallowInline",
      kDisallowInlineHQPFieldTrialExperimentFraction);

  // Create the suggest field trial.
  // Make it expire on September 1, 2013.
  trial = base::FieldTrialList::FactoryGetFieldTrial(
      kSuggestFieldTrialStarted2013Q1Name, kSuggestFieldTrialNumberOfGroups,
      "0", 2013, 9, 1, NULL);
  trial->UseOneTimeRandomization();

  // Mark this group in suggest requests to Google.
  chrome_variations::AssociateGoogleVariationID(
      chrome_variations::GOOGLE_WEB_PROPERTIES,
      kSuggestFieldTrialStarted2013Q1Name, "0",
      chrome_variations::SUGGEST_TRIAL_STARTED_2013_Q1_ID_MIN);
  DCHECK_EQ(kSuggestFieldTrialNumberOfGroups,
      chrome_variations::SUGGEST_TRIAL_STARTED_2013_Q1_ID_MAX -
      chrome_variations::SUGGEST_TRIAL_STARTED_2013_Q1_ID_MIN + 1);

  // We've already created one group; now just need to create
  // kSuggestFieldTrialNumGroups - 1 more. Mark these groups in
  // suggest requests to Google.
  for (int i = 1; i < kSuggestFieldTrialNumberOfGroups; i++) {
    const std::string group_name = base::IntToString(i);
    trial->AppendGroup(group_name, 1);
    chrome_variations::AssociateGoogleVariationID(
        chrome_variations::GOOGLE_WEB_PROPERTIES,
        kSuggestFieldTrialStarted2013Q1Name, group_name,
        static_cast<chrome_variations::VariationID>(
            chrome_variations::SUGGEST_TRIAL_STARTED_2013_Q1_ID_MIN + i));
  }

  // Make sure that we participate in the suggest experiment by calling group()
  // on the newly created field trial.  This is necessary to activate the field
  // trial group as there are no more references to it within the Chrome code.
  trial->group();

  // Create inline History Quick Provider new scoring field trial.
  // Make it expire on April 14, 2013.
  trial = base::FieldTrialList::FactoryGetFieldTrial(
      kHQPNewScoringFieldTrialName, kHQPNewScoringFieldTrialDivisor,
      "Standard", 2013, 4, 14, NULL);
  trial->UseOneTimeRandomization();
  hqp_new_scoring_experiment_group = trial->AppendGroup("NewScoring",
      kHQPNewScoringFieldTrialExperimentFraction);

  // Create the HistoryURL provider cull redirects field trial.
  // Make it expire on March 1, 2013.
  trial = base::FieldTrialList::FactoryGetFieldTrial(
      kHUPCullRedirectsFieldTrialName, kHUPCullRedirectsFieldTrialDivisor,
      "Standard", 2013, 3, 1, NULL);
  trial->UseOneTimeRandomization();
  hup_dont_cull_redirects_experiment_group =
      trial->AppendGroup("DontCullRedirects",
                         kHUPCullRedirectsFieldTrialExperimentFraction);

  // Create the HistoryURL provider create shorter match field trial.
  // Make it expire on March 1, 2013.
  trial = base::FieldTrialList::FactoryGetFieldTrial(
      kHUPCreateShorterMatchFieldTrialName,
      kHUPCreateShorterMatchFieldTrialDivisor, "Standard", 2013, 3, 1, NULL);
  trial->UseOneTimeRandomization();
  hup_dont_create_shorter_match_experiment_group =
      trial->AppendGroup("DontCreateShorterMatch",
                         kHUPCreateShorterMatchFieldTrialExperimentFraction);

  // Create the field trial that makes HistoryQuick provider score
  // some results like HistoryURL provider and simultaneously disable
  // HistoryURL provider from searching the URL database.  Make it
  // expire on June 23, 2013.
  trial = base::FieldTrialList::FactoryGetFieldTrial(
    kHQPReplaceHUPScoringFieldTrialName, kHQPReplaceHUPScoringFieldTrialDivisor,
      "Standard", 2013, 6, 23, NULL);
  trial->UseOneTimeRandomization();
  hqp_replace_hup_scoring_experiment_group = trial->AppendGroup("HQPReplaceHUP",
      kHQPReplaceHUPScoringFieldTrialExperimentFraction);

  // Create the field trial that makes HistoryQuick provider score
  // ignore all matches that happen in the middle of a word.  Make it
  // expire on June 23, 2013.
  trial = base::FieldTrialList::FactoryGetFieldTrial(
      kHQPOnlyCountMatchesAtWordBoundariesFieldTrialName,
      kHQPOnlyCountMatchesAtWordBoundariesFieldTrialDivisor,
      "Standard", 2013, 6, 23, NULL);
  trial->UseOneTimeRandomization();
  hqp_only_count_matches_at_word_boundaries_experiment_group =
      trial->AppendGroup("HQPOnlyCountMatchesAtWordBoundaries",
          kHQPOnlyCountMatchesAtWordBoundariesFieldTrialExperimentFraction);
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

bool AutocompleteFieldTrial::GetActiveSuggestFieldTrialHash(
    uint32* field_trial_hash) {
  if (!base::FieldTrialList::TrialExists(kSuggestFieldTrialStarted2013Q1Name))
    return false;

  *field_trial_hash = metrics::HashName(kSuggestFieldTrialStarted2013Q1Name);
  return true;
}

bool AutocompleteFieldTrial::InHQPNewScoringFieldTrial() {
  return base::FieldTrialList::TrialExists(kHQPNewScoringFieldTrialName);
}

bool AutocompleteFieldTrial::InHQPNewScoringFieldTrialExperimentGroup() {
  if (!InHQPNewScoringFieldTrial())
    return false;

  // Return true if we're in the experiment group.
  const int group = base::FieldTrialList::FindValue(
      kHQPNewScoringFieldTrialName);
  return group == hqp_new_scoring_experiment_group;
}

bool AutocompleteFieldTrial::InHUPCullRedirectsFieldTrial() {
  return base::FieldTrialList::TrialExists(kHUPCullRedirectsFieldTrialName);
}

bool AutocompleteFieldTrial::InHUPCullRedirectsFieldTrialExperimentGroup() {
  if (!base::FieldTrialList::TrialExists(kHUPCullRedirectsFieldTrialName))
    return false;

  // Return true if we're in the experiment group.
  const int group = base::FieldTrialList::FindValue(
      kHUPCullRedirectsFieldTrialName);
  return group == hup_dont_cull_redirects_experiment_group;
}

bool AutocompleteFieldTrial::InHUPCreateShorterMatchFieldTrial() {
  return
      base::FieldTrialList::TrialExists(kHUPCreateShorterMatchFieldTrialName);
}

bool AutocompleteFieldTrial::
    InHUPCreateShorterMatchFieldTrialExperimentGroup() {
  if (!base::FieldTrialList::TrialExists(kHUPCreateShorterMatchFieldTrialName))
    return false;

  // Return true if we're in the experiment group.
  const int group = base::FieldTrialList::FindValue(
      kHUPCreateShorterMatchFieldTrialName);
  return group == hup_dont_create_shorter_match_experiment_group;
}

bool AutocompleteFieldTrial::InHQPReplaceHUPScoringFieldTrial() {
  return base::FieldTrialList::TrialExists(kHQPReplaceHUPScoringFieldTrialName);
}

bool AutocompleteFieldTrial::InHQPReplaceHUPScoringFieldTrialExperimentGroup() {
  if (!InHQPReplaceHUPScoringFieldTrial())
    return false;

  // Return true if we're in the experiment group.
  const int group = base::FieldTrialList::FindValue(
      kHQPReplaceHUPScoringFieldTrialName);
  return group == hqp_replace_hup_scoring_experiment_group;
}

bool AutocompleteFieldTrial::InHQPOnlyCountMatchesAtWordBoundariesFieldTrial() {
  return base::FieldTrialList::TrialExists(
      kHQPOnlyCountMatchesAtWordBoundariesFieldTrialName);
}

bool AutocompleteFieldTrial::
    InHQPOnlyCountMatchesAtWordBoundariesFieldTrialExperimentGroup() {
  if (!InHQPOnlyCountMatchesAtWordBoundariesFieldTrial())
    return false;

  // Return true if we're in the experiment group.
  const int group = base::FieldTrialList::FindValue(
      kHQPOnlyCountMatchesAtWordBoundariesFieldTrialName);
  return group == hqp_only_count_matches_at_word_boundaries_experiment_group;
}
