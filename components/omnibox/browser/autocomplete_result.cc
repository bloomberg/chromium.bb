// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_result.h"

#include <algorithm>
#include <iostream>
#include <iterator>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_switches.h"
#include "components/url_formatter/url_fixer.h"

// static
size_t AutocompleteResult::GetMaxMatches() {
  constexpr size_t kDefaultMaxAutocompleteMatches = 6;

  return base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kUIExperimentMaxAutocompleteMatches,
      OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam,
      kDefaultMaxAutocompleteMatches);
}

AutocompleteResult::AutocompleteResult() {
  // It's probably safe to do this in the initializer list, but there's little
  // penalty to doing it here and it ensures our object is fully constructed
  // before calling member functions.
  default_match_ = end();
}

AutocompleteResult::~AutocompleteResult() {}

void AutocompleteResult::CopyOldMatches(
    const AutocompleteInput& input,
    const AutocompleteResult& old_matches,
    TemplateURLService* template_url_service) {
  if (old_matches.empty())
    return;

  if (empty()) {
    // If we've got no matches we can copy everything from the last result.
    CopyFrom(old_matches);
    for (ACMatches::iterator i(begin()); i != end(); ++i)
      i->from_previous = true;
    return;
  }

  // In hopes of providing a stable popup we try to keep the number of matches
  // per provider consistent. Other schemes (such as blindly copying the most
  // relevant matches) typically result in many successive 'What You Typed'
  // results filling all the matches, which looks awful.
  //
  // Instead of starting with the current matches and then adding old matches
  // until we hit our overall limit, we copy enough old matches so that each
  // provider has at least as many as before, and then use SortAndCull() to
  // clamp globally. This way, old high-relevance matches will starve new
  // low-relevance matches, under the assumption that the new matches will
  // ultimately be similar.  If the assumption holds, this prevents seeing the
  // new low-relevance match appear and then quickly get pushed off the bottom;
  // if it doesn't, then once the providers are done and we expire the old
  // matches, the new ones will all become visible, so we won't have lost
  // anything permanently.
  ProviderToMatches matches_per_provider, old_matches_per_provider;
  BuildProviderToMatches(&matches_per_provider);
  old_matches.BuildProviderToMatches(&old_matches_per_provider);
  for (ProviderToMatches::const_iterator i(old_matches_per_provider.begin());
       i != old_matches_per_provider.end(); ++i) {
    MergeMatchesByProvider(input.current_page_classification(),
                           i->second, matches_per_provider[i->first]);
  }

  SortAndCull(input, template_url_service);
}

void AutocompleteResult::AppendMatches(const AutocompleteInput& input,
                                       const ACMatches& matches) {
  for (const auto& i : matches) {
#ifndef NDEBUG
    DCHECK_EQ(AutocompleteMatch::SanitizeString(i.contents), i.contents);
    DCHECK_EQ(AutocompleteMatch::SanitizeString(i.description),
              i.description);
#endif
    matches_.push_back(i);
    if (!AutocompleteMatch::IsSearchType(i.type)) {
      const OmniboxFieldTrial::EmphasizeTitlesCondition condition(
          OmniboxFieldTrial::GetEmphasizeTitlesConditionForInput(input));
      bool emphasize = false;
      switch (condition) {
        case OmniboxFieldTrial::EMPHASIZE_WHEN_NONEMPTY:
          emphasize = !i.description.empty();
          break;
        case OmniboxFieldTrial::EMPHASIZE_WHEN_TITLE_MATCHES:
          emphasize = !i.description.empty() &&
              AutocompleteMatch::HasMatchStyle(i.description_class);
          break;
        case OmniboxFieldTrial::EMPHASIZE_WHEN_ONLY_TITLE_MATCHES:
          emphasize = !i.description.empty() &&
              AutocompleteMatch::HasMatchStyle(i.description_class) &&
              !AutocompleteMatch::HasMatchStyle(i.contents_class);
          break;
        case OmniboxFieldTrial::EMPHASIZE_NEVER:
          break;
        default:
          NOTREACHED();
      }
      matches_.back().swap_contents_and_description = emphasize;
    }
  }
  default_match_ = end();
  alternate_nav_url_ = GURL();
}

void AutocompleteResult::SortAndCull(
    const AutocompleteInput& input,
    TemplateURLService* template_url_service) {
  for (ACMatches::iterator i(matches_.begin()); i != matches_.end(); ++i)
    i->ComputeStrippedDestinationURL(input, template_url_service);

  SortAndDedupMatches(input.current_page_classification(), &matches_);

  // Sort and trim to the most relevant GetMaxMatches() matches.
  size_t max_num_matches = std::min(GetMaxMatches(), matches_.size());
  CompareWithDemoteByType<AutocompleteMatch> comparing_object(
      input.current_page_classification());
  matches_.sort(comparing_object);
  // Top match is not allowed to be the default match.  Find the most
  // relevant legal match and shift it to the front.
  ACMatches::iterator it = FindTopMatch(&matches_);
  if (it != matches_.end())
    matches_.splice(matches_.begin(), matches_, it);
  // In the process of trimming, drop all matches with a demoted relevance
  // score of 0.
  size_t num_matches;
  ACMatches::iterator i = matches_.begin();
  for (num_matches = 0u; (num_matches < max_num_matches) &&
                         (comparing_object.GetDemotedRelevance(*i) > 0);
       ++num_matches) {
    ++i;
  }
  matches_.erase(i, matches_.end());

  default_match_ = matches_.begin();

  if (default_match_ != matches_.end()) {
    const base::string16 debug_info =
        base::ASCIIToUTF16("fill_into_edit=") +
        matches_.front().fill_into_edit + base::ASCIIToUTF16(", provider=") +
        ((matches_.front().provider != NULL)
             ? base::ASCIIToUTF16(matches_.front().provider->GetName())
             : base::string16()) +
        base::ASCIIToUTF16(", input=") + input.text();

    // We should only get here with an empty omnibox for automatic suggestions
    // on focus on the NTP; in these cases hitting enter should do nothing, so
    // there should be no default match.  Otherwise, we're doing automatic
    // suggestions for the currently visible URL (and hitting enter should
    // reload it), or the user is typing; in either of these cases, there should
    // be a default match.
    DCHECK_NE(input.text().empty(), default_match_->allowed_to_be_default_match)
        << debug_info;

    // For navigable default matches, make sure the destination type is what the
    // user would expect given the input.
    if (default_match_->allowed_to_be_default_match &&
        default_match_->destination_url.is_valid()) {
      if (AutocompleteMatch::IsSearchType(default_match_->type)) {
        // We shouldn't get query matches for URL inputs.
        DCHECK_NE(metrics::OmniboxInputType::URL, input.type()) << debug_info;
      } else {
        // If the user explicitly typed a scheme, the default match should
        // have the same scheme.
        if ((input.type() == metrics::OmniboxInputType::URL) &&
            input.parts().scheme.is_nonempty()) {
          const std::string& in_scheme = base::UTF16ToUTF8(input.scheme());
          const std::string& dest_scheme =
              default_match_->destination_url.scheme();
          DCHECK(url_formatter::IsEquivalentScheme(in_scheme, dest_scheme))
              << debug_info;
        }
      }
    }
  }

  // Set the alternate nav URL.
  alternate_nav_url_ = (default_match_ == matches_.end()) ?
      GURL() : ComputeAlternateNavUrl(input, *default_match_);
}

bool AutocompleteResult::HasCopiedMatches() const {
  for (ACMatches::const_iterator i(begin()); i != end(); ++i) {
    if (i->from_previous)
      return true;
  }
  return false;
}

size_t AutocompleteResult::size() const {
  return matches_.size();
}

bool AutocompleteResult::empty() const {
  return matches_.empty();
}

AutocompleteResult::const_iterator AutocompleteResult::begin() const {
  return matches_.begin();
}

AutocompleteResult::iterator AutocompleteResult::begin() {
  return matches_.begin();
}

AutocompleteResult::const_iterator AutocompleteResult::end() const {
  return matches_.end();
}

AutocompleteResult::iterator AutocompleteResult::end() {
  return matches_.end();
}

// Returns the match at the given index.
// NOTE: O(N). Minimize use, if possible.
const AutocompleteMatch& AutocompleteResult::match_at(size_t index) const {
  DCHECK_LT(index, matches_.size());
  return *std::next(matches_.begin(), index);
}

AutocompleteMatch* AutocompleteResult::match_at(size_t index) {
  DCHECK_LT(index, matches_.size());
  return &(*std::next(matches_.begin(), index));
}

bool AutocompleteResult::TopMatchIsStandaloneVerbatimMatch() const {
  if (empty() || !begin()->IsVerbatimType())
    return false;

  // Skip any copied matches, under the assumption that they'll be expired and
  // disappear.  We don't want this disappearance to cause the visibility of the
  // top match to change.
  for (const_iterator i = std::next(begin()); i != end(); ++i) {
    if (!i->from_previous)
      return !i->IsVerbatimType();
  }
  return true;
}

// static
ACMatches::const_iterator AutocompleteResult::FindTopMatch(
    const ACMatches& matches) {
  ACMatches::const_iterator it = matches.begin();
  while ((it != matches.end()) && !it->allowed_to_be_default_match)
    ++it;
  return it;
}

// static
ACMatches::iterator AutocompleteResult::FindTopMatch(ACMatches* matches) {
  ACMatches::iterator it = matches->begin();
  while ((it != matches->end()) && !it->allowed_to_be_default_match)
    ++it;
  return it;
}

void AutocompleteResult::Reset() {
  matches_.clear();
  default_match_ = end();
}

void AutocompleteResult::Swap(AutocompleteResult* other) {
  bool this_was_end = default_match_ == end();
  bool other_was_end = other->default_match_ == other->end();
  matches_.swap(other->matches_);
  if (this_was_end)
    default_match_ = other->end();
  if (other_was_end)
    other->default_match_ = end();
  swap(default_match_, other->default_match_);
  alternate_nav_url_.Swap(&(other->alternate_nav_url_));
}

#ifndef NDEBUG
void AutocompleteResult::Validate() const {
  for (const_iterator i(begin()); i != end(); ++i)
    i->Validate();
}
#endif

// static
GURL AutocompleteResult::ComputeAlternateNavUrl(
    const AutocompleteInput& input,
    const AutocompleteMatch& match) {
  return ((input.type() == metrics::OmniboxInputType::UNKNOWN) &&
          (AutocompleteMatch::IsSearchType(match.type)) &&
          !ui::PageTransitionCoreTypeIs(match.transition,
                                        ui::PAGE_TRANSITION_KEYWORD) &&
          (input.canonicalized_url() != match.destination_url))
             ? input.canonicalized_url()
             : GURL();
}

void AutocompleteResult::SortAndDedupMatches(
    metrics::OmniboxEventProto::PageClassification page_classification,
    ACMatches* matches) {
  // Sort matches such that duplicate matches are consecutive.
  matches->sort(DestinationSort<AutocompleteMatch>(page_classification));

  // Set duplicate_matches for the first match before erasing duplicate
  // matches.
  for (ACMatches::iterator i(matches->begin()); i != matches->end(); ++i) {
    for (auto j = std::next(i);
         j != matches->end() && AutocompleteMatch::DestinationsEqual(*i, *j);
         ++j) {
      AutocompleteMatch& dup_match(*j);
      i->duplicate_matches.insert(i->duplicate_matches.end(),
                                  dup_match.duplicate_matches.begin(),
                                  dup_match.duplicate_matches.end());
      dup_match.duplicate_matches.clear();
      i->duplicate_matches.push_back(dup_match);
    }
  }

  // Erase duplicate matches.
  matches->unique(AutocompleteMatch::DestinationsEqual);
}

void AutocompleteResult::InlineTailPrefixes() {
  base::string16 common_prefix;

  for (const auto& match : matches_) {
    if (match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
      int common_length;
      base::StringToInt(
          match.GetAdditionalInfo(kACMatchPropertyContentsStartIndex),
          &common_length);
      common_prefix = base::UTF8ToUTF16(match.GetAdditionalInfo(
                                            kACMatchPropertySuggestionText))
                          .substr(0, common_length);
      break;
    }
  }
  if (common_prefix.size()) {
    for (auto& match : matches_)
      match.InlineTailPrefix(common_prefix);
  }
}

void AutocompleteResult::CopyFrom(const AutocompleteResult& rhs) {
  if (this == &rhs)
    return;

  matches_ = rhs.matches_;
  // Careful!  You can't just copy iterators from another container, you have to
  // reconstruct them.
  default_match_ =
      (rhs.default_match_ == rhs.end())
          ? end()
          : std::next(begin(), std::distance(rhs.begin(), rhs.default_match_));

  alternate_nav_url_ = rhs.alternate_nav_url_;
}

void AutocompleteResult::BuildProviderToMatches(
    ProviderToMatches* provider_to_matches) const {
  for (ACMatches::const_iterator i(begin()); i != end(); ++i)
    (*provider_to_matches)[i->provider].push_back(*i);
}

// static
bool AutocompleteResult::HasMatchByDestination(const AutocompleteMatch& match,
                                               const ACMatches& matches) {
  for (ACMatches::const_iterator i(matches.begin()); i != matches.end(); ++i) {
    if (i->destination_url == match.destination_url)
      return true;
  }
  return false;
}

void AutocompleteResult::MergeMatchesByProvider(
    metrics::OmniboxEventProto::PageClassification page_classification,
    const ACMatches& old_matches,
    const ACMatches& new_matches) {
  if (new_matches.size() >= old_matches.size())
    return;

  // Prevent old matches from this provider from outranking new ones and
  // becoming the default match by capping old matches' scores to be less than
  // the highest-scoring allowed-to-be-default match from this provider.
  ACMatches::const_iterator i = std::find_if(
      new_matches.begin(), new_matches.end(),
      [] (const AutocompleteMatch& m) {
        return m.allowed_to_be_default_match;
      });

  // If the provider doesn't have any matches that are allowed-to-be-default,
  // cap scores below the global allowed-to-be-default match.
  // AutocompleteResult maintains the invariant that the first item in
  // |matches_| is always such a match.
  if (i == new_matches.end())
    i = matches_.begin();

  DCHECK(i->allowed_to_be_default_match);
  const int max_relevance = i->relevance - 1;

  // Because the goal is a visibly-stable popup, rather than one that preserves
  // the highest-relevance matches, we copy in the lowest-relevance matches
  // first. This means that within each provider's "group" of matches, any
  // synchronous matches (which tend to have the highest scores) will
  // "overwrite" the initial matches from that provider's previous results,
  // minimally disturbing the rest of the matches.
  size_t delta = old_matches.size() - new_matches.size();
  for (ACMatches::const_reverse_iterator i(old_matches.rbegin());
       i != old_matches.rend() && delta > 0; ++i) {
    if (!HasMatchByDestination(*i, new_matches)) {
      AutocompleteMatch match = *i;
      match.relevance = std::min(max_relevance, match.relevance);
      match.from_previous = true;
      matches_.push_back(match);
      delta--;
    }
  }
}
