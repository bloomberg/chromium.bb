// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/test_omnibox_client.h"

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"

std::unique_ptr<AutocompleteProviderClient>
TestOmniboxClient::CreateAutocompleteProviderClient() {
  return base::MakeUnique<ChromeAutocompleteProviderClient>(&profile_);
}

const SessionID& TestOmniboxClient::GetSessionID() const {
  return session_id_;
}

const AutocompleteSchemeClassifier& TestOmniboxClient::GetSchemeClassifier()
    const {
  return scheme_classifier_;
}
