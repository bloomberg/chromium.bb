// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_MATCH_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_MATCH_H_
#pragma once

#include <vector>
#include <string>

#include "chrome/common/page_transition_types.h"
#include "googleurl/src/gurl.h"

class AutocompleteProvider;
class PageTransition;
class TemplateURL;

// AutocompleteMatch ----------------------------------------------------------

// A single result line with classified spans.  The autocomplete popup displays
// the 'contents' and the 'description' (the description is optional) in the
// autocomplete dropdown, and fills in 'fill_into_edit' into the textbox when
// that line is selected.  fill_into_edit may be the same as 'description' for
// things like URLs, but may be different for searches or other providers.  For
// example, a search result may say "Search for asdf" as the description, but
// "asdf" should appear in the box.
struct AutocompleteMatch {
  // Autocomplete matches contain strings that are classified according to a
  // separate vector of styles.  This vector associates flags with particular
  // string segments, and must be in sorted order.  All text must be associated
  // with some kind of classification.  Even if a match has no distinct
  // segments, its vector should contain an entry at offset 0 with no flags.
  //
  // Example: The user typed "goog"
  //   http://www.google.com/        Google
  //   ^          ^   ^              ^   ^
  //   0,         |   15,            |   4,
  //              11,match           0,match
  //
  // This structure holds the classification information for each span.
  struct ACMatchClassification {
    // The values in here are not mutually exclusive -- use them like a
    // bitfield.  This also means we use "int" instead of this enum type when
    // passing the values around, so the compiler doesn't complain.
    enum Style {
      NONE  = 0,
      URL   = 1 << 0,  // A URL
      MATCH = 1 << 1,  // A match for the user's search term
      DIM   = 1 << 2,  // "Helper text"
    };

    ACMatchClassification(size_t offset, int style)
        : offset(offset),
          style(style) {
    }

    // Offset within the string that this classification starts
    size_t offset;

    int style;
  };

  typedef std::vector<ACMatchClassification> ACMatchClassifications;

  // The type of this match.
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
    NUM_TYPES,
  };

  AutocompleteMatch();
  AutocompleteMatch(AutocompleteProvider* provider,
                    int relevance,
                    bool deletable,
                    Type type);
  ~AutocompleteMatch();

  // Converts |type| to a string representation.  Used in logging.
  static std::string TypeToString(Type type);

  // Converts |type| to a resource identifier for the appropriate icon for this
  // type.
  static int TypeToIcon(Type type);

  // Comparison function for determining when one match is better than another.
  static bool MoreRelevant(const AutocompleteMatch& elem1,
                           const AutocompleteMatch& elem2);

  // Comparison functions for removing matches with duplicate destinations.
  static bool DestinationSortFunc(const AutocompleteMatch& elem1,
                                  const AutocompleteMatch& elem2);
  static bool DestinationsEqual(const AutocompleteMatch& elem1,
                                const AutocompleteMatch& elem2);

  // Helper functions for classes creating matches:
  // Fills in the classifications for |text|, using |style| as the base style
  // and marking the first instance of |find_text| as a match.  (This match
  // will also not be dimmed, if |style| has DIM set.)
  static void ClassifyMatchInString(const string16& find_text,
                                    const string16& text,
                                    int style,
                                    ACMatchClassifications* classifications);

  // Similar to ClassifyMatchInString(), but for cases where the range to mark
  // as matching is already known (avoids calling find()).  This can be helpful
  // when find() would be misleading (e.g. you want to mark the second match in
  // a string instead of the first).
  static void ClassifyLocationInString(size_t match_location,
                                       size_t match_length,
                                       size_t overall_length,
                                       int style,
                                       ACMatchClassifications* classifications);

  // The provider of this match, used to remember which provider the user had
  // selected when the input changes. This may be NULL, in which case there is
  // no provider (or memory of the user's selection).
  AutocompleteProvider* provider;

  // The relevance of this match. See table above for scores returned by
  // various providers. This is used to rank matches among all responding
  // providers, so different providers must be carefully tuned to supply
  // matches with appropriate relevance.
  //
  // TODO(pkasting): http://b/1111299 This should be calculated algorithmically,
  // rather than being a fairly fixed value defined by the table above.
  int relevance;

  // True if the user should be able to delete this match.
  bool deletable;

  // This string is loaded into the location bar when the item is selected
  // by pressing the arrow keys. This may be different than a URL, for example,
  // for search suggestions, this would just be the search terms.
  string16 fill_into_edit;

  // The position within fill_into_edit from which we'll display the inline
  // autocomplete string.  This will be string16::npos if this match should
  // not be inline autocompleted.
  size_t inline_autocomplete_offset;

  // The URL to actually load when the autocomplete item is selected. This URL
  // should be canonical so we can compare URLs with strcmp to avoid dupes.
  // It may be empty if there is no possible navigation.
  GURL destination_url;

  // The main text displayed in the address bar dropdown.
  string16 contents;
  ACMatchClassifications contents_class;

  // Additional helper text for each entry, such as a title or description.
  string16 description;
  ACMatchClassifications description_class;

  // The transition type to use when the user opens this match.  By default
  // this is TYPED.  Providers whose matches do not look like URLs should set
  // it to GENERATED.
  PageTransition::Type transition;

  // True when this match is the "what you typed" match from the history
  // system.
  bool is_history_what_you_typed_match;

  // Type of this match.
  Type type;

  // If this match corresponds to a keyword, this is the TemplateURL the
  // keyword was obtained from.
  const TemplateURL* template_url;

  // True if the user has starred the destination URL.
  bool starred;

#ifndef NDEBUG
  // Does a data integrity check on this match.
  void Validate() const;

  // Checks one text/classifications pair for valid values.
  void ValidateClassifications(
      const string16& text,
      const ACMatchClassifications& classifications) const;
#endif
};

typedef AutocompleteMatch::ACMatchClassification ACMatchClassification;
typedef std::vector<ACMatchClassification> ACMatchClassifications;

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_MATCH_H_
