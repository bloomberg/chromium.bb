// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler.h"
#include "chrome/browser/policy/configuration_policy_handler_list.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(DefaultSearchPolicyHandlerTest, PolicyToPreferenceMapInitialization) {
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_ENABLED].policy_name,
            (const char * const)key::kDefaultSearchProviderEnabled);
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_NAME].policy_name,
            (const char * const)key::kDefaultSearchProviderName);
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_KEYWORD].policy_name,
            (const char * const)key::kDefaultSearchProviderKeyword);
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_SEARCH_URL].policy_name,
            (const char * const)key::kDefaultSearchProviderSearchURL);
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_SUGGEST_URL].policy_name,
            (const char * const)key::kDefaultSearchProviderSuggestURL);
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_INSTANT_URL].policy_name,
            (const char * const)key::kDefaultSearchProviderInstantURL);
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_ICON_URL].policy_name,
            (const char * const)key::kDefaultSearchProviderIconURL);
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_ENCODINGS].policy_name,
            (const char * const)key::kDefaultSearchProviderEncodings);
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_ALTERNATE_URLS].policy_name,
            (const char * const)key::kDefaultSearchProviderAlternateURLs);
  EXPECT_EQ(
      kDefaultSearchPolicyMap[DEFAULT_SEARCH_TERMS_REPLACEMENT_KEY].policy_name,
      (const char * const)key::kDefaultSearchProviderSearchTermsReplacementKey);
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_IMAGE_URL].policy_name,
            (const char * const)key::kDefaultSearchProviderImageURL);
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_NEW_TAB_URL].policy_name,
            (const char * const)key::kDefaultSearchProviderNewTabURL);
  EXPECT_EQ(kDefaultSearchPolicyMap[DEFAULT_SEARCH_SEARCH_URL_POST_PARAMS]
                .policy_name,
            (const char * const)key::kDefaultSearchProviderSearchURLPostParams);
  EXPECT_EQ(
      kDefaultSearchPolicyMap[DEFAULT_SEARCH_SUGGEST_URL_POST_PARAMS]
          .policy_name,
      (const char * const)key::kDefaultSearchProviderSuggestURLPostParams);
  EXPECT_EQ(
      kDefaultSearchPolicyMap[DEFAULT_SEARCH_INSTANT_URL_POST_PARAMS]
          .policy_name,
      (const char * const)key::kDefaultSearchProviderInstantURLPostParams);
  EXPECT_EQ(
      kDefaultSearchPolicyMap[DEFAULT_SEARCH_IMAGE_URL_POST_PARAMS].policy_name,
      (const char * const)key::kDefaultSearchProviderImageURLPostParams);
}

}  // namespace policy
