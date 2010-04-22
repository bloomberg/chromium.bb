// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_VERSUS_NAVIGATE_CLASSIFIER_H_
#define CHROME_BROWSER_SEARCH_VERSUS_NAVIGATE_CLASSIFIER_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/common/page_transition_types.h"

class AutocompleteController;
class GURL;
class Profile;

class SearchVersusNavigateClassifier {
 public:
  explicit SearchVersusNavigateClassifier(Profile* profile);
  virtual ~SearchVersusNavigateClassifier();

  // Given some string |text| that the user wants to use for navigation,
  // determines whether to treat it as a search query or a URL, and returns the
  // details of the resulting navigation.
  // NOTE: After |desired_tld|, all parameters are potentially-NULL outparams.
  // |desired_tld|        - User's desired TLD.
  //                        See AutocompleteInput::desired_tld().
  // |is_search|          - Set to true if this is to be treated as a
  //                        query rather than URL.
  // |destination_url|    - The URL to load. It may be empty if there is no
  //                        possible navigation (when |text| is empty).
  // |transition|         - The transition type.
  // |is_history_what_you_typed_match|
  //                      - Set to true when the default match is the
  //                        "what you typed" match from the history.
  // |alternate_nav_url|  - The navigational URL in case of an accidental
  //                        search; see comments on
  //                        AutocompleteResult::alternate_nav_url_ in
  //                        autocomplete.h.
  void Classify(const std::wstring& text,
                const std::wstring& desired_tld,
                bool* is_search,
                GURL* destination_url,
                PageTransition::Type* transition,
                bool* is_history_what_you_typed_match,
                GURL* alternate_nav_url);

 private:
  scoped_ptr<AutocompleteController> controller_;
};

#endif  // CHROME_BROWSER_SEARCH_VERSUS_NAVIGATE_CLASSIFIER_H_
