// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

class AutocompleteController;
struct AutocompleteMatch;
class GURL;
class Profile;

class AutocompleteClassifier {
 public:
  explicit AutocompleteClassifier(Profile* profile);
  virtual ~AutocompleteClassifier();

  // Given some string |text| that the user wants to use for navigation,
  // determines how it should be interpreted.  |desired_tld| is the user's
  // desired TLD, if any; see AutocompleteInput::desired_tld().
  // |allow_exact_keyword_match| should be true when treating the string as a
  // potential keyword search is valid; see
  // AutocompleteInput::allow_exact_keyword_match(). |match| should be a
  // non-NULL outparam that will be set to the default match for this input, if
  // any (for invalid input, there will be no default match, and |match| will be
  // left unchanged).  |alternate_nav_url| is a possibly-NULL outparam that, if
  // non-NULL, will be set to the navigational URL (if any) in case of an
  // accidental search; see comments on
  // AutocompleteResult::alternate_nav_url_ in autocomplete.h.
  void Classify(const std::wstring& text,
                const std::wstring& desired_tld,
                bool allow_exact_keyword_match,
                AutocompleteMatch* match,
                GURL* alternate_nav_url);

 private:
  scoped_ptr<AutocompleteController> controller_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AutocompleteClassifier);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_H_
