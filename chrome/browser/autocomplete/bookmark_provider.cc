// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/bookmark_provider.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/autocomplete/history_provider.h"
#include "chrome/browser/autocomplete/url_prefix.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/core/browser/bookmark_title_match.h"
#include "net/base/net_util.h"

typedef std::vector<BookmarkTitleMatch> TitleMatches;

// BookmarkProvider ------------------------------------------------------------

BookmarkProvider::BookmarkProvider(
    AutocompleteProviderListener* listener,
    Profile* profile)
    : AutocompleteProvider(listener, profile,
                           AutocompleteProvider::TYPE_BOOKMARK),
      bookmark_model_(NULL) {
  if (profile) {
    bookmark_model_ = BookmarkModelFactory::GetForProfile(profile);
    languages_ = profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  }
}

void BookmarkProvider::Start(const AutocompleteInput& input,
                             bool minimal_changes) {
  if (minimal_changes)
    return;
  matches_.clear();

  if (input.text().empty() ||
      ((input.type() != AutocompleteInput::UNKNOWN) &&
       (input.type() != AutocompleteInput::QUERY)))
    return;

  DoAutocomplete(input,
                 input.matches_requested() == AutocompleteInput::BEST_MATCH);
}

BookmarkProvider::~BookmarkProvider() {}

void BookmarkProvider::DoAutocomplete(const AutocompleteInput& input,
                                      bool best_match) {
  // We may not have a bookmark model for some unit tests.
  if (!bookmark_model_)
    return;

  TitleMatches matches;
  // Retrieve enough bookmarks so that we have a reasonable probability of
  // suggesting the one that the user desires.
  const size_t kMaxBookmarkMatches = 50;

  // GetBookmarksWithTitlesMatching returns bookmarks matching the user's
  // search terms using the following rules:
  //  - The search text is broken up into search terms. Each term is searched
  //    for separately.
  //  - Term matches are always performed against the start of a word. 'def'
  //    will match against 'define' but not against 'indefinite'.
  //  - Terms must be at least three characters in length in order to perform
  //    partial word matches. Any term of lesser length will only be used as an
  //    exact match. 'def' will match against 'define' but 'de' will not match.
  //  - A search containing multiple terms will return results with those words
  //    occuring in any order.
  //  - Terms enclosed in quotes comprises a phrase that must match exactly.
  //  - Multiple terms enclosed in quotes will require those exact words in that
  //    exact order to match.
  //
  // Note: GetBookmarksWithTitlesMatching() will never return a match span
  // greater than the length of the title against which it is being matched,
  // nor can those spans ever overlap because the match spans are coalesced
  // for all matched terms.
  //
  // Please refer to the code for BookmarkIndex::GetBookmarksWithTitlesMatching
  // for complete details of how title searches are performed against the user's
  // bookmarks.
  bookmark_model_->GetBookmarksWithTitlesMatching(input.text(),
                                                  kMaxBookmarkMatches,
                                                  &matches);
  if (matches.empty())
    return;  // There were no matches.
  AutocompleteInput fixed_up_input(input);
  FixupUserInput(&fixed_up_input);
  for (TitleMatches::const_iterator i = matches.begin(); i != matches.end();
       ++i) {
    // Create and score the AutocompleteMatch. If its score is 0 then the
    // match is discarded.
    AutocompleteMatch match(TitleMatchToACMatch(input, fixed_up_input, *i));
    if (match.relevance > 0)
      matches_.push_back(match);
  }

  // Sort and clip the resulting matches.
  size_t max_matches = best_match ? 1 : AutocompleteProvider::kMaxMatches;
  if (matches_.size() > max_matches) {
    std::partial_sort(matches_.begin(),
                      matches_.begin() + max_matches,
                      matches_.end(),
                      AutocompleteMatch::MoreRelevant);
    matches_.resize(max_matches);
  } else {
    std::sort(matches_.begin(), matches_.end(),
              AutocompleteMatch::MoreRelevant);
  }
}

namespace {

// for_each helper functor that calculates a match factor for each query term
// when calculating the final score.
//
// Calculate a 'factor' from 0.0 to 1.0 based on 1) how much of the bookmark's
// title the term matches, and 2) where the match is positioned within the
// bookmark's title. A full length match earns a 1.0. A half-length match earns
// at most a 0.5 and at least a 0.25. A single character match against a title
// that is 100 characters long where the match is at the first character will
// earn a 0.01 and at the last character will earn a 0.0001.
class ScoringFunctor {
 public:
  // |title_length| is the length of the bookmark title against which this
  // match will be scored.
  explicit ScoringFunctor(size_t title_length)
      : title_length_(static_cast<double>(title_length)),
        scoring_factor_(0.0) {
  }

  void operator()(const query_parser::Snippet::MatchPosition& match) {
    double term_length = static_cast<double>(match.second - match.first);
    scoring_factor_ += term_length / title_length_ *
        (title_length_ - match.first) / title_length_;
  }

  double ScoringFactor() { return scoring_factor_; }

 private:
  double title_length_;
  double scoring_factor_;
};

}  // namespace

AutocompleteMatch BookmarkProvider::TitleMatchToACMatch(
    const AutocompleteInput& input,
    const AutocompleteInput& fixed_up_input,
    const BookmarkTitleMatch& title_match) {
  // The AutocompleteMatch we construct is non-deletable because the only
  // way to support this would be to delete the underlying bookmark, which is
  // unlikely to be what the user intends.
  AutocompleteMatch match(this, 0, false,
                          AutocompleteMatchType::BOOKMARK_TITLE);
  const base::string16& title(title_match.node->GetTitle());
  DCHECK(!title.empty());

  const GURL& url(title_match.node->url());
  const base::string16& url_utf16 = base::UTF8ToUTF16(url.spec());
  size_t match_start, inline_autocomplete_offset;
  URLPrefix::ComputeMatchStartAndInlineAutocompleteOffset(
      input, fixed_up_input, false, url_utf16, &match_start,
      &inline_autocomplete_offset);
  match.destination_url = url;
  const bool trim_http = !AutocompleteInput::HasHTTPScheme(input.text()) &&
      ((match_start == base::string16::npos) || (match_start != 0));
  match.contents = net::FormatUrl(url, languages_,
      net::kFormatUrlOmitAll & ~(trim_http ? 0 : net::kFormatUrlOmitHTTP),
      net::UnescapeRule::SPACES, NULL, NULL, &inline_autocomplete_offset);
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::URL));
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(url,
                                                              match.contents);
  if (inline_autocomplete_offset != base::string16::npos) {
    // |inline_autocomplete_offset| may be beyond the end of the
    // |fill_into_edit| if the user has typed an URL with a scheme and the
    // last character typed is a slash.  That slash is removed by the
    // FormatURLWithOffsets call above.
    if (inline_autocomplete_offset < match.fill_into_edit.length()) {
      match.inline_autocompletion =
          match.fill_into_edit.substr(inline_autocomplete_offset);
    }
    match.allowed_to_be_default_match = match.inline_autocompletion.empty() ||
        !HistoryProvider::PreventInlineAutocomplete(input);
  }
  match.description = title;
  match.description_class =
      ClassificationsFromMatch(title_match.match_positions,
                               match.description.size());
  match.starred = true;

  // Summary on how a relevance score is determined for the match:
  //
  // For each term matching within the bookmark's title (as given by the set of
  // Snippet::MatchPositions) calculate a 'factor', sum up those factors, then
  // use the sum to figure out a value between the base score and the maximum
  // score.
  //
  // The factor for each term is the product of:
  //
  //  1) how much of the bookmark's title has been matched by the term:
  //       (term length / title length).
  //
  //  Example: Given a bookmark title 'abcde fghijklm', with a title length
  //     of 14, and two different search terms, 'abcde' and 'fghijklm', with
  //     term lengths of 5 and 8, respectively, 'fghijklm' will score higher
  //     (with a partial factor of 8/14 = 0.571) than 'abcde' (5/14 = 0.357).
  //
  //  2) where the term match occurs within the bookmark's title, giving more
  //     points for matches that appear earlier in the title:
  //       ((title length - position of match start) / title_length).
  //
  //  Example: Given a bookmark title of 'abcde fghijklm', with a title length
  //     of 14, and two different search terms, 'abcde' and 'fghij', with
  //     start positions of 0 and 6, respectively, 'abcde' will score higher
  //     (with a a partial factor of (14-0)/14 = 1.000 ) than 'fghij' (with
  //     a partial factor of (14-6)/14 = 0.571 ).
  //
  // Once all term factors have been calculated they are summed. The resulting
  // sum will never be greater than 1.0 because of the way the bookmark model
  // matches and removes overlaps. (In particular, the bookmark model only
  // matches terms to the beginning of words and it removes all overlapping
  // matches, keeping only the longest. Together these mean that each
  // character is included in at most one match. This property ensures the
  // sum of factors is at most 1.) This sum is then multiplied against the
  // scoring range available, which is 299. The 299 is calculated by
  // subtracting the minimum possible score, 900, from the maximum possible
  // score, 1199. This product, ranging from 0 to 299, is added to the minimum
  // possible score, 900, giving the preliminary score.
  //
  // If the preliminary score is less than the maximum possible score, 1199,
  // it can be boosted up to that maximum possible score if the URL referenced
  // by the bookmark is also referenced by any of the user's other bookmarks.
  // A count of how many times the bookmark's URL is referenced is determined
  // and, for each additional reference beyond the one for the bookmark being
  // scored up to a maximum of three, the score is boosted by a fixed amount
  // given by |kURLCountBoost|, below.
  //
  ScoringFunctor position_functor =
      for_each(title_match.match_positions.begin(),
               title_match.match_positions.end(), ScoringFunctor(title.size()));
  const int kBaseBookmarkScore = 900;
  const int kMaxBookmarkScore = AutocompleteResult::kLowestDefaultScore - 1;
  const double kBookmarkScoreRange =
      static_cast<double>(kMaxBookmarkScore - kBaseBookmarkScore);
  // It's not likely that GetBookmarksWithTitlesMatching will return overlapping
  // matches but let's play it safe.
  match.relevance = std::min(kMaxBookmarkScore,
      static_cast<int>(position_functor.ScoringFactor() * kBookmarkScoreRange) +
      kBaseBookmarkScore);
  // Don't waste any time searching for additional referenced URLs if we
  // already have a perfect title match.
  if (match.relevance >= kMaxBookmarkScore)
    return match;
  // Boost the score if the bookmark's URL is referenced by other bookmarks.
  const int kURLCountBoost[4] = { 0, 75, 125, 150 };
  std::vector<const BookmarkNode*> nodes;
  bookmark_model_->GetNodesByURL(url, &nodes);
  DCHECK_GE(std::min(arraysize(kURLCountBoost), nodes.size()), 1U);
  match.relevance +=
      kURLCountBoost[std::min(arraysize(kURLCountBoost), nodes.size()) - 1];
  match.relevance = std::min(kMaxBookmarkScore, match.relevance);
  return match;
}

// static
ACMatchClassifications BookmarkProvider::ClassificationsFromMatch(
    const query_parser::Snippet::MatchPositions& positions,
    size_t text_length) {
  ACMatchClassifications classifications;
  if (positions.empty()) {
    classifications.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    return classifications;
  }

  for (query_parser::Snippet::MatchPositions::const_iterator i =
           positions.begin();
       i != positions.end();
       ++i) {
    AutocompleteMatch::ACMatchClassifications new_class;
    AutocompleteMatch::ClassifyLocationInString(i->first, i->second - i->first,
        text_length, 0, &new_class);
    classifications = AutocompleteMatch::MergeClassifications(
        classifications, new_class);
  }
  return classifications;
}
