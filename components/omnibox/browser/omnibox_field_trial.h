// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FIELD_TRIAL_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FIELD_TRIAL_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/autocomplete_match_type.h"

namespace base {
class TimeDelta;
}

// The set of parameters customizing the HUP scoring.
struct HUPScoringParams {
  // A set of parameters describing how to cap a given count score.  First,
  // we apply a half-life based decay of the given count and then find the
  // maximum relevance score based on the decay factor or counts specified
  // in the corresponding bucket list. See comment on |buckets_| for details.
  class ScoreBuckets {
   public:
    // Stores the max relevance at each count/decay factor threshold.
    typedef std::pair<double, int> CountMaxRelevance;

    ScoreBuckets();
    ~ScoreBuckets();

    // Computes a half-life time decay given the |elapsed_time|.
    double HalfLifeTimeDecay(const base::TimeDelta& elapsed_time) const;

    int relevance_cap() const { return relevance_cap_; }
    void set_relevance_cap(int relevance_cap) {
      relevance_cap_ = relevance_cap;
    }

    int half_life_days() const { return half_life_days_; }
    void set_half_life_days(int half_life_days) {
      half_life_days_ = half_life_days;
    }

    bool use_decay_factor() const { return use_decay_factor_; }
    void set_use_decay_factor(bool use_decay_factor) {
      use_decay_factor_ = use_decay_factor;
    }

    std::vector<CountMaxRelevance>& buckets() { return buckets_; }
    const std::vector<CountMaxRelevance>& buckets() const { return buckets_; }

   private:
    // History matches with relevance score greater or equal to |relevance_cap_|
    // are not affected by this experiment.
    // Set to -1, if there is no relevance cap in place and all matches are
    // subject to demotion.
    int relevance_cap_;

    // Half life time for a decayed count as measured since the last visit.
    // Set to -1 if not used.
    int half_life_days_;

    // The relevance score caps at successively decreasing threshold values.
    // The thresholds are either decayed counts or decay factors, depending on
    // the value of |use_decay_factor_|.
    //
    // Consider this example specifying the decayed counts:
    //   [(1, 1000), (0.5, 500), (0, 100)]
    // If decayed count is 2 (which is >= 1), the corresponding match's maximum
    // relevance will be capped at 1000.  In case of 0.5, the score is capped
    // at 500.  Anything below 0.5 is capped at 100.
    //
    // This list is sorted by the pair's first element in descending order.
    std::vector<CountMaxRelevance> buckets_;

    // True when the bucket thresholds are decay factors rather than counts.
    bool use_decay_factor_;
  };

  HUPScoringParams() : experimental_scoring_enabled(false) {}

  bool experimental_scoring_enabled;

  ScoreBuckets typed_count_buckets;

  // Used only when the typed count is 0.
  ScoreBuckets visited_count_buckets;
};

// This class manages the Omnibox field trials.
class OmniboxFieldTrial {
 public:
  // A mapping that contains multipliers indicating that matches of the
  // specified type should have their relevance score multiplied by the
  // given number.  Omitted types are assumed to have multipliers of 1.0.
  typedef std::map<AutocompleteMatchType::Type, float> DemotionMultipliers;

  // Activates all dynamic field trials.  The main difference between
  // the autocomplete dynamic and static field trials is that the former
  // don't require any code changes on the Chrome side as they are controlled
  // on the server side.  Chrome binary simply propagates all necessary
  // information through the X-Client-Data header.
  // This method may be called multiple times.
  static void ActivateDynamicTrials();

  // ---------------------------------------------------------
  // For any experiment that's part of the bundled omnibox field trial.

  // Returns a bitmap containing AutocompleteProvider::Type values
  // that should be disabled in AutocompleteController.
  static int GetDisabledProviderTypes();

  // Returns whether the user is in any dynamic field trial where the
  // group has a the prefix |group_prefix|.
  static bool HasDynamicFieldTrialGroupPrefix(const char *group_prefix);

  // ---------------------------------------------------------
  // For the suggest field trial.

  // Populates |field_trial_hash| with hashes of the active suggest field trial
  // names, if any.
  static void GetActiveSuggestFieldTrialHashes(
      std::vector<uint32>* field_trial_hash);

  // ---------------------------------------------------------
  // For the AutocompleteController "stop timer" field trial.

  // Returns the duration to be used for the AutocompleteController's stop
  // timer.  Returns the default value of 1.5 seconds if the stop timer
  // override experiment isn't active or if parsing the experiment-provided
  // duration fails.
  static base::TimeDelta StopTimerFieldTrialDuration();

  // ---------------------------------------------------------
  // For the ZeroSuggestProvider field trial.

  // Returns whether the user is in any field trial where the
  // ZeroSuggestProvider should be used to get suggestions when the
  // user clicks on the omnibox but has not typed anything yet.
  static bool InZeroSuggestFieldTrial();

  // Returns whether the user is in a ZeroSuggest field trial, which shows
  // most visited URLs. This is true for both "MostVisited" and
  // "MostVisitedWithoutSERP" trials.
  static bool InZeroSuggestMostVisitedFieldTrial();

  // Returns whether the user is in ZeroSuggest field trial showing most
  // visited URLs except it doesn't show suggestions on Google search result
  // pages.
  static bool InZeroSuggestMostVisitedWithoutSerpFieldTrial();

  // Returns whether the user is in a ZeroSuggest field trial and URL-based
  // suggestions can continue to appear after the user has started typing.
  static bool InZeroSuggestAfterTypingFieldTrial();

  // Returns whether the user is in a ZeroSuggest field trial, but should
  // show recently searched-for queries instead.
  static bool InZeroSuggestPersonalizedFieldTrial();

  // ---------------------------------------------------------
  // For the ShortcutsScoringMaxRelevance experiment that's part of the
  // bundled omnibox field trial.

  // If the user is in an experiment group that, given the provided
  // |current_page_classification| context, changes the maximum relevance
  // ShortcutsProvider::CalculateScore() is supposed to assign, extract
  // that maximum relevance score and put in in |max_relevance|.  Returns
  // true on a successful extraction.  CalculateScore()'s return value is
  // a product of this maximum relevance score and some attenuating factors
  // that are all between 0 and 1.  (Note that Shortcuts results may have
  // their scores reduced later if the assigned score is higher than allowed
  // for non-inlineable results.  Shortcuts results are not allowed to be
  // inlined.)
  static bool ShortcutsScoringMaxRelevance(
      metrics::OmniboxEventProto::PageClassification
          current_page_classification,
      int* max_relevance);

  // ---------------------------------------------------------
  // For the SearchHistory experiment that's part of the bundled omnibox
  // field trial.

  // Returns true if the user is in the experiment group that, given the
  // provided |current_page_classification| context, scores search history
  // query suggestions less aggressively so that they don't inline.
  static bool SearchHistoryPreventInlining(
      metrics::OmniboxEventProto::PageClassification
          current_page_classification);

  // Returns true if the user is in the experiment group that, given the
  // provided |current_page_classification| context, disables all query
  // suggestions from search history.
  static bool SearchHistoryDisable(
      metrics::OmniboxEventProto::PageClassification
          current_page_classification);

  // ---------------------------------------------------------
  // For the DemoteByType experiment that's part of the bundled omnibox field
  // trial.

  // If the user is in an experiment group that, in the provided
  // |current_page_classification| context, demotes the relevance scores
  // of certain types of matches, populates the |demotions_by_type| map
  // appropriately.  Otherwise, sets |demotions_by_type| to its default
  // value based on the context.
  static void GetDemotionsByType(
      metrics::OmniboxEventProto::PageClassification
          current_page_classification,
      DemotionMultipliers* demotions_by_type);

  // ---------------------------------------------------------
  // For the HistoryURL provider new scoring experiment that is part of the
  // bundled omnibox field trial.

  // Initializes the HUP |scoring_params| based on the active HUP scoring
  // experiment.  If there is no such experiment, this function simply sets
  // |scoring_params|->experimental_scoring_enabled to false.
  static void GetDefaultHUPScoringParams(HUPScoringParams* scoring_params);
  static void GetExperimentalHUPScoringParams(HUPScoringParams* scoring_params);

  // For the HQPBookmarkValue experiment that's part of the
  // bundled omnibox field trial.

  // Returns the value an untyped visit to a bookmark should receive.
  // Compare this value with the default of 1 for non-bookmarked untyped
  // visits to pages and the default of 20 for typed visits.  Returns
  // 10 if the bookmark value experiment isn't active.
  static int HQPBookmarkValue();

  // ---------------------------------------------------------
  // For the HQPAllowMatchInTLD experiment that's part of the
  // bundled omnibox field trial.

  // Returns true if HQP should allow an input term to match in the
  // top level domain (e.g., .com) of a URL.  Returns false if the
  // allow match in TLD experiment isn't active.
  static bool HQPAllowMatchInTLDValue();

  // ---------------------------------------------------------
  // For the HQPAllowMatchInScheme experiment that's part of the
  // bundled omnibox field trial.

  // Returns true if HQP should allow an input term to match in the
  // scheme (e.g., http://) of a URL.  Returns false if the allow
  // match in scheme experiment isn't active.
  static bool HQPAllowMatchInSchemeValue();

  // ---------------------------------------------------------
  // For SearchProvider related experiments.

  // Returns true if the search provider should not be caching results.
  static bool DisableResultsCaching();

  // Returns how the search provider should poll Suggest. Currently, we support
  // measuring polling delay from the last keystroke or last suggest request.
  static void GetSuggestPollingStrategy(bool* from_last_keystroke,
                                        int* polling_delay_ms);

  // ---------------------------------------------------------
  // For HQP scoring related experiments to control the topicality and scoring
  // ranges of relevancy scores.

  // Returns true if HQP experimental scoring is enabled. Returns false if
  // |kHQPExperimentalScoringEnabledParam| is not specified in the field trial.
  static bool HQPExperimentalScoringEnabled();

  // Returns the scoring buckets for HQP experiments. Returns empty string
  // in case |kHQPExperimentalScoringBucketsParam| or
  // |kHQPExperimentalScoringEnabledParam| is not specified in the
  // field trial. Scoring buckets are stored in string form giving mapping from
  // (topicality_score, frequency_score) to final relevance score.
  // Please see GetRelevancyScore() under
  // chrome/browser/history::ScoredHistoryMatch for details.
  static std::string HQPExperimentalScoringBuckets();

  // Returns the topicality threshold for HQP experiments. Returns -1 if
  // |kHQPExperimentalScoringTopicalityThresholdParam| or
  // |kHQPExperimentalScoringEnabledParam| is not specified in the field trial.
  static float HQPExperimentalTopicalityThreshold();

  // ---------------------------------------------------------
  // For the HQPFixFrequencyScoring experiment that's part of the
  // bundled omnibox field trial.

  // Returns true if HQP should apply the bug fixes to the GetFrequency()
  // function.
  static bool HQPFixFrequencyScoringBugs();

  // ---------------------------------------------------------
  // For the HQPNumTitleWords experiment that's part of the
  // bundled omnibox field trial.

  // Returns the number of title words that are allowed to contribute
  // to the topicality score.  Words later in the title are ignored.
  // Returns 10 as a default if the experiment isn't active.
  static size_t HQPNumTitleWordsToAllow();

  // ---------------------------------------------------------
  // For the HQPAlsoDoHUPLikeScoring experiment that's part of the
  // bundled omnibox field trial.

  // Returns whether HistoryQuick provider (HQP) should attempt to score
  // suggestions also with a HistoryURL-provider-like (HUP-like) mode, and
  // assign suggestions the max of this score and the normal score.
  // Returns false if the experiment isn't active.
  static bool HQPAlsoDoHUPLikeScoring();

  // ---------------------------------------------------------
  // For the PreventUWYTDefaultForNonURLInputs experiment that's part of the
  // bundled omnibox field trial.

  // Returns true if HistoryURL provider should prohibit the URL-what-you-
  // typed match from being the legal default match for non-URL inputs.
  // If this behavior is active, some code in AutocompleteInput::Parse() also
  // gets disabled; this code is unnecessary given the not-allowed-to-be-
  // default constraint.  Returns false if the experiment isn't active.
  static bool PreventUWYTDefaultForNonURLInputs();

  // ---------------------------------------------------------
  // For the aggressive keyword matching experiment that's part of the bundled
  // omnibox field trial.

  // Returns whether KeywordProvider should consider the registry portion
  // (e.g., co.uk) of keywords that look like hostnames as an important part of
  // the keyword name for matching purposes.  Returns true if the experiment
  // isn't active.
  static bool KeywordRequiresRegistry();

  // For keywords that look like hostnames, returns whether KeywordProvider
  // should require users to type a prefix of the hostname to match against
  // them, rather than just the domain name portion.  In other words, returns
  // whether the prefix before the domain name should be considered important
  // for matching purposes.  Returns true if the experiment isn't active.
  static bool KeywordRequiresPrefixMatch();

  // Returns the relevance score that KeywordProvider should assign to keywords
  // with sufficiently-complete matches, i.e., the user has typed all of the
  // important part of the keyword.  Returns -1 if the experiment isn't active.
  static int KeywordScoreForSufficientlyCompleteMatch();

  // ---------------------------------------------------------
  // Exposed publicly for the sake of unittests.
  static const char kBundledExperimentFieldTrialName[];
  // Rule names used by the bundled experiment.
  static const char kDisableProvidersRule[];
  static const char kShortcutsScoringMaxRelevanceRule[];
  static const char kSearchHistoryRule[];
  static const char kDemoteByTypeRule[];
  static const char kHQPBookmarkValueRule[];
  static const char kHQPDiscountFrecencyWhenFewVisitsRule[];
  static const char kHQPAllowMatchInTLDRule[];
  static const char kHQPAllowMatchInSchemeRule[];
  static const char kZeroSuggestRule[];
  static const char kZeroSuggestVariantRule[];
  static const char kSuggestVariantRule[];
  static const char kDisableResultsCachingRule[];
  static const char kMeasureSuggestPollingDelayFromLastKeystrokeRule[];
  static const char kSuggestPollingDelayMsRule[];
  static const char kHQPFixFrequencyScoringBugsRule[];
  static const char kHQPNumTitleWordsRule[];
  static const char kHQPAlsoDoHUPLikeScoringRule[];
  static const char kPreventUWYTDefaultForNonURLInputsRule[];
  static const char kKeywordRequiresRegistryRule[];
  static const char kKeywordRequiresPrefixMatchRule[];
  static const char kKeywordScoreForSufficientlyCompleteMatchRule[];

  // Parameter names used by the HUP new scoring experiments.
  static const char kHUPNewScoringEnabledParam[];
  static const char kHUPNewScoringTypedCountRelevanceCapParam[];
  static const char kHUPNewScoringTypedCountHalfLifeTimeParam[];
  static const char kHUPNewScoringTypedCountScoreBucketsParam[];
  static const char kHUPNewScoringTypedCountUseDecayFactorParam[];
  static const char kHUPNewScoringVisitedCountRelevanceCapParam[];
  static const char kHUPNewScoringVisitedCountHalfLifeTimeParam[];
  static const char kHUPNewScoringVisitedCountScoreBucketsParam[];
  static const char kHUPNewScoringVisitedCountUseDecayFactorParam[];

  // Parameter names used by the HQP experimental scoring experiments.
  static const char kHQPExperimentalScoringEnabledParam[];
  static const char kHQPExperimentalScoringBucketsParam[];
  static const char kHQPExperimentalScoringTopicalityThresholdParam[];

  // The amount of time to wait before sending a new suggest request after the
  // previous one unless overridden by a field trial parameter.
  // Non-const because some unittests modify this value.
  static int kDefaultMinimumTimeBetweenSuggestQueriesMs;

 private:
  friend class OmniboxFieldTrialTest;

  // The bundled omnibox experiment comes with a set of parameters
  // (key-value pairs).  Each key indicates a certain rule that applies in
  // a certain context.  The value indicates what the consequences of
  // applying the rule are.  For example, the value of a SearchHistory rule
  // in the context of a search results page might indicate that we should
  // prevent search history matches from inlining.
  //
  // This function returns the value associated with the |rule| that applies
  // in the current context (which currently consists of |page_classification|
  // and whether Instant Extended is enabled).  If no such rule exists in the
  // current context, fall back to the rule in various wildcard contexts and
  // return its value if found.  If the rule remains unfound in the global
  // context, returns the empty string.  For more details, including how we
  // prioritize different wildcard contexts, see the implementation.  How to
  // interpret the value is left to the caller; this is rule-dependent.
  static std::string GetValueForRuleInContext(
      const std::string& rule,
      metrics::OmniboxEventProto::PageClassification page_classification);

  DISALLOW_IMPLICIT_CONSTRUCTORS(OmniboxFieldTrial);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FIELD_TRIAL_H_
