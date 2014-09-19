// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/scheme_classifier_factory.h"

#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"

namespace athena {

scoped_ptr<AutocompleteSchemeClassifier> CreateSchemeClassifier(
    content::BrowserContext* context) {
  return scoped_ptr<AutocompleteSchemeClassifier>(
      new ChromeAutocompleteSchemeClassifier(
          Profile::FromBrowserContext(context)));
}

}  // namespace athena
