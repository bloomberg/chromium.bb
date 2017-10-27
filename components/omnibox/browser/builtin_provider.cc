// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/builtin_provider.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/history_provider.h"
#include "components/url_formatter/url_fixer.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"

const int BuiltinProvider::kRelevance = 860;

BuiltinProvider::BuiltinProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_BUILTIN),
      client_(client) {
  builtins_ = client_->GetBuiltinURLs();
}

void BuiltinProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();
  if (input.from_omnibox_focus() ||
      (input.type() == metrics::OmniboxInputType::INVALID) ||
      (input.type() == metrics::OmniboxInputType::QUERY))
    return;

  const size_t kAboutSchemeLength = strlen(url::kAboutScheme);
  const base::string16 kAbout =
      base::ASCIIToUTF16(url::kAboutScheme) +
      base::ASCIIToUTF16(url::kStandardSchemeSeparator);
  const base::string16 embedderAbout =
      base::UTF8ToUTF16(client_->GetEmbedderRepresentationOfAboutScheme()) +
      base::ASCIIToUTF16(url::kStandardSchemeSeparator);

  const int kUrl = ACMatchClassification::URL;
  const int kMatch = kUrl | ACMatchClassification::MATCH;

  const base::string16 text = input.text();
  bool starting_about = base::StartsWith(embedderAbout, text,
                                         base::CompareCase::INSENSITIVE_ASCII);
  if (starting_about ||
      base::StartsWith(kAbout, text, base::CompareCase::INSENSITIVE_ASCII)) {
    ACMatchClassifications styles;
    // Highlight the input portion matching |embedderAbout|; or if the user has
    // input "about:" (with optional slashes), highlight the whole
    // |embedderAbout|.
    bool highlight = starting_about || text.length() > kAboutSchemeLength;
    styles.push_back(ACMatchClassification(0, highlight ? kMatch : kUrl));
    size_t offset = starting_about ? text.length() : embedderAbout.length();
    if (highlight)
      styles.push_back(ACMatchClassification(offset, kUrl));
    // Include some common builtin URLs as the user types the scheme.
    for (base::string16 url : client_->GetBuiltinsToProvideAsUserTypes())
      AddMatch(url, base::string16(), styles);
  } else {
    // Match input about: or |embedderAbout| URL input against builtin URLs.
    GURL url = url_formatter::FixupURL(base::UTF16ToUTF8(text), std::string());
    const bool text_ends_with_slash =
        base::EndsWith(text, base::ASCIIToUTF16("/"),
                       base::CompareCase::SENSITIVE);
    // BuiltinProvider doesn't know how to suggest valid ?query or #fragment
    // extensions to builtin URLs.
    if (url.SchemeIs(client_->GetEmbedderRepresentationOfAboutScheme()) &&
        url.has_host() && !url.has_query() && !url.has_ref()) {
      // Suggest about:blank for substrings, taking URL fixup into account.
      // Chrome does not support trailing slashes or paths for about:blank.
      const base::string16 blank_host = base::ASCIIToUTF16("blank");
      const base::string16 host = base::UTF8ToUTF16(url.host());
      if (base::StartsWith(text, base::ASCIIToUTF16(url::kAboutScheme),
                           base::CompareCase::INSENSITIVE_ASCII) &&
          base::StartsWith(blank_host, host,
                           base::CompareCase::INSENSITIVE_ASCII) &&
          (url.path().length() <= 1) && !text_ends_with_slash) {
        ACMatchClassifications styles;
        styles.push_back(ACMatchClassification(0, kMatch));
        base::string16 match = base::ASCIIToUTF16(url::kAboutBlankURL);
        // Measure the length of the matching host after the "about:" scheme.
        const size_t corrected_length = kAboutSchemeLength + 1 + host.length();
        if (blank_host.length() > host.length())
          styles.push_back(ACMatchClassification(corrected_length, kUrl));
        AddMatch(match, match.substr(corrected_length), styles);
      }

      // Include the path for sub-pages (e.g. "chrome://settings/browser").
      base::string16 host_and_path = base::UTF8ToUTF16(url.host() + url.path());
      base::TrimString(host_and_path, base::ASCIIToUTF16("/"), &host_and_path);
      size_t match_length = embedderAbout.length() + host_and_path.length();
      for (Builtins::const_iterator i(builtins_.begin());
          (i != builtins_.end()) && (matches_.size() < kMaxMatches); ++i) {
        if (base::StartsWith(*i, host_and_path,
                             base::CompareCase::INSENSITIVE_ASCII)) {
          ACMatchClassifications styles;
          // Highlight |embedderAbout|, even for input "about:foo".
          styles.push_back(ACMatchClassification(0, kMatch));
          base::string16 match_string = embedderAbout + *i;
          if (match_string.length() > match_length)
            styles.push_back(ACMatchClassification(match_length, kUrl));
          // FixupURL() may have dropped a trailing slash on the user's input.
          // Ensure that in that case, we don't inline autocomplete unless the
          // autocompletion restores the slash.  This prevents us from e.g.
          // trying to add a 'y' to an input like "chrome://histor/".
          base::string16 inline_autocompletion(
              match_string.substr(match_length));
          if (text_ends_with_slash && !base::StartsWith(
              match_string.substr(match_length), base::ASCIIToUTF16("/"),
              base::CompareCase::INSENSITIVE_ASCII))
            inline_autocompletion = base::string16();
          AddMatch(match_string, inline_autocompletion, styles);
        }
      }
    }
  }

  for (size_t i = 0; i < matches_.size(); ++i)
    matches_[i].relevance = kRelevance + matches_.size() - (i + 1);
  if (!HistoryProvider::PreventInlineAutocomplete(input) &&
      (matches_.size() == 1) && !matches_[0].inline_autocompletion.empty()) {
    // If there's only one possible completion of the user's input and
    // allowing completions is okay, give the match a high enough score to
    // allow it to beat url-what-you-typed and be inlined.
    matches_[0].relevance = 1250;
    matches_[0].allowed_to_be_default_match = true;
  }
}

BuiltinProvider::~BuiltinProvider() {}

void BuiltinProvider::AddMatch(const base::string16& match_string,
                               const base::string16& inline_completion,
                               const ACMatchClassifications& styles) {
  AutocompleteMatch match(this, kRelevance, false,
                          AutocompleteMatchType::NAVSUGGEST);
  match.fill_into_edit = match_string;
  match.inline_autocompletion = inline_completion;
  match.destination_url = GURL(match_string);
  match.contents = match_string;
  match.contents_class = styles;
  matches_.push_back(match);
}
