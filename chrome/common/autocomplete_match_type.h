// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_AUTOCOMPLETE_MATCH_TYPE_H_
#define CHROME_COMMON_AUTOCOMPLETE_MATCH_TYPE_H_

#include <string>

struct AutocompleteMatchType {
  // Type of AutocompleteMatch. Typedef'ed in autocomplete_match.h. Defined here
  // to pass the type details back and forth between the browser and renderer.
  enum Type {
    URL_WHAT_YOU_TYPED = 0,  // The input as a URL.
    HISTORY_URL,             // A past page whose URL contains the input.
    HISTORY_TITLE,           // A past page whose title contains the input.
    HISTORY_BODY,            // A past page whose body contains the input.
    HISTORY_KEYWORD,         // A past page whose keyword contains the input.
    NAVSUGGEST,              // A suggested URL.
    SEARCH_WHAT_YOU_TYPED,   // The input as a search query (with the default
                             // engine).
    SEARCH_HISTORY,          // A past search (with the default engine)
                             // containing the input.
    SEARCH_SUGGEST,          // A suggested search (with the default engine).
    SEARCH_OTHER_ENGINE,     // A search with a non-default engine.
    EXTENSION_APP,           // An Extension App with a title/url that contains
                             // the input.
    CONTACT,                 // One of the user's contacts.
    BOOKMARK_TITLE,          // A bookmark whose title contains the input.
    NUM_TYPES,
  };

  // Converts |type| to a string representation. Used in logging.
  static std::string ToString(AutocompleteMatchType::Type type);
};

#endif  // CHROME_COMMON_AUTOCOMPLETE_MATCH_TYPE_H_
