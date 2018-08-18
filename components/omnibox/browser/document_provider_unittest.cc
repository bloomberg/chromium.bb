// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/document_provider.h"

#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeAutocompleteProviderClient : public MockAutocompleteProviderClient {
 public:
  FakeAutocompleteProviderClient()
      : template_url_service_(new TemplateURLService(nullptr, 0)) {
    pref_service_.registry()->RegisterBooleanPref(
        omnibox::kDocumentSuggestEnabled, true);
  }

  bool SearchSuggestEnabled() const override { return true; }

  TemplateURLService* GetTemplateURLService() override {
    return template_url_service_.get();
  }

  TemplateURLService* GetTemplateURLService() const override {
    return template_url_service_.get();
  }

  PrefService* GetPrefs() override { return &pref_service_; }

 private:
  std::unique_ptr<TemplateURLService> template_url_service_;
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeAutocompleteProviderClient);
};

}  // namespace

class DocumentProviderTest : public testing::Test,
                             public AutocompleteProviderListener {
 public:
  DocumentProviderTest();

  void SetUp() override;

 protected:
  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override;

  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<DocumentProvider> provider_;
  TemplateURL* default_template_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DocumentProviderTest);
};

DocumentProviderTest::DocumentProviderTest() {}

void DocumentProviderTest::SetUp() {
  client_.reset(new FakeAutocompleteProviderClient());

  TemplateURLService* turl_model = client_->GetTemplateURLService();
  turl_model->Load();

  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16("t"));
  data.SetURL("https://www.google.com/?q={searchTerms}");
  data.suggestions_url = "https://www.google.com/complete/?q={searchTerms}";
  default_template_url_ = turl_model->Add(std::make_unique<TemplateURL>(data));
  turl_model->SetUserSelectedDefaultSearchProvider(default_template_url_);

  provider_ = DocumentProvider::Create(client_.get(), this);
}

void DocumentProviderTest::OnProviderUpdate(bool updated_matches) {
  // No action required.
}

TEST_F(DocumentProviderTest, CheckFeatureBehindFlag) {
  PrefService* fake_prefs = client_->GetPrefs();
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  bool is_incognito = false;
  bool is_authenticated = true;

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(omnibox::kDocumentProvider);
  EXPECT_FALSE(provider_->IsDocumentProviderAllowed(
      fake_prefs, is_incognito, is_authenticated, template_url_service));
}

TEST_F(DocumentProviderTest, CheckFeaturePrerequisiteNoIncognito) {
  PrefService* fake_prefs = client_->GetPrefs();
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  bool is_incognito = false;
  bool is_authenticated = true;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kDocumentProvider);

  // Feature starts enabled.
  EXPECT_TRUE(provider_->IsDocumentProviderAllowed(
      fake_prefs, is_incognito, is_authenticated, template_url_service));

  // Feature should be disabled in incognito.
  is_incognito = true;
  EXPECT_FALSE(provider_->IsDocumentProviderAllowed(
      fake_prefs, is_incognito, is_authenticated, template_url_service));
}

TEST_F(DocumentProviderTest, CheckFeaturePrerequisiteClientSettingOff) {
  PrefService* fake_prefs = client_->GetPrefs();
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  bool is_incognito = false;
  bool is_authenticated = true;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kDocumentProvider);

  // Feature starts enabled.
  EXPECT_TRUE(provider_->IsDocumentProviderAllowed(
      fake_prefs, is_incognito, is_authenticated, template_url_service));

  // Disabling toggle in chrome://settings should be respected.
  fake_prefs->SetBoolean(omnibox::kDocumentSuggestEnabled, false);
  EXPECT_FALSE(provider_->IsDocumentProviderAllowed(
      fake_prefs, is_incognito, is_authenticated, template_url_service));
  fake_prefs->SetBoolean(omnibox::kDocumentSuggestEnabled, true);
}

TEST_F(DocumentProviderTest, CheckFeaturePrerequisiteDefaultSearch) {
  PrefService* fake_prefs = client_->GetPrefs();
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  bool is_incognito = false;
  bool is_authenticated = true;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kDocumentProvider);

  // Feature starts enabled.
  EXPECT_TRUE(provider_->IsDocumentProviderAllowed(
      fake_prefs, is_incognito, is_authenticated, template_url_service));

  // Switching default search disables it.
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16("t"));
  data.SetURL("https://www.notgoogle.com/?q={searchTerms}");
  data.suggestions_url = "https://www.notgoogle.com/complete/?q={searchTerms}";
  TemplateURL* new_default_provider =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      new_default_provider);
  EXPECT_FALSE(provider_->IsDocumentProviderAllowed(
      fake_prefs, is_incognito, is_authenticated, template_url_service));
  template_url_service->SetUserSelectedDefaultSearchProvider(
      default_template_url_);
  template_url_service->Remove(new_default_provider);
}

TEST_F(DocumentProviderTest, CheckFeaturePrerequisiteServerBackoff) {
  PrefService* fake_prefs = client_->GetPrefs();
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  bool is_incognito = false;
  bool is_authenticated = true;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(omnibox::kDocumentProvider);

  // Feature starts enabled.
  EXPECT_TRUE(provider_->IsDocumentProviderAllowed(
      fake_prefs, is_incognito, is_authenticated, template_url_service));

  // Server setting backoff flag disables it.
  provider_->backoff_for_session_ = true;
  EXPECT_FALSE(provider_->IsDocumentProviderAllowed(
      fake_prefs, is_incognito, is_authenticated, template_url_service));
  provider_->backoff_for_session_ = false;
}

TEST_F(DocumentProviderTest, ParseDocumentSearchResults) {
  const char kGoodJSONResponse[] = R"({
      "results": [
        {
          "title": "Document 1",
          "url": "https://documentprovider.tld/doc?id=1",
          "score": 1234,
          "originalUrl": "https://shortened.url"
        },
        {
          "title": "Document 2",
          "url": "https://documentprovider.tld/doc?id=2"
        }
      ]
     })";

  std::unique_ptr<base::DictionaryValue> response =
      base::DictionaryValue::From(base::JSONReader::Read(kGoodJSONResponse));
  ASSERT_TRUE(response != nullptr);

  ACMatches matches;
  provider_->ParseDocumentSearchResults(*response, &matches);
  EXPECT_EQ(matches.size(), 2u);

  EXPECT_EQ(matches[0].contents, base::ASCIIToUTF16("Document 1"));
  EXPECT_EQ(matches[0].destination_url,
            GURL("https://documentprovider.tld/doc?id=1"));
  EXPECT_EQ(matches[0].relevance, 1234);  // Server-specified.
  EXPECT_EQ(matches[0].stripped_destination_url, GURL("https://shortened.url"));

  EXPECT_EQ(matches[1].contents, base::ASCIIToUTF16("Document 2"));
  EXPECT_EQ(matches[1].destination_url,
            GURL("https://documentprovider.tld/doc?id=2"));
  EXPECT_EQ(matches[1].relevance, 700);  // From study default.
  EXPECT_TRUE(matches[1].stripped_destination_url.is_empty());

  ASSERT_FALSE(provider_->backoff_for_session_);
}

TEST_F(DocumentProviderTest, ParseDocumentSearchResultsWithBackoff) {
  // Response where the server wishes to trigger backoff.
  const char kBackoffJSONResponse[] = R"({
      "error": {
        "code": 503,
        "message": "Not eligible to query, see retry info.",
        "status": "UNAVAILABLE",
        "details": [
          {
            "@type": "type.googleapis.com/google.rpc.RetryInfo",
            "retryDelay": "100000s"
          },
        ]
      }
    })";

  ASSERT_FALSE(provider_->backoff_for_session_);
  std::unique_ptr<base::DictionaryValue> backoff_response =
      base::DictionaryValue::From(base::JSONReader::Read(
          kBackoffJSONResponse, base::JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(backoff_response != nullptr);

  ACMatches matches;
  provider_->ParseDocumentSearchResults(*backoff_response, &matches);
  ASSERT_TRUE(provider_->backoff_for_session_);
}

TEST_F(DocumentProviderTest, ParseDocumentSearchResultsWithIneligibleFlag) {
  // Response where the server wishes to trigger backoff.
  const char kIneligibleJSONResponse[] = R"({
      "error": {
        "code": 403,
        "message": "Not eligible to query due to admin disabled Chrome search settings.",
        "status": "PERMISSION_DENIED",
      }
    })";

  // Same as above, but the message doesn't match. We should accept this
  // response, but it isn't expected to trigger backoff.
  const char kMismatchedMessageJSON[] = R"({
      "error": {
        "code": 403,
        "message": "Some other thing went wrong.",
        "status": "PERMISSION_DENIED",
      }
    })";

  ACMatches matches;
  ASSERT_FALSE(provider_->backoff_for_session_);

  // First, parse an invalid response - shouldn't prohibit future requests
  // from working but also shouldn't trigger backoff.
  std::unique_ptr<base::DictionaryValue> bad_response =
      base::DictionaryValue::From(base::JSONReader::Read(
          kMismatchedMessageJSON, base::JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(bad_response != nullptr);
  provider_->ParseDocumentSearchResults(*bad_response, &matches);
  ASSERT_FALSE(provider_->backoff_for_session_);

  // Now parse a response that does trigger backoff.
  std::unique_ptr<base::DictionaryValue> backoff_response =
      base::DictionaryValue::From(base::JSONReader::Read(
          kIneligibleJSONResponse, base::JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(backoff_response != nullptr);
  provider_->ParseDocumentSearchResults(*backoff_response, &matches);
  ASSERT_TRUE(provider_->backoff_for_session_);
}
