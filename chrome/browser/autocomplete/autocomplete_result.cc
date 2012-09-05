// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_result.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"

// static
const size_t AutocompleteResult::kMaxMatches = 6;
const int AutocompleteResult::kLowestDefaultScore = 1200;

void AutocompleteResult::Selection::Clear() {
  destination_url = GURL();
  provider_affinity = NULL;
  is_history_what_you_typed_match = false;
}

AutocompleteResult::AutocompleteResult() {
  // Reserve space for the max number of matches we'll show.
  matches_.reserve(kMaxMatches);

  // It's probably safe to do this in the initializer list, but there's little
  // penalty to doing it here and it ensures our object is fully constructed
  // before calling member functions.
  default_match_ = end();
}

AutocompleteResult::~AutocompleteResult() {}

void AutocompleteResult::CopyFrom(const AutocompleteResult& rhs) {
  if (this == &rhs)
    return;

  matches_ = rhs.matches_;
  // Careful!  You can't just copy iterators from another container, you have to
  // reconstruct them.
  default_match_ = (rhs.default_match_ == rhs.end()) ?
      end() : (begin() + (rhs.default_match_ - rhs.begin()));

  alternate_nav_url_ = rhs.alternate_nav_url_;
}

void AutocompleteResult::CopyOldMatches(const AutocompleteInput& input,
                                        const AutocompleteResult& old_matches) {
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
    MergeMatchesByProvider(i->second, matches_per_provider[i->first]);
  }

  SortAndCull(input);
}

void AutocompleteResult::AppendMatches(const ACMatches& matches) {
#ifndef NDEBUG
  for (ACMatches::const_iterator i(matches.begin()); i != matches.end(); ++i) {
    DCHECK_EQ(AutocompleteMatch::SanitizeString(i->contents), i->contents);
    DCHECK_EQ(AutocompleteMatch::SanitizeString(i->description),
              i->description);
  }
#endif
  std::copy(matches.begin(), matches.end(), std::back_inserter(matches_));
  default_match_ = end();
  alternate_nav_url_ = GURL();
}

void AutocompleteResult::AddMatch(const AutocompleteMatch& match) {
  DCHECK(default_match_ != end());
  DCHECK_EQ(AutocompleteMatch::SanitizeString(match.contents), match.contents);
  DCHECK_EQ(AutocompleteMatch::SanitizeString(match.description),
            match.description);
  ACMatches::iterator insertion_point =
      std::upper_bound(begin(), end(), match, &AutocompleteMatch::MoreRelevant);
  matches_difference_type default_offset = default_match_ - begin();
  if ((insertion_point - begin()) <= default_offset)
    ++default_offset;
  matches_.insert(insertion_point, match);
  default_match_ = begin() + default_offset;
}

void AutocompleteResult::SortAndCull(const AutocompleteInput& input) {
  for (ACMatches::iterator i(matches_.begin()); i != matches_.end(); ++i)
    i->ComputeStrippedDestinationURL();

  // Remove duplicates.
  std::sort(matches_.begin(), matches_.end(),
            &AutocompleteMatch::DestinationSortFunc);
  matches_.erase(std::unique(matches_.begin(), matches_.end(),
                             &AutocompleteMatch::DestinationsEqual),
                 matches_.end());

  // Sort and trim to the most relevant kMaxMatches matches.
  const size_t num_matches = std::min(kMaxMatches, matches_.size());
  std::partial_sort(matches_.begin(), matches_.begin() + num_matches,
                    matches_.end(), &AutocompleteMatch::MoreRelevant);
  matches_.resize(num_matches);

  default_match_ = begin();

  // Set the alternate nav URL.
  alternate_nav_url_ = GURL();
  if (((input.type() == AutocompleteInput::UNKNOWN) ||
       (input.type() == AutocompleteInput::REQUESTED_URL)) &&
      (default_match_ != end()) &&
      AutocompleteMatch::IsSearchType(default_match_->type) &&
      (default_match_->transition != content::PAGE_TRANSITION_KEYWORD) &&
      (input.canonicalized_url() != default_match_->destination_url))
    alternate_nav_url_ = input.canonicalized_url();
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
const AutocompleteMatch& AutocompleteResult::match_at(size_t index) const {
  DCHECK_LT(index, matches_.size());
  return matches_[index];
}

AutocompleteMatch* AutocompleteResult::match_at(size_t index) {
  DCHECK_LT(index, matches_.size());
  return &matches_[index];
}

void AutocompleteResult::Reset() {
  matches_.clear();
  default_match_ = end();
}

void AutocompleteResult::Swap(AutocompleteResult* other) {
  const size_t default_match_offset = default_match_ - begin();
  const size_t other_default_match_offset =
      other->default_match_ - other->begin();
  matches_.swap(other->matches_);
  default_match_ = begin() + other_default_match_offset;
  other->default_match_ = other->begin() + default_match_offset;
  alternate_nav_url_.Swap(&(other->alternate_nav_url_));
}

#ifndef NDEBUG
void AutocompleteResult::Validate() const {
  for (const_iterator i(begin()); i != end(); ++i)
    i->Validate();
}
#endif

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

void AutocompleteResult::MergeMatchesByProvider(const ACMatches& old_matches,
                                                const ACMatches& new_matches) {
  if (new_matches.size() >= old_matches.size())
    return;

  size_t delta = old_matches.size() - new_matches.size();
  const int max_relevance = (new_matches.empty() ?
      matches_.front().relevance : new_matches[0].relevance) - 1;
  // Because the goal is a visibly-stable popup, rather than one that preserves
  // the highest-relevance matches, we copy in the lowest-relevance matches
  // first. This means that within each provider's "group" of matches, any
  // synchronous matches (which tend to have the highest scores) will
  // "overwrite" the initial matches from that provider's previous results,
  // minimally disturbing the rest of the matches.
  for (ACMatches::const_reverse_iterator i(old_matches.rbegin());
       i != old_matches.rend() && delta > 0; ++i) {
    if (!HasMatchByDestination(*i, new_matches)) {
      AutocompleteMatch match = *i;
      match.relevance = std::min(max_relevance, match.relevance);
      match.from_previous = true;
      AddMatch(match);
      delta--;
    }
  }
}
