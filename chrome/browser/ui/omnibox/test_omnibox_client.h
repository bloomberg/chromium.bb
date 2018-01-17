// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_CLIENT_H_
#define CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_CLIENT_H_

#include "chrome/browser/ui/omnibox/test_omnibox_client.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/sessions/core/session_id.h"

class AutocompleteProviderClient;

class TestOmniboxClient : public OmniboxClient {
 public:
  TestOmniboxClient() : scheme_classifier_(&profile_) {}

  // OmniboxClient:
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;
  const SessionID& GetSessionID() const override;
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override;

 private:
  TestingProfile profile_;
  ChromeAutocompleteSchemeClassifier scheme_classifier_;
  SessionID session_id_;

  DISALLOW_COPY_AND_ASSIGN(TestOmniboxClient);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_CLIENT_H_
