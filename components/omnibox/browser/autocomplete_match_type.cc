// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match_type.h"

#include "base/macros.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
std::string AutocompleteMatchType::ToString(AutocompleteMatchType::Type type) {
  // clang-format off
  const char* strings[] = {
    "url-what-you-typed",
    "history-url",
    "history-title",
    "history-body",
    "history-keyword",
    "navsuggest",
    "search-what-you-typed",
    "search-history",
    "search-suggest",
    "search-suggest-entity",
    "search-suggest-infinite",
    "search-suggest-personalized",
    "search-suggest-profile",
    "search-other-engine",
    "extension-app",
    "contact",
    "bookmark-title",
    "navsuggest-personalized",
    "search-calculator-answer",
    "url-from-clipboard",
    "voice-suggest",
    "physical-web",
    "physical-web-overflow",
    "tab-search",
  };
  // clang-format on
  static_assert(arraysize(strings) == AutocompleteMatchType::NUM_TYPES,
                "strings array must have NUM_TYPES elements");
  return strings[type];
}

base::string16 AutocompleteMatchType::ToAccessibilityLabel(
    AutocompleteMatchType::Type type,
    const base::string16& descriptive_text) {
  // Types with a message ID of zero get |text| returned as-is.
  static constexpr int message_ids[] = {
      0,                             // URL_WHAT_YOU_TYPED
      IDS_ACC_AUTOCOMPLETE_HISTORY,  // HISTORY_URL
      IDS_ACC_AUTOCOMPLETE_HISTORY,  // HISTORY_TITLE
      IDS_ACC_AUTOCOMPLETE_HISTORY,  // HISTORY_BODY

      // HISTORY_KEYWORD is a custom search engine with no %s in its string - so
      // more or less a regular URL.
      0,                                      // HISTORY_KEYWORD
      0,                                      // NAVSUGGEST
      IDS_ACC_AUTOCOMPLETE_SEARCH,            // SEARCH_WHAT_YOU_TYPED
      IDS_ACC_AUTOCOMPLETE_SEARCH_HISTORY,    // SEARCH_HISTORY
      IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH,  // SEARCH_SUGGEST
      IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH,  // SEARCH_SUGGEST_ENTITY
      IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH,  // SEARCH_SUGGEST_TAIL

      // SEARCH_SUGGEST_PERSONALIZED are searches from history elsewhere, maybe
      // on other machines via Sync, or when signed in to Google.
      IDS_ACC_AUTOCOMPLETE_HISTORY,           // SEARCH_SUGGEST_PERSONALIZED
      IDS_ACC_AUTOCOMPLETE_SUGGESTED_SEARCH,  // SEARCH_SUGGEST_PROFILE
      IDS_ACC_AUTOCOMPLETE_SEARCH,            // SEARCH_OTHER_ENGINE
      0,                                      // EXTENSION_APP (deprecated)
      0,                                      // CONTACT_DEPRECATED
      IDS_ACC_AUTOCOMPLETE_BOOKMARK,          // BOOKMARK_TITLE

      // NAVSUGGEST_PERSONALIZED is like SEARCH_SUGGEST_PERSONALIZED, but it's a
      // URL instead of a search query.
      IDS_ACC_AUTOCOMPLETE_HISTORY,    // NAVSUGGEST_PERSONALIZED
      0,                               // CALCULATOR
      IDS_ACC_AUTOCOMPLETE_CLIPBOARD,  // CLIPBOARD
      0,                               // VOICE_SUGGEST
      0,                               // PHYSICAL_WEB
      0,                               // PHYSICAL_WEB_OVERFLOW
      0,                               // TAB_SEARCH
  };
  static_assert(arraysize(message_ids) == AutocompleteMatchType::NUM_TYPES,
                "message_ids must have NUM_TYPES elements");
  if (!message_ids[type])
    return descriptive_text;
  return l10n_util::GetStringFUTF16(message_ids[type], descriptive_text);
}
