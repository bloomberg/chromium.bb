// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/autocomplete_match_type.h"

#include "base/basictypes.h"

// static
std::string AutocompleteMatchType::ToString(AutocompleteMatchType::Type type) {
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
    "search-suggest-answer",
  };
  COMPILE_ASSERT(arraysize(strings) == AutocompleteMatchType::NUM_TYPES,
                 strings_array_must_match_type_enum);
  return strings[type];
}
