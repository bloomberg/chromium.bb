// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/builtin_provider.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/history_provider.h"
#include "chrome/common/url_constants.h"
#include "components/autocomplete/autocomplete_input.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/url_fixer/url_fixer.h"

namespace {

#if !defined(OS_ANDROID)
// This list should be kept in sync with chrome/common/url_constants.h.
// Only include useful sub-pages, confirmation alerts are not useful.
const char* const kChromeSettingsSubPages[] = {
  chrome::kAutofillSubPage,
  chrome::kClearBrowserDataSubPage,
  chrome::kContentSettingsSubPage,
  chrome::kContentSettingsExceptionsSubPage,
  chrome::kImportDataSubPage,
  chrome::kLanguageOptionsSubPage,
  chrome::kPasswordManagerSubPage,
  chrome::kResetProfileSettingsSubPage,
  chrome::kSearchEnginesSubPage,
  chrome::kSyncSetupSubPage,
#if defined(OS_CHROMEOS)
  chrome::kInternetOptionsSubPage,
#endif
};
#endif // !defined(OS_ANDROID)

}  // namespace

const int BuiltinProvider::kRelevance = 860;

BuiltinProvider::BuiltinProvider()
    : AutocompleteProvider(AutocompleteProvider::TYPE_BUILTIN) {
  std::vector<std::string> builtins(
      chrome::kChromeHostURLs,
      chrome::kChromeHostURLs + chrome::kNumberOfChromeHostURLs);
  std::sort(builtins.begin(), builtins.end());
  for (std::vector<std::string>::iterator i(builtins.begin());
       i != builtins.end(); ++i)
    builtins_.push_back(base::ASCIIToUTF16(*i));

#if !defined(OS_ANDROID)
  base::string16 settings(base::ASCIIToUTF16(chrome::kChromeUISettingsHost) +
                          base::ASCIIToUTF16("/"));
  for (size_t i = 0; i < arraysize(kChromeSettingsSubPages); i++) {
    builtins_.push_back(
        settings + base::ASCIIToUTF16(kChromeSettingsSubPages[i]));
  }
#endif
}

void BuiltinProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();
  if ((input.type() == metrics::OmniboxInputType::INVALID) ||
      (input.type() == metrics::OmniboxInputType::FORCED_QUERY) ||
      (input.type() == metrics::OmniboxInputType::QUERY))
    return;

  const size_t kAboutSchemeLength = strlen(url::kAboutScheme);
  const base::string16 kAbout =
      base::ASCIIToUTF16(url::kAboutScheme) +
      base::ASCIIToUTF16(url::kStandardSchemeSeparator);
  const base::string16 kChrome = base::ASCIIToUTF16(content::kChromeUIScheme) +
      base::ASCIIToUTF16(url::kStandardSchemeSeparator);

  const int kUrl = ACMatchClassification::URL;
  const int kMatch = kUrl | ACMatchClassification::MATCH;

  base::string16 text = input.text();
  bool starting_chrome = StartsWith(kChrome, text, false);
  if (starting_chrome || StartsWith(kAbout, text, false)) {
    ACMatchClassifications styles;
    // Highlight the input portion matching "chrome://"; or if the user has
    // input "about:" (with optional slashes), highlight the whole "chrome://".
    bool highlight = starting_chrome || text.length() > kAboutSchemeLength;
    styles.push_back(ACMatchClassification(0, highlight ? kMatch : kUrl));
    size_t offset = starting_chrome ? text.length() : kChrome.length();
    if (highlight)
      styles.push_back(ACMatchClassification(offset, kUrl));
    // Include some common builtin chrome URLs as the user types the scheme.
    AddMatch(base::ASCIIToUTF16(chrome::kChromeUIChromeURLsURL),
             base::string16(), styles);
#if !defined(OS_ANDROID)
    AddMatch(base::ASCIIToUTF16(chrome::kChromeUISettingsURL),
             base::string16(), styles);
#endif
    AddMatch(base::ASCIIToUTF16(chrome::kChromeUIVersionURL),
             base::string16(), styles);
  } else {
    // Match input about: or chrome: URL input against builtin chrome URLs.
    GURL url = url_fixer::FixupURL(base::UTF16ToUTF8(text), std::string());
    // BuiltinProvider doesn't know how to suggest valid ?query or #fragment
    // extensions to chrome: URLs.
    if (url.SchemeIs(content::kChromeUIScheme) && url.has_host() &&
        !url.has_query() && !url.has_ref()) {
      // Suggest about:blank for substrings, taking URL fixup into account.
      // Chrome does not support trailing slashes or paths for about:blank.
      const base::string16 blank_host = base::ASCIIToUTF16("blank");
      const base::string16 host = base::UTF8ToUTF16(url.host());
      if (StartsWith(text, base::ASCIIToUTF16(url::kAboutScheme), false) &&
          StartsWith(blank_host, host, false) && (url.path().length() <= 1) &&
          !EndsWith(text, base::ASCIIToUTF16("/"), false)) {
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
      size_t match_length = kChrome.length() + host_and_path.length();
      for (Builtins::const_iterator i(builtins_.begin());
          (i != builtins_.end()) && (matches_.size() < kMaxMatches); ++i) {
        if (StartsWith(*i, host_and_path, false)) {
          ACMatchClassifications styles;
          // Highlight the "chrome://" scheme, even for input "about:foo".
          styles.push_back(ACMatchClassification(0, kMatch));
          base::string16 match_string = kChrome + *i;
          if (match_string.length() > match_length)
            styles.push_back(ACMatchClassification(match_length, kUrl));
          AddMatch(match_string, match_string.substr(match_length), styles);
        }
      }
    }
  }

  for (size_t i = 0; i < matches_.size(); ++i)
    matches_[i].relevance = kRelevance + matches_.size() - (i + 1);
  if (!HistoryProvider::PreventInlineAutocomplete(input) &&
      (matches_.size() == 1)) {
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
