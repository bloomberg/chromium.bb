// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/titled_url_match_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/titled_url_node.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/history_provider.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/url_formatter/url_formatter.h"

namespace {

// Converts |positions| into ACMatchClassifications and returns the
// classifications. |text_length| is used to determine the need to add an
// 'unhighlighted' classification span so the tail of the source string
// properly highlighted.
ACMatchClassifications ClassificationsFromMatchPositions(
    const bookmarks::TitledUrlMatch::MatchPositions& positions,
    size_t text_length,
    bool is_url) {
  ACMatchClassification::Style url_style =
      is_url ? ACMatchClassification::URL : ACMatchClassification::NONE;
  ACMatchClassifications classifications;
  if (positions.empty()) {
    if (text_length > 0) {
      classifications.push_back(ACMatchClassification(0, url_style));
    }
    return classifications;
  }

  for (bookmarks::TitledUrlMatch::MatchPositions::const_iterator i =
           positions.begin();
       i != positions.end(); ++i) {
    AutocompleteMatch::ACMatchClassifications new_class;
    AutocompleteMatch::ClassifyLocationInString(
        i->first, i->second - i->first, text_length, url_style, &new_class);
    classifications =
        AutocompleteMatch::MergeClassifications(classifications, new_class);
  }
  return classifications;
}

}  // namespace

namespace bookmarks {

AutocompleteMatch TitledUrlMatchToAutocompleteMatch(
    const TitledUrlMatch& titled_url_match,
    AutocompleteMatchType::Type type,
    int relevance,
    AutocompleteProvider* provider,
    const AutocompleteSchemeClassifier& scheme_classifier,
    const AutocompleteInput& input,
    const base::string16& fixed_up_input_text) {
  const GURL& url = titled_url_match.node->GetTitledUrlNodeUrl();
  base::string16 title = titled_url_match.node->GetTitledUrlNodeTitle();

  // The AutocompleteMatch we construct is non-deletable because the only way to
  // support this would be to delete the underlying object that created the
  // titled_url_match. E.g., for the bookmark provider this would mean deleting
  // the underlying bookmark, which is unlikely to be what the user intends.
  AutocompleteMatch match(provider, relevance, false, type);
  TitledUrlMatch::MatchPositions new_title_match_positions =
      titled_url_match.title_match_positions;
  CorrectTitleAndMatchPositions(&title, &new_title_match_positions);
  const base::string16& url_utf16 = base::UTF8ToUTF16(url.spec());
  size_t inline_autocomplete_offset = URLPrefix::GetInlineAutocompleteOffset(
      input.text(), fixed_up_input_text, false, url_utf16);
  match.destination_url = url;
  const size_t match_start =
      titled_url_match.url_match_positions.empty()
          ? 0
          : titled_url_match.url_match_positions[0].first;
  const bool trim_http =
      !AutocompleteInput::HasHTTPScheme(input.text()) &&
      ((match_start == base::string16::npos) || (match_start != 0));
  std::vector<size_t> offsets = TitledUrlMatch::OffsetsFromMatchPositions(
      titled_url_match.url_match_positions);
  // In addition to knowing how |offsets| is transformed, we need to know how
  // |inline_autocomplete_offset| is transformed.  We add it to the end of
  // |offsets|, compute how everything is transformed, then remove it from the
  // end.
  offsets.push_back(inline_autocomplete_offset);
  match.contents = url_formatter::FormatUrlWithOffsets(
      url, url_formatter::kFormatUrlOmitAll &
               ~(trim_http ? 0 : url_formatter::kFormatUrlOmitHTTP),
      net::UnescapeRule::SPACES, nullptr, nullptr, &offsets);
  inline_autocomplete_offset = offsets.back();
  offsets.pop_back();
  TitledUrlMatch::MatchPositions new_url_match_positions =
      TitledUrlMatch::ReplaceOffsetsInMatchPositions(
          titled_url_match.url_match_positions, offsets);
  match.contents_class = ClassificationsFromMatchPositions(
      new_url_match_positions, match.contents.size(), true);
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url, match.contents, scheme_classifier);
  if (inline_autocomplete_offset != base::string16::npos) {
    // |inline_autocomplete_offset| may be beyond the end of the
    // |fill_into_edit| if the user has typed an URL with a scheme and the
    // last character typed is a slash.  That slash is removed by the
    // FormatURLWithOffsets call above.
    if (inline_autocomplete_offset < match.fill_into_edit.length()) {
      match.inline_autocompletion =
          match.fill_into_edit.substr(inline_autocomplete_offset);
    }
    match.allowed_to_be_default_match =
        match.inline_autocompletion.empty() ||
        !HistoryProvider::PreventInlineAutocomplete(input);
  }
  match.description = title;
  match.description_class = ClassificationsFromMatchPositions(
      titled_url_match.title_match_positions, match.description.size(), false);

  return match;
}

void CorrectTitleAndMatchPositions(
    base::string16* title,
    TitledUrlMatch::MatchPositions* title_match_positions) {
  size_t leading_whitespace_chars = title->length();
  base::TrimWhitespace(*title, base::TRIM_LEADING, title);
  leading_whitespace_chars -= title->length();
  if (leading_whitespace_chars == 0)
    return;
  for (TitledUrlMatch::MatchPositions::iterator it =
           title_match_positions->begin();
       it != title_match_positions->end(); ++it) {
    it->first -= leading_whitespace_chars;
    it->second -= leading_whitespace_chars;
  }
}

}  // namespace bookmarks
