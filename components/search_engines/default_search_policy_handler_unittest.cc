// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/default_search_policy_handler.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "policy/policy_constants.h"

namespace {
// TODO(caitkp): Should we find a way to route this through DefaultSearchManager
// to avoid hardcoding this here?
const char kDefaultSearchProviderData[] =
    "default_search_provider_data.template_url_data";
}  // namespace

namespace policy {

class DefaultSearchPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  DefaultSearchPolicyHandlerTest() {
    default_alternate_urls_.AppendString(
        "http://www.google.com/#q={searchTerms}");
    default_alternate_urls_.AppendString(
        "http://www.google.com/search#q={searchTerms}");
  }

  virtual void SetUp() OVERRIDE {
    handler_list_.AddHandler(make_scoped_ptr<ConfigurationPolicyHandler>(
        new DefaultSearchPolicyHandler));
  }

 protected:
  static const char kSearchURL[];
  static const char kSuggestURL[];
  static const char kIconURL[];
  static const char kName[];
  static const char kKeyword[];
  static const char kReplacementKey[];
  static const char kImageURL[];
  static const char kImageParams[];
  static const char kNewTabURL[];
  static const char kFileSearchURL[];
  static const char kHostName[];

  // Build a default search policy by setting search-related keys in |policy| to
  // reasonable values. You can update any of the keys after calling this
  // method.
  void BuildDefaultSearchPolicy(PolicyMap* policy);

  base::ListValue default_alternate_urls_;
};

const char DefaultSearchPolicyHandlerTest::kSearchURL[] =
    "http://test.com/search?t={searchTerms}";
const char DefaultSearchPolicyHandlerTest::kSuggestURL[] =
    "http://test.com/sugg?={searchTerms}";
const char DefaultSearchPolicyHandlerTest::kIconURL[] =
    "http://test.com/icon.jpg";
const char DefaultSearchPolicyHandlerTest::kName[] =
    "MyName";
const char DefaultSearchPolicyHandlerTest::kKeyword[] =
    "MyKeyword";
const char DefaultSearchPolicyHandlerTest::kReplacementKey[] =
    "espv";
const char DefaultSearchPolicyHandlerTest::kImageURL[] =
    "http://test.com/searchbyimage/upload";
const char DefaultSearchPolicyHandlerTest::kImageParams[] =
    "image_content=content,image_url=http://test.com/test.png";
const char DefaultSearchPolicyHandlerTest::kNewTabURL[] =
    "http://test.com/newtab";
const char DefaultSearchPolicyHandlerTest::kFileSearchURL[] =
    "file:///c:/path/to/search?t={searchTerms}";
const char DefaultSearchPolicyHandlerTest::kHostName[] = "test.com";

void DefaultSearchPolicyHandlerTest::
    BuildDefaultSearchPolicy(PolicyMap* policy) {
  base::ListValue* encodings = new base::ListValue();
  encodings->AppendString("UTF-16");
  encodings->AppendString("UTF-8");
  policy->Set(key::kDefaultSearchProviderEnabled,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              new base::FundamentalValue(true),
              NULL);
  policy->Set(key::kDefaultSearchProviderSearchURL,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              new base::StringValue(kSearchURL),
              NULL);
  policy->Set(key::kDefaultSearchProviderName,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              new base::StringValue(kName),
              NULL);
  policy->Set(key::kDefaultSearchProviderKeyword,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              new base::StringValue(kKeyword),
              NULL);
  policy->Set(key::kDefaultSearchProviderSuggestURL,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              new base::StringValue(kSuggestURL),
              NULL);
  policy->Set(key::kDefaultSearchProviderIconURL,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              new base::StringValue(kIconURL),
              NULL);
  policy->Set(key::kDefaultSearchProviderEncodings, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, encodings, NULL);
  policy->Set(key::kDefaultSearchProviderAlternateURLs, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, default_alternate_urls_.DeepCopy(), NULL);
  policy->Set(key::kDefaultSearchProviderSearchTermsReplacementKey,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              new base::StringValue(kReplacementKey),
              NULL);
  policy->Set(key::kDefaultSearchProviderImageURL,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              new base::StringValue(kImageURL),
              NULL);
  policy->Set(key::kDefaultSearchProviderImageURLPostParams,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              new base::StringValue(kImageParams),
              NULL);
  policy->Set(key::kDefaultSearchProviderNewTabURL,
              POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER,
              new base::StringValue(kNewTabURL),
              NULL);
}

// Checks that if the policy for default search is valid, i.e. there's a
// search URL, that all the elements have been given proper defaults.
TEST_F(DefaultSearchPolicyHandlerTest, MinimallyDefined) {
  PolicyMap policy;
  policy.Set(key::kDefaultSearchProviderEnabled,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(true),
             NULL);
  policy.Set(key::kDefaultSearchProviderSearchURL,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(kSearchURL),
             NULL);
  UpdateProviderPolicy(policy);

  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderSearchURL, &value));
  EXPECT_TRUE(base::StringValue(kSearchURL).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderName, &value));
  EXPECT_TRUE(base::StringValue(kHostName).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderKeyword, &value));
  EXPECT_TRUE(base::StringValue(kHostName).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderSuggestURL,
                               &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderIconURL, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderEncodings, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderInstantURL,
                               &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderAlternateURLs,
                             &value));
  EXPECT_TRUE(base::ListValue().Equals(value));

  EXPECT_TRUE(
      store_->GetValue(prefs::kDefaultSearchProviderSearchTermsReplacementKey,
      &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderImageURL, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(
      prefs::kDefaultSearchProviderSearchURLPostParams, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(
      prefs::kDefaultSearchProviderSuggestURLPostParams, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(
      prefs::kDefaultSearchProviderInstantURLPostParams, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(
      prefs::kDefaultSearchProviderImageURLPostParams, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(
      prefs::kDefaultSearchProviderNewTabURL, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));
}

// Checks that for a fully defined search policy, all elements have been
// read properly.
TEST_F(DefaultSearchPolicyHandlerTest, FullyDefined) {
  PolicyMap policy;
  BuildDefaultSearchPolicy(&policy);
  UpdateProviderPolicy(policy);

  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderSearchURL, &value));
  EXPECT_TRUE(base::StringValue(kSearchURL).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderName, &value));
  EXPECT_TRUE(base::StringValue(kName).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderKeyword, &value));
  EXPECT_TRUE(base::StringValue(kKeyword).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderSuggestURL,
                               &value));
  EXPECT_TRUE(base::StringValue(kSuggestURL).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderIconURL, &value));
  EXPECT_TRUE(base::StringValue(kIconURL).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderEncodings, &value));
  EXPECT_TRUE(base::StringValue("UTF-16;UTF-8").Equals(value));

  EXPECT_TRUE(store_->GetValue(
      prefs::kDefaultSearchProviderAlternateURLs, &value));
  EXPECT_TRUE(default_alternate_urls_.Equals(value));

  EXPECT_TRUE(
      store_->GetValue(prefs::kDefaultSearchProviderSearchTermsReplacementKey,
      &value));
  EXPECT_TRUE(base::StringValue(kReplacementKey).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderImageURL, &value));
  EXPECT_TRUE(base::StringValue(std::string(kImageURL)).Equals(value));

  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderImageURLPostParams,
                               &value));
  EXPECT_TRUE(base::StringValue(std::string(kImageParams)).Equals(value));

  EXPECT_TRUE(store_->GetValue(
      prefs::kDefaultSearchProviderSearchURLPostParams, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(
      prefs::kDefaultSearchProviderSuggestURLPostParams, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));

  EXPECT_TRUE(store_->GetValue(
      prefs::kDefaultSearchProviderInstantURLPostParams, &value));
  EXPECT_TRUE(base::StringValue(std::string()).Equals(value));
}

// Checks that if the default search policy is missing, that no elements of the
// default search policy will be present.
TEST_F(DefaultSearchPolicyHandlerTest, MissingUrl) {
  PolicyMap policy;
  BuildDefaultSearchPolicy(&policy);
  policy.Erase(key::kDefaultSearchProviderSearchURL);
  UpdateProviderPolicy(policy);

  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderSearchURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderName, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderKeyword, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderSuggestURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderIconURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderEncodings, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderAlternateURLs,
                                NULL));
  EXPECT_FALSE(store_->GetValue(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderImageURL, NULL));
  EXPECT_FALSE(store_->GetValue(
      prefs::kDefaultSearchProviderImageURLPostParams, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderInstantURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderNewTabURL, NULL));
}

// Checks that if the default search policy is invalid, that no elements of the
// default search policy will be present.
TEST_F(DefaultSearchPolicyHandlerTest, Invalid) {
  PolicyMap policy;
  BuildDefaultSearchPolicy(&policy);
  const char bad_search_url[] = "http://test.com/noSearchTerms";
  policy.Set(key::kDefaultSearchProviderSearchURL,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(bad_search_url),
             NULL);
  UpdateProviderPolicy(policy);

  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderSearchURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderName, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderKeyword, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderSuggestURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderIconURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderEncodings, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderAlternateURLs,
                                NULL));
  EXPECT_FALSE(store_->GetValue(
      prefs::kDefaultSearchProviderSearchTermsReplacementKey, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderImageURL, NULL));
  EXPECT_FALSE(store_->GetValue(
      prefs::kDefaultSearchProviderImageURLPostParams, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderInstantURL, NULL));
  EXPECT_FALSE(store_->GetValue(prefs::kDefaultSearchProviderNewTabURL, NULL));
}

// Checks that if the default search policy is invalid, that no elements of the
// default search policy will be present.
TEST_F(DefaultSearchPolicyHandlerTest, Disabled) {
  PolicyMap policy;
  policy.Set(key::kDefaultSearchProviderEnabled,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(false),
             NULL);
  UpdateProviderPolicy(policy);

  const base::Value* value = NULL;
  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderEnabled, &value));
  base::FundamentalValue expected_enabled(false);
  EXPECT_TRUE(base::Value::Equals(&expected_enabled, value));
  EXPECT_TRUE(store_->GetValue(prefs::kDefaultSearchProviderSearchURL, &value));
  base::StringValue expected_search_url((std::string()));
  EXPECT_TRUE(base::Value::Equals(&expected_search_url, value));
}

// Checks that for a fully defined search policy, all elements have been
// read properly into the dictionary pref.
TEST_F(DefaultSearchPolicyHandlerTest, DictionaryPref) {
  PolicyMap policy;
  BuildDefaultSearchPolicy(&policy);
  UpdateProviderPolicy(policy);

  const base::Value* temp = NULL;
  const base::DictionaryValue* dictionary;
  std::string value;
  const base::ListValue* list_value;
  EXPECT_TRUE(store_->GetValue(kDefaultSearchProviderData, &temp));
  temp->GetAsDictionary(&dictionary);

  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kURL, &value));
  EXPECT_EQ(kSearchURL, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kShortName, &value));
  EXPECT_EQ(kName, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kKeyword, &value));
  EXPECT_EQ(kKeyword, value);

  EXPECT_TRUE(
      dictionary->GetString(DefaultSearchManager::kSuggestionsURL, &value));
  EXPECT_EQ(kSuggestURL, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kFaviconURL, &value));
  EXPECT_EQ(kIconURL, value);

  base::ListValue encodings;
  encodings.AppendString("UTF-16");
  encodings.AppendString("UTF-8");

  EXPECT_TRUE(
      dictionary->GetList(DefaultSearchManager::kInputEncodings, &list_value));
  EXPECT_TRUE(encodings.Equals(list_value));

  EXPECT_TRUE(
      dictionary->GetList(DefaultSearchManager::kAlternateURLs, &list_value));
  EXPECT_TRUE(default_alternate_urls_.Equals(list_value));

  EXPECT_TRUE(dictionary->GetString(
      DefaultSearchManager::kSearchTermsReplacementKey, &value));
  EXPECT_EQ(kReplacementKey, value);

  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kImageURL, &value));
  EXPECT_EQ(kImageURL, value);

  EXPECT_TRUE(
      dictionary->GetString(DefaultSearchManager::kImageURLPostParams, &value));
  EXPECT_EQ(kImageParams, value);

  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kSearchURLPostParams,
                                    &value));
  EXPECT_EQ(std::string(), value);

  EXPECT_TRUE(dictionary->GetString(
      DefaultSearchManager::kSuggestionsURLPostParams, &value));
  EXPECT_EQ(std::string(), value);

  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kInstantURLPostParams,
                                    &value));
  EXPECT_EQ(std::string(), value);
}

// Checks that disabling default search is properly reflected the dictionary
// pref.
TEST_F(DefaultSearchPolicyHandlerTest, DictionaryPrefDSEDisabled) {
  PolicyMap policy;
  policy.Set(key::kDefaultSearchProviderEnabled,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(false),
             NULL);
  UpdateProviderPolicy(policy);
  const base::Value* temp = NULL;
  const base::DictionaryValue* dictionary;
  EXPECT_TRUE(store_->GetValue(kDefaultSearchProviderData, &temp));
  temp->GetAsDictionary(&dictionary);
  bool disabled = false;
  EXPECT_TRUE(dictionary->GetBoolean(DefaultSearchManager::kDisabledByPolicy,
                                     &disabled));
  EXPECT_TRUE(disabled);
}

// Checks that if the policy for default search is valid, i.e. there's a
// search URL, that all the elements have been given proper defaults.
TEST_F(DefaultSearchPolicyHandlerTest, DictionaryPrefMinimallyDefined) {
  PolicyMap policy;
  policy.Set(key::kDefaultSearchProviderEnabled,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(true),
             NULL);
  policy.Set(key::kDefaultSearchProviderSearchURL,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(kSearchURL),
             NULL);
  UpdateProviderPolicy(policy);

  const base::Value* temp = NULL;
  const base::DictionaryValue* dictionary;
  std::string value;
  const base::ListValue* list_value;
  EXPECT_TRUE(store_->GetValue(kDefaultSearchProviderData, &temp));
  temp->GetAsDictionary(&dictionary);

  // Name and keyword should be derived from host.
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kURL, &value));
  EXPECT_EQ(kSearchURL, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kShortName, &value));
  EXPECT_EQ(kHostName, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kKeyword, &value));
  EXPECT_EQ(kHostName, value);

  // Everything else should be set to the default value.
  EXPECT_TRUE(
      dictionary->GetString(DefaultSearchManager::kSuggestionsURL, &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kFaviconURL, &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(
      dictionary->GetList(DefaultSearchManager::kInputEncodings, &list_value));
  EXPECT_TRUE(base::ListValue().Equals(list_value));
  EXPECT_TRUE(
      dictionary->GetList(DefaultSearchManager::kAlternateURLs, &list_value));
  EXPECT_TRUE(base::ListValue().Equals(list_value));
  EXPECT_TRUE(dictionary->GetString(
      DefaultSearchManager::kSearchTermsReplacementKey, &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kImageURL, &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(
      dictionary->GetString(DefaultSearchManager::kImageURLPostParams, &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kSearchURLPostParams,
                                    &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(dictionary->GetString(
      DefaultSearchManager::kSuggestionsURLPostParams, &value));
  EXPECT_EQ(std::string(), value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kInstantURLPostParams,
                                    &value));
  EXPECT_EQ(std::string(), value);
}

// Checks that setting a file URL as the default search is reflected properly in
// the dictionary pref.
TEST_F(DefaultSearchPolicyHandlerTest, DictionaryPrefFileURL) {
  PolicyMap policy;
  policy.Set(key::kDefaultSearchProviderEnabled,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(true),
             NULL);
  policy.Set(key::kDefaultSearchProviderSearchURL,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(kFileSearchURL),
             NULL);
  UpdateProviderPolicy(policy);

  const base::Value* temp = NULL;
  const base::DictionaryValue* dictionary;
  std::string value;

  EXPECT_TRUE(store_->GetValue(kDefaultSearchProviderData, &temp));
  temp->GetAsDictionary(&dictionary);

  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kURL, &value));
  EXPECT_EQ(kFileSearchURL, value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kShortName, &value));
  EXPECT_EQ("_", value);
  EXPECT_TRUE(dictionary->GetString(DefaultSearchManager::kKeyword, &value));
  EXPECT_EQ("_", value);
}
}  // namespace policy
