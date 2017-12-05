// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/test_omnibox_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TestOmniboxClient::TestOmniboxClient()
    : autocomplete_classifier_(
          std::make_unique<AutocompleteController>(
              CreateAutocompleteProviderClient(),
              nullptr,
              AutocompleteClassifier::DefaultOmniboxProviders()),
          std::make_unique<TestSchemeClassifier>()) {}

TestOmniboxClient::~TestOmniboxClient() {
  autocomplete_classifier_.Shutdown();
}

std::unique_ptr<AutocompleteProviderClient>
TestOmniboxClient::CreateAutocompleteProviderClient() {
  std::unique_ptr<MockAutocompleteProviderClient> provider_client(
      new testing::NiceMock<MockAutocompleteProviderClient>());
  EXPECT_CALL(*provider_client.get(), GetBuiltinURLs())
      .WillRepeatedly(testing::Return(std::vector<base::string16>()));
  EXPECT_CALL(*provider_client.get(), GetSchemeClassifier())
      .WillRepeatedly(testing::ReturnRef(scheme_classifier_));

  std::unique_ptr<TemplateURLService> template_url_service(
      new TemplateURLService(
          nullptr, std::unique_ptr<SearchTermsData>(new SearchTermsData),
          nullptr, std::unique_ptr<TemplateURLServiceClient>(), nullptr,
          nullptr, base::Closure()));
  provider_client->set_template_url_service(std::move(template_url_service));

  return std::move(provider_client);
}

std::unique_ptr<OmniboxNavigationObserver>
TestOmniboxClient::CreateOmniboxNavigationObserver(
    const base::string16& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternate_nav_match) {
  alternate_nav_match_ = alternate_nav_match;
  return nullptr;
}

const SessionID& TestOmniboxClient::GetSessionID() const {
  return session_id_;
}

const AutocompleteSchemeClassifier& TestOmniboxClient::GetSchemeClassifier()
    const {
  return scheme_classifier_;
}

AutocompleteClassifier* TestOmniboxClient::GetAutocompleteClassifier() {
  return &autocomplete_classifier_;
}
