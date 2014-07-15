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
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/history_provider.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/autocomplete/url_prefix.h"
#include "components/bookmarks/browser/bookmark_match.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "net/base/net_util.h"

using bookmarks::BookmarkMatch;

typedef std::vector<BookmarkMatch> BookmarkMatches;

// BookmarkProvider ------------------------------------------------------------

BookmarkProvider::BookmarkProvider(Profile* profile)
    : AutocompleteProvider(AutocompleteProvider::TYPE_BOOKMARK),
      profile_(profile),
      bookmark_model_(NULL),
      score_using_url_matches_(OmniboxFieldTrial::BookmarksIndexURLsValue()) {
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
      (input.type() == metrics::OmniboxInputType::FORCED_QUERY))
    return;

  DoAutocomplete(input);
}

BookmarkProvider::~BookmarkProvider() {}

void BookmarkProvider::DoAutocomplete(const AutocompleteInput& input) {
  // We may not have a bookmark model for some unit tests.
  if (!bookmark_model_)
    return;

  BookmarkMatches matches;
  // Retrieve enough bookmarks so that we have a reasonable probability of
  // suggesting the one that the user desires.
  const size_t kMaxBookmarkMatches = 50;

  // GetBookmarksMatching returns bookmarks matching the user's
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
  // Please refer to the code for BookmarkIndex::GetBookmarksMatching for
  // complete details of how searches are performed against the user's
  // bookmarks.
  bookmark_model_->GetBookmarksMatching(input.text(),
                                        kMaxBookmarkMatches,
                                        &matches);
  if (matches.empty())
    return;  // There were no matches.
  const base::string16 fixed_up_input(FixupUserInput(input).second);
  for (BookmarkMatches::const_iterator i = matches.begin(); i != matches.end();
       ++i) {
    // Create and score the AutocompleteMatch. If its score is 0 then the
    // match is discarded.
    AutocompleteMatch match(BookmarkMatchToACMatch(input, fixed_up_input, *i));
    if (match.relevance > 0)
      matches_.push_back(match);
  }

  // Sort and clip the resulting matches.
  size_t num_matches =
      std::min(matches_.size(), AutocompleteProvider::kMaxMatches);
  std::partial_sort(matches_.begin(), matches_.begin() + num_matches,
                    matches_.end(), AutocompleteMatch::MoreRelevant);
  matches_.resize(num_matches);
}

namespace {

// for_each helper functor that calculates a match factor for each query term
// when calculating the final score.
//
// Calculate a 'factor' from 0 to the bookmark's title length for a match
// based on 1) how many characters match and 2) where the match is positioned.
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
    scoring_factor_ += term_length *
        (title_length_ - match.first) / title_length_;
  }

  double ScoringFactor() { return scoring_factor_; }

 private:
  double title_length_;
  double scoring_factor_;
};

}  // namespace

AutocompleteMatch BookmarkProvider::BookmarkMatchToACMatch(
    const AutocompleteInput& input,
    const base::string16& fixed_up_input_text,
    const BookmarkMatch& bookmark_match) {
  // The AutocompleteMatch we construct is non-deletable because the only
  // way to support this would be to delete the underlying bookmark, which is
  // unlikely to be what the user intends.
  AutocompleteMatch match(this, 0, false,
                          AutocompleteMatchType::BOOKMARK_TITLE);
  base::string16 title(bookmark_match.node->GetTitle());
  const GURL& url(bookmark_match.node->url());
  const base::string16& url_utf16 = base::UTF8ToUTF16(url.spec());
  size_t inline_autocomplete_offset = URLPrefix::GetInlineAutocompleteOffset(
      input.text(), fixed_up_input_text, false, url_utf16);
  match.destination_url = url;
  const size_t match_start = bookmark_match.url_match_positions.empty() ?
      0 : bookmark_match.url_match_positions[0].first;
  const bool trim_http = !AutocompleteInput::HasHTTPScheme(input.text()) &&
      ((match_start == base::string16::npos) || (match_start != 0));
  std::vector<size_t> offsets = BookmarkMatch::OffsetsFromMatchPositions(
      bookmark_match.url_match_positions);
  // In addition to knowing how |offsets| is transformed, we need to know how
  // |inline_autocomplete_offset| is transformed.  We add it to the end of
  // |offsets|, compute how everything is transformed, then remove it from the
  // end.
  offsets.push_back(inline_autocomplete_offset);
  match.contents = net::FormatUrlWithOffsets(url, languages_,
      net::kFormatUrlOmitAll & ~(trim_http ? 0 : net::kFormatUrlOmitHTTP),
      net::UnescapeRule::SPACES, NULL, NULL, &offsets);
  inline_autocomplete_offset = offsets.back();
  offsets.pop_back();
  BookmarkMatch::MatchPositions new_url_match_positions =
      BookmarkMatch::ReplaceOffsetsInMatchPositions(
          bookmark_match.url_match_positions, offsets);
  match.contents_class =
      ClassificationsFromMatch(new_url_match_positions,
                               match.contents.size(),
                               true);
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url, match.contents, ChromeAutocompleteSchemeClassifier(profile_));
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
      ClassificationsFromMatch(bookmark_match.title_match_positions,
                               match.description.size(),
                               false);
  match.starred = true;

  // Summary on how a relevance score is determined for the match:
  //
  // For each match within the bookmark's title or URL (or both), calculate a
  // 'factor', sum up those factors, then use the sum to figure out a value
  // between the base score and the maximum score.
  //
  // The factor for each match is the product of:
  //
  //  1) how many characters in the bookmark's title/URL are part of this match.
  //     This is capped at the length of the bookmark's title
  //     to prevent terms that match in both the title and the URL from
  //     scoring too strongly.
  //
  //  2) where the match occurs within the bookmark's title or URL,
  //     giving more points for matches that appear earlier in the string:
  //       ((string_length - position of match start) / string_length).
  //
  //  Example: Given a bookmark title of 'abcde fghijklm', with a title length
  //     of 14, and two different search terms, 'abcde' and 'fghij', with
  //     start positions of 0 and 6, respectively, 'abcde' will score higher
  //     (with a a partial factor of (14-0)/14 = 1.000 ) than 'fghij' (with
  //     a partial factor of (14-6)/14 = 0.571 ).  (In this example neither
  //     term matches in the URL.)
  //
  // Once all match factors have been calculated they are summed.  If URL
  // matches are not considered, the resulting sum will never be greater than
  // the length of the bookmark title because of the way the bookmark model
  // matches and removes overlaps.  (In particular, the bookmark model only
  // matches terms to the beginning of words and it removes all overlapping
  // matches, keeping only the longest.  Together these mean that each
  // character is included in at most one match.)  If URL matches are
  // considered, the sum can be greater.
  //
  // This sum is then normalized by the length of the bookmark title (if URL
  // matches are not considered) or by the length of the bookmark title + 10
  // (if URL matches are considered) and capped at 1.0.  (If URL matches
  // are considered, we want to expand the scoring range so fewer bookmarks
  // will hit the 1.0 cap and hence lose all ability to distinguish between
  // these high-quality bookmarks.)
  //
  // The normalized value is multiplied against the scoring range available,
  // which is 299.  The 299 is calculated by subtracting the minimum possible
  // score, 900, from the maximum possible score, 1199.  This product, ranging
  // from 0 to 299, is added to the minimum possible score, 900, giving the
  // preliminary score.
  //
  // If the preliminary score is less than the maximum possible score, 1199,
  // it can be boosted up to that maximum possible score if the URL referenced
  // by the bookmark is also referenced by any of the user's other bookmarks.
  // A count of how many times the bookmark's URL is referenced is determined
  // and, for each additional reference beyond the one for the bookmark being
  // scored up to a maximum of three, the score is boosted by a fixed amount
  // given by |kURLCountBoost|, below.
  //
  if (score_using_url_matches_) {
    // Pretend empty titles are identical to the URL.
    if (title.empty())
      title = base::ASCIIToUTF16(url.spec());
  } else {
    DCHECK(!title.empty());
  }
  ScoringFunctor title_position_functor =
      for_each(bookmark_match.title_match_positions.begin(),
               bookmark_match.title_match_positions.end(),
               ScoringFunctor(title.size()));
  ScoringFunctor url_position_functor =
      for_each(bookmark_match.url_match_positions.begin(),
               bookmark_match.url_match_positions.end(),
               ScoringFunctor(bookmark_match.node->url().spec().length()));
  const double summed_factors = title_position_functor.ScoringFactor() +
      (score_using_url_matches_ ? url_position_functor.ScoringFactor() : 0);
  const double normalized_sum = std::min(
      summed_factors / (title.size() + (score_using_url_matches_ ? 10 : 0)),
      1.0);
  const int kBaseBookmarkScore = 900;
  const int kMaxBookmarkScore = 1199;
  const double kBookmarkScoreRange =
      static_cast<double>(kMaxBookmarkScore - kBaseBookmarkScore);
  match.relevance = static_cast<int>(normalized_sum * kBookmarkScoreRange) +
      kBaseBookmarkScore;
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
    size_t text_length,
    bool is_url) {
  ACMatchClassification::Style url_style =
      is_url ? ACMatchClassification::URL : ACMatchClassification::NONE;
  ACMatchClassifications classifications;
  if (positions.empty()) {
    classifications.push_back(
        ACMatchClassification(0, url_style));
    return classifications;
  }

  for (query_parser::Snippet::MatchPositions::const_iterator i =
           positions.begin();
       i != positions.end();
       ++i) {
    AutocompleteMatch::ACMatchClassifications new_class;
    AutocompleteMatch::ClassifyLocationInString(i->first, i->second - i->first,
        text_length, url_style, &new_class);
    classifications = AutocompleteMatch::MergeClassifications(
        classifications, new_class);
  }
  return classifications;
}
