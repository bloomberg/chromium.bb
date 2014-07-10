// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOCOMPLETE_AUTOCOMPLETE_MATCH_TYPE_H_
#define COMPONENTS_AUTOCOMPLETE_AUTOCOMPLETE_MATCH_TYPE_H_

#include <string>

struct AutocompleteMatchType {
  // Type of AutocompleteMatch. Typedef'ed in autocomplete_match.h. Defined here
  // to pass the type details back and forth between the browser and renderer.
  //
  // These values are stored in ShortcutsDatabase and in GetDemotionsByType()
  // and cannot be renumbered.
  enum Type {
    URL_WHAT_YOU_TYPED    = 0,  // The input as a URL.
    HISTORY_URL           = 1,  // A past page whose URL contains the input.
    HISTORY_TITLE         = 2,  // A past page whose title contains the input.
    HISTORY_BODY          = 3,  // A past page whose body contains the input.
    HISTORY_KEYWORD       = 4,  // A past page whose keyword contains the
                                // input.
    NAVSUGGEST            = 5,  // A suggested URL.
    SEARCH_WHAT_YOU_TYPED = 6,  // The input as a search query (with the
                                // default engine).
    SEARCH_HISTORY        = 7,  // A past search (with the default engine)
                                // containing the input.
    SEARCH_SUGGEST        = 8,  // A suggested search (with the default engine)
                                // query that doesn't fall into one of the more
                                // specific suggestion categories below.
    SEARCH_SUGGEST_ENTITY = 9,  // A suggested search for an entity.
    SEARCH_SUGGEST_INFINITE     = 10,  // A suggested search to complete the
                                       // tail of the query.
    SEARCH_SUGGEST_PERSONALIZED = 11,  // A personalized suggested search.
    SEARCH_SUGGEST_PROFILE      = 12,  // A personalized suggested search for a
                                       // Google+ profile.
    SEARCH_OTHER_ENGINE         = 13,  // A search with a non-default engine.
    EXTENSION_APP               = 14,  // An Extension App with a title/url that
                                       // contains the input (deprecated).
    CONTACT_DEPRECATED          = 15,  // One of the user's contacts
                                       // (deprecated).
    BOOKMARK_TITLE              = 16,  // A bookmark whose title contains the
                                       // input.
    NAVSUGGEST_PERSONALIZED     = 17,  // A personalized suggestion URL.
    SEARCH_SUGGEST_ANSWER       = 18,  // A short result for a suggested search.
    NUM_TYPES,
  };

  // Converts |type| to a string representation. Used in logging.
  static std::string ToString(AutocompleteMatchType::Type type);
};

#endif  // COMPONENTS_AUTOCOMPLETE_AUTOCOMPLETE_MATCH_TYPE_H_
