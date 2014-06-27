// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOCOMPLETE_TEST_SCHEME_CLASSIFIER_
#define COMPONENTS_AUTOCOMPLETE_TEST_SCHEME_CLASSIFIER_

#include "base/macros.h"
#include "components/autocomplete/autocomplete_scheme_classifier.h"

// The subclass of AutocompleteSchemeClassifier for testing.
class TestSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  TestSchemeClassifier();
  virtual ~TestSchemeClassifier();

  // Overridden from AutocompleteInputSchemeChecker:
  virtual metrics::OmniboxInputType::Type GetInputTypeForScheme(
      const std::string& scheme) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSchemeClassifier);
};

#endif  // COMPONENTS_AUTOCOMPLETE_TEST_SCHEME_CLASSIFIER_
