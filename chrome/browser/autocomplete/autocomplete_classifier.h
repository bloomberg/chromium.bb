// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/profiles/profile_keyed_service.h"

class AutocompleteController;
struct AutocompleteMatch;
class GURL;
class Profile;

class AutocompleteClassifier : public ProfileKeyedService {
 public:
  // Bitmap of AutocompleteProvider::Type values describing the default set of
  // providers queried for the omnibox.  Intended to be passed to
  // AutocompleteController().
  static const int kDefaultOmniboxProviders;

  explicit AutocompleteClassifier(Profile* profile);
  virtual ~AutocompleteClassifier();

  // Given some string |text| that the user wants to use for navigation,
  // determines how it should be interpreted.  |desired_tld| is the user's
  // desired TLD, if any; see AutocompleteInput::desired_tld().
  // |prefer_keyword| should be true the when keyword UI is onscreen; see
  // comments on AutocompleteController::Start().
  // |allow_exact_keyword_match| should be true when treating the string as a
  // potential keyword search is valid; see
  // AutocompleteInput::allow_exact_keyword_match(). |match| should be a
  // non-NULL outparam that will be set to the default match for this input, if
  // any (for invalid input, there will be no default match, and |match| will be
  // left unchanged).  |alternate_nav_url| is a possibly-NULL outparam that, if
  // non-NULL, will be set to the navigational URL (if any) in case of an
  // accidental search; see comments on
  // AutocompleteResult::alternate_nav_url_ in autocomplete.h.
  void Classify(const string16& text,
                const string16& desired_tld,
                bool prefer_keyword,
                bool allow_exact_keyword_match,
                AutocompleteMatch* match,
                GURL* alternate_nav_url);

 private:
  // ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  scoped_ptr<AutocompleteController> controller_;

  // Are we currently in Classify? Used to verify Classify isn't invoked
  // recursively, since this can corrupt state and cause crashes.
  bool inside_classify_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AutocompleteClassifier);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_CLASSIFIER_H_
