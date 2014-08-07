// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_

#include "base/macros.h"
#include "components/omnibox/autocomplete_scheme_classifier.h"

class Profile;

// The subclass to provide chrome-specific scheme handling.
class ChromeAutocompleteSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  explicit ChromeAutocompleteSchemeClassifier(Profile* profile);
  virtual ~ChromeAutocompleteSchemeClassifier();

  // AutocompleteInputSchemeChecker:
  virtual metrics::OmniboxInputType::Type GetInputTypeForScheme(
      const std::string& scheme) const OVERRIDE;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAutocompleteSchemeClassifier);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_CHROME_AUTOCOMPLETE_SCHEME_CLASSIFIER_H_
