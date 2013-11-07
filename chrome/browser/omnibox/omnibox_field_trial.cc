// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/omnibox_field_trial.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/metrics/variations/variation_ids.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "components/variations/metrics_util.h"

namespace {

// Field trial names.
const char kHUPCullRedirectsFieldTrialName[] = "OmniboxHUPCullRedirects";
const char kHUPCreateShorterMatchFieldTrialName[] =
    "OmniboxHUPCreateShorterMatch";
const char kStopTimerFieldTrialName[] = "OmniboxStopTimer";
const char kEnableZeroSuggestGroupPrefix[] = "EnableZeroSuggest";
const char kEnableZeroSuggestMostVisitedGroupPrefix[] =
    "EnableZeroSuggestMostVisited";
const char kEnableZeroSuggestAfterTypingGroupPrefix[] =
    "EnableZeroSuggestAfterTyping";

// The autocomplete dynamic field trial name prefix.  Each field trial is
// configured dynamically and is retrieved automatically by Chrome during
// the startup.
const char kAutocompleteDynamicFieldTrialPrefix[] = "AutocompleteDynamicTrial_";
// The maximum number of the autocomplete dynamic field trials (aka layers).
const int kMaxAutocompleteDynamicFieldTrials = 5;

// Field trial experiment probabilities.

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

// Experiment group names.

const char kStopTimerExperimentGroupName[] = "UseStopTimer";

// Field trial IDs.
// Though they are not literally "const", they are set only once, in
// ActivateStaticTrials() below.

// Whether the static field trials have been initialized by
// ActivateStaticTrials() method.
bool static_field_trials_initialized = false;

// Field trial ID for the HistoryURL provider cull redirects experiment group.
int hup_dont_cull_redirects_experiment_group = 0;

// Field trial ID for the HistoryURL provider create shorter match
// experiment group.
int hup_dont_create_shorter_match_experiment_group = 0;


// Concatenates the autocomplete dynamic field trial prefix with a field trial
// ID to form a complete autocomplete field trial name.
std::string DynamicFieldTrialName(int id) {
  return base::StringPrintf("%s%d", kAutocompleteDynamicFieldTrialPrefix, id);
}

}  // namespace


void OmniboxFieldTrial::ActivateStaticTrials() {
  DCHECK(!static_field_trials_initialized);

  // Create the HistoryURL provider cull redirects field trial.
  // Make it expire on March 1, 2013.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kHUPCullRedirectsFieldTrialName, kHUPCullRedirectsFieldTrialDivisor,
          "Standard", 2013, 3, 1, base::FieldTrial::ONE_TIME_RANDOMIZED, NULL));
  hup_dont_cull_redirects_experiment_group =
      trial->AppendGroup("DontCullRedirects",
                         kHUPCullRedirectsFieldTrialExperimentFraction);

  // Create the HistoryURL provider create shorter match field trial.
  // Make it expire on March 1, 2013.
  trial = base::FieldTrialList::FactoryGetFieldTrial(
      kHUPCreateShorterMatchFieldTrialName,
      kHUPCreateShorterMatchFieldTrialDivisor, "Standard", 2013, 3, 1,
      base::FieldTrial::ONE_TIME_RANDOMIZED, NULL);
  hup_dont_create_shorter_match_experiment_group =
      trial->AppendGroup("DontCreateShorterMatch",
                         kHUPCreateShorterMatchFieldTrialExperimentFraction);

  static_field_trials_initialized = true;
}

void OmniboxFieldTrial::ActivateDynamicTrials() {
  // Initialize all autocomplete dynamic field trials.  This method may be
  // called multiple times.
  for (int i = 0; i < kMaxAutocompleteDynamicFieldTrials; ++i)
    base::FieldTrialList::FindValue(DynamicFieldTrialName(i));
}

int OmniboxFieldTrial::GetDisabledProviderTypes() {
  // Make sure that Autocomplete dynamic field trials are activated.  It's OK to
  // call this method multiple times.
  ActivateDynamicTrials();

  // Look for group names in form of "DisabledProviders_<mask>" where "mask"
  // is a bitmap of disabled provider types (AutocompleteProvider::Type).
  int provider_types = 0;
  for (int i = 0; i < kMaxAutocompleteDynamicFieldTrials; ++i) {
    std::string group_name = base::FieldTrialList::FindFullName(
        DynamicFieldTrialName(i));
    const char kDisabledProviders[] = "DisabledProviders_";
    if (!StartsWithASCII(group_name, kDisabledProviders, true))
      continue;
    int types = 0;
    if (!base::StringToInt(base::StringPiece(
            group_name.substr(strlen(kDisabledProviders))), &types))
      continue;
    provider_types |= types;
  }
  return provider_types;
}

void OmniboxFieldTrial::GetActiveSuggestFieldTrialHashes(
    std::vector<uint32>* field_trial_hashes) {
  field_trial_hashes->clear();
  for (int i = 0; i < kMaxAutocompleteDynamicFieldTrials; ++i) {
    const std::string& trial_name = DynamicFieldTrialName(i);
    if (base::FieldTrialList::TrialExists(trial_name))
      field_trial_hashes->push_back(metrics::HashName(trial_name));
  }
}

bool OmniboxFieldTrial::InHUPCullRedirectsFieldTrial() {
  return base::FieldTrialList::TrialExists(kHUPCullRedirectsFieldTrialName);
}

bool OmniboxFieldTrial::InHUPCullRedirectsFieldTrialExperimentGroup() {
  if (!base::FieldTrialList::TrialExists(kHUPCullRedirectsFieldTrialName))
    return false;

  // Return true if we're in the experiment group.
  const int group = base::FieldTrialList::FindValue(
      kHUPCullRedirectsFieldTrialName);
  return group == hup_dont_cull_redirects_experiment_group;
}

bool OmniboxFieldTrial::InHUPCreateShorterMatchFieldTrial() {
  return
      base::FieldTrialList::TrialExists(kHUPCreateShorterMatchFieldTrialName);
}

bool OmniboxFieldTrial::InHUPCreateShorterMatchFieldTrialExperimentGroup() {
  if (!base::FieldTrialList::TrialExists(kHUPCreateShorterMatchFieldTrialName))
    return false;

  // Return true if we're in the experiment group.
  const int group = base::FieldTrialList::FindValue(
      kHUPCreateShorterMatchFieldTrialName);
  return group == hup_dont_create_shorter_match_experiment_group;
}

bool OmniboxFieldTrial::InStopTimerFieldTrialExperimentGroup() {
  return (base::FieldTrialList::FindFullName(kStopTimerFieldTrialName) ==
          kStopTimerExperimentGroupName);
}

bool OmniboxFieldTrial::HasDynamicFieldTrialGroupPrefix(
    const char* group_prefix) {
  // Make sure that Autocomplete dynamic field trials are activated.  It's OK to
  // call this method multiple times.
  ActivateDynamicTrials();

  // Look for group names starting with |group_prefix|.
  for (int i = 0; i < kMaxAutocompleteDynamicFieldTrials; ++i) {
    const std::string& group_name = base::FieldTrialList::FindFullName(
        DynamicFieldTrialName(i));
    if (StartsWithASCII(group_name, group_prefix, true))
      return true;
  }
  return false;
}

bool OmniboxFieldTrial::InZeroSuggestFieldTrial() {
  return HasDynamicFieldTrialGroupPrefix(kEnableZeroSuggestGroupPrefix);
}

bool OmniboxFieldTrial::InZeroSuggestMostVisitedFieldTrial() {
  return HasDynamicFieldTrialGroupPrefix(
      kEnableZeroSuggestMostVisitedGroupPrefix);
}

bool OmniboxFieldTrial::InZeroSuggestAfterTypingFieldTrial() {
  return HasDynamicFieldTrialGroupPrefix(
      kEnableZeroSuggestAfterTypingGroupPrefix);
}

bool OmniboxFieldTrial::ShortcutsScoringMaxRelevance(
    AutocompleteInput::PageClassification current_page_classification,
    int* max_relevance) {
  // The value of the rule is a string that encodes an integer containing
  // the max relevance.
  const std::string& max_relevance_str =
      OmniboxFieldTrial::GetValueForRuleInContext(
          kShortcutsScoringMaxRelevanceRule, current_page_classification);
  if (max_relevance_str.empty())
    return false;
  if (!base::StringToInt(max_relevance_str, max_relevance))
    return false;
  return true;
}

bool OmniboxFieldTrial::SearchHistoryPreventInlining(
    AutocompleteInput::PageClassification current_page_classification) {
  return OmniboxFieldTrial::GetValueForRuleInContext(
      kSearchHistoryRule, current_page_classification) == "PreventInlining";
}

bool OmniboxFieldTrial::SearchHistoryDisable(
    AutocompleteInput::PageClassification current_page_classification) {
  return OmniboxFieldTrial::GetValueForRuleInContext(
      kSearchHistoryRule, current_page_classification) == "Disable";
}

void OmniboxFieldTrial::GetDemotionsByType(
    AutocompleteInput::PageClassification current_page_classification,
    DemotionMultipliers* demotions_by_type) {
  demotions_by_type->clear();
  const std::string demotion_rule =
      OmniboxFieldTrial::GetValueForRuleInContext(
          kDemoteByTypeRule,
          current_page_classification);
  // The value of the DemoteByType rule is a comma-separated list of
  // {ResultType + ":" + Number} where ResultType is an AutocompleteMatchType::
  // Type enum represented as an integer and Number is an integer number
  // between 0 and 100 inclusive.   Relevance scores of matches of that result
  // type are multiplied by Number / 100.  100 means no change.
  base::StringPairs kv_pairs;
  if (base::SplitStringIntoKeyValuePairs(demotion_rule, ':', ',', &kv_pairs)) {
    for (base::StringPairs::const_iterator it = kv_pairs.begin();
         it != kv_pairs.end(); ++it) {
      // This is a best-effort conversion; we trust the hand-crafted parameters
      // downloaded from the server to be perfect.  There's no need to handle
      // errors smartly.
      int k, v;
      base::StringToInt(it->first, &k);
      base::StringToInt(it->second, &v);
      (*demotions_by_type)[static_cast<AutocompleteMatchType::Type>(k)] =
          static_cast<float>(v) / 100.0f;
    }
  }
}

OmniboxFieldTrial::UndemotableTopMatchTypes
OmniboxFieldTrial::GetUndemotableTopTypes(
    AutocompleteInput::PageClassification current_page_classification) {
  UndemotableTopMatchTypes undemotable_types;
  const std::string types_rule =
      OmniboxFieldTrial::GetValueForRuleInContext(
          kUndemotableTopTypeRule,
          current_page_classification);
  // The value of the UndemotableTopTypes rule is a comma-separated list of
  // AutocompleteMatchType::Type enums represented as an integer. The
  // DemoteByType rule does not apply to the top match if the type of the top
  // match is in this list.
  std::vector<std::string> types;
  base::SplitString(types_rule, ',', &types);
  for (std::vector<std::string>::const_iterator it = types.begin();
       it != types.end(); ++it) {
    // This is a best-effort conversion; we trust the hand-crafted parameters
    // downloaded from the server to be perfect.  There's no need to handle
    // errors smartly.
    int t;
    base::StringToInt(*it, &t);
    undemotable_types.insert(static_cast<AutocompleteMatchType::Type>(t));
  }
  return undemotable_types;
}

bool OmniboxFieldTrial::ReorderForLegalDefaultMatch(
    AutocompleteInput::PageClassification current_page_classification) {
  return OmniboxFieldTrial::GetValueForRuleInContext(
      kReorderForLegalDefaultMatchRule, current_page_classification) ==
      kReorderForLegalDefaultMatchRuleEnabled;
}

const char OmniboxFieldTrial::kBundledExperimentFieldTrialName[] =
    "OmniboxBundledExperimentV1";
const char OmniboxFieldTrial::kShortcutsScoringMaxRelevanceRule[] =
    "ShortcutsScoringMaxRelevance";
const char OmniboxFieldTrial::kSearchHistoryRule[] = "SearchHistory";
const char OmniboxFieldTrial::kDemoteByTypeRule[] = "DemoteByType";
const char OmniboxFieldTrial::kUndemotableTopTypeRule[] = "UndemotableTopTypes";
const char OmniboxFieldTrial::kReorderForLegalDefaultMatchRule[] =
    "ReorderForLegalDefaultMatch";
const char OmniboxFieldTrial::kReorderForLegalDefaultMatchRuleEnabled[] =
    "ReorderForLegalDefaultMatch";

// Background and implementation details:
//
// Each experiment group in any field trial can come with an optional set of
// parameters (key-value pairs).  In the bundled omnibox experiment
// (kBundledExperimentFieldTrialName), each experiment group comes with a
// list of parameters in the form:
//   key=<Rule>:
//       <AutocompleteInput::PageClassification (as an int)>:
//       <whether Instant Extended is enabled (as a 1 or 0)>
//     (note that there are no linebreaks in keys; this format is for
//      presentation only>
//   value=<arbitrary string>
// Both the AutocompleteInput::PageClassification and the Instant Extended
// entries can be "*", which means this rule applies for all values of the
// matching portion of the context.
// One example parameter is
//   key=SearchHistory:6:1
//   value=PreventInlining
// This means in page classification context 6 (a search result page doing
// search term replacement) with Instant Extended enabled, the SearchHistory
// experiment should PreventInlining.
//
// When an exact match to the rule in the current context is missing, we
// give preference to a wildcard rule that matches the instant extended
// context over a wildcard rule that matches the page classification
// context.  Hopefully, though, users will write their field trial configs
// so as not to rely on this fall back order.
//
// In short, this function tries to find the value associated with key
// |rule|:|page_classification|:|instant_extended|, failing that it looks up
// |rule|:*:|instant_extended|, failing that it looks up
// |rule|:|page_classification|:*, failing that it looks up |rule|:*:*,
// and failing that it returns the empty string.
std::string OmniboxFieldTrial::GetValueForRuleInContext(
    const std::string& rule,
    AutocompleteInput::PageClassification page_classification) {
  std::map<std::string, std::string> params;
  if (!chrome_variations::GetVariationParams(kBundledExperimentFieldTrialName,
                                             &params)) {
    return std::string();
  }
  const std::string page_classification_str =
      base::IntToString(static_cast<int>(page_classification));
  const std::string instant_extended =
      chrome::IsInstantExtendedAPIEnabled() ? "1" : "0";
  // Look up rule in this exact context.
  std::map<std::string, std::string>::iterator it = params.find(
      rule + ":" + page_classification_str + ":" + instant_extended);
  if (it != params.end())
    return it->second;
  // Fall back to the global page classification context.
  it = params.find(rule + ":*:" + instant_extended);
  if (it != params.end())
    return it->second;
  // Fall back to the global instant extended context.
  it = params.find(rule + ":" + page_classification_str + ":*");
  if (it != params.end())
    return it->second;
  // Look up rule in the global context.
  it = params.find(rule + ":*:*");
  return (it != params.end()) ? it->second : std::string();
}
