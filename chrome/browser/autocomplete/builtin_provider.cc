// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/builtin_provider.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/common/net/url_fixer_upper.h"
#include "chrome/common/url_constants.h"

namespace {

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
  chrome::kSearchEnginesSubPage,
  chrome::kSyncSetupSubPage,
#if defined(OS_CHROMEOS)
  chrome::kInternetOptionsSubPage,
#endif
};

}  // namespace

const int BuiltinProvider::kRelevance = 860;

BuiltinProvider::BuiltinProvider(AutocompleteProviderListener* listener,
                                 Profile* profile)
    : AutocompleteProvider(listener, profile,
          AutocompleteProvider::TYPE_BUILTIN) {
  std::vector<std::string> builtins(
      chrome::kChromeHostURLs,
      chrome::kChromeHostURLs + chrome::kNumberOfChromeHostURLs);
  std::sort(builtins.begin(), builtins.end());
  for (std::vector<std::string>::iterator i(builtins.begin());
       i != builtins.end(); ++i)
    builtins_.push_back(ASCIIToUTF16(*i));
  string16 settings(ASCIIToUTF16(chrome::kChromeUISettingsHost) +
                    ASCIIToUTF16("/"));
  for (size_t i = 0; i < arraysize(kChromeSettingsSubPages); i++)
    builtins_.push_back(settings + ASCIIToUTF16(kChromeSettingsSubPages[i]));
}

void BuiltinProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();
  if ((input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY) ||
      (input.type() == AutocompleteInput::QUERY) ||
      (input.matches_requested() == AutocompleteInput::BEST_MATCH))
    return;

  const string16 kAbout = ASCIIToUTF16(chrome::kAboutScheme) +
      ASCIIToUTF16(content::kStandardSchemeSeparator);
  const string16 kChrome = ASCIIToUTF16(chrome::kChromeUIScheme) +
      ASCIIToUTF16(content::kStandardSchemeSeparator);

  const int kUrl = ACMatchClassification::URL;
  const int kMatch = kUrl | ACMatchClassification::MATCH;

  string16 text = input.text();
  bool starting_chrome = StartsWith(kChrome, text, false);
  if (starting_chrome || StartsWith(kAbout, text, false)) {
    ACMatchClassifications styles;
    // Highlight the input portion matching "chrome://"; or if the user has
    // input "about:" (with optional slashes), highlight the whole "chrome://".
    const size_t kAboutSchemeLength = strlen(chrome::kAboutScheme);
    bool highlight = starting_chrome || text.length() > kAboutSchemeLength;
    styles.push_back(ACMatchClassification(0, highlight ? kMatch : kUrl));
    size_t offset = starting_chrome ? text.length() : kChrome.length();
    if (highlight)
      styles.push_back(ACMatchClassification(offset, kUrl));
    // Include some common builtin chrome URLs as the user types the scheme.
    AddMatch(ASCIIToUTF16(chrome::kChromeUIChromeURLsURL), string16(), styles);
    AddMatch(ASCIIToUTF16(chrome::kChromeUISettingsURL), string16(), styles);
    AddMatch(ASCIIToUTF16(chrome::kChromeUIVersionURL), string16(), styles);
  } else {
    // Match input about: or chrome: URL input against builtin chrome URLs.
    GURL url = URLFixerUpper::FixupURL(UTF16ToUTF8(text), std::string());
    if (url.SchemeIs(chrome::kChromeUIScheme) && url.has_host()) {
      // Include the path for sub-pages (e.g. "chrome://settings/browser").
      string16 host_and_path = UTF8ToUTF16(url.host() + url.path());
      TrimString(host_and_path, ASCIIToUTF16("/").c_str(), &host_and_path);
      size_t match_length = kChrome.length() + host_and_path.length();
      for (Builtins::const_iterator i(builtins_.begin());
          (i != builtins_.end()) && (matches_.size() < kMaxMatches); ++i) {
        if (StartsWith(*i, host_and_path, false)) {
          ACMatchClassifications styles;
          // Highlight the "chrome://" scheme, even for input "about:foo".
          styles.push_back(ACMatchClassification(0, kMatch));
          string16 match_string = kChrome + *i;
          if (match_string.length() > match_length)
            styles.push_back(ACMatchClassification(match_length, kUrl));
          AddMatch(match_string, match_string.substr(match_length), styles);
        }
      }
    }
  }

  for (size_t i = 0; i < matches_.size(); ++i)
    matches_[i].relevance = kRelevance + matches_.size() - (i + 1);
  if (!input.prevent_inline_autocomplete() && (matches_.size() == 1)) {
    // If there's only one possible completion of the user's input and
    // allowing completions is okay, give the match a high enough score to
    // allow it to beat url-what-you-typed and be inlined.
    matches_[0].relevance = 1250;
    matches_[0].allowed_to_be_default_match = true;
  }
}

BuiltinProvider::~BuiltinProvider() {}

void BuiltinProvider::AddMatch(const string16& match_string,
                               const string16& inline_completion,
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
