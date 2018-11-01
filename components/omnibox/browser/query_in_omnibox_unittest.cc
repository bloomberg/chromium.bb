// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/query_in_omnibox.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const GURL kValidSearchResultsPage =
    GURL("https://www.google.com/search?q=foo+query");

}  // namespace

class QueryInOmniboxTest : public testing::Test {
 protected:
  QueryInOmniboxTest()
      : omnibox_client_(new TestOmniboxClient),
        model_(new QueryInOmnibox(omnibox_client_->GetAutocompleteClassifier(),
                                  omnibox_client_->GetTemplateURLService())) {}

  QueryInOmnibox* model() { return model_.get(); }

 private:
  base::MessageLoop message_loop_;
  std::unique_ptr<TestOmniboxClient> omnibox_client_;
  std::unique_ptr<QueryInOmnibox> model_;
};

TEST_F(QueryInOmniboxTest, FeatureFlagWorks) {
  EXPECT_FALSE(model()->GetDisplaySearchTerms(
      security_state::SecurityLevel::SECURE, kValidSearchResultsPage, nullptr));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  EXPECT_TRUE(model()->GetDisplaySearchTerms(
      security_state::SecurityLevel::SECURE, kValidSearchResultsPage, nullptr));
}

TEST_F(QueryInOmniboxTest, SecurityLevel) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  EXPECT_TRUE(model()->GetDisplaySearchTerms(
      security_state::SecurityLevel::SECURE, kValidSearchResultsPage, nullptr));
  EXPECT_TRUE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::EV_SECURE,
                                     kValidSearchResultsPage, nullptr));

  // Insecure levels should not be allowed to display search terms.
  EXPECT_FALSE(model()->GetDisplaySearchTerms(
      security_state::SecurityLevel::NONE, kValidSearchResultsPage, nullptr));
  EXPECT_FALSE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::DANGEROUS,
                                     kValidSearchResultsPage, nullptr));

  // But respect the flag on to ignore the security level.
  model()->set_ignore_security_level(true);
  EXPECT_TRUE(model()->GetDisplaySearchTerms(
      security_state::SecurityLevel::NONE, kValidSearchResultsPage, nullptr));
  EXPECT_TRUE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::DANGEROUS,
                                     kValidSearchResultsPage, nullptr));
}

TEST_F(QueryInOmniboxTest, DefaultSearchProviderWithAndWithoutQuery) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  base::string16 result;
  EXPECT_TRUE(model()->GetDisplaySearchTerms(
      security_state::SecurityLevel::SECURE, kValidSearchResultsPage, &result));
  EXPECT_EQ(base::ASCIIToUTF16("foo query"), result);

  const GURL kDefaultSearchProviderURLWithNoQuery(
      "https://www.google.com/maps");
  result.clear();
  EXPECT_FALSE(model()->GetDisplaySearchTerms(
      security_state::SecurityLevel::SECURE,
      kDefaultSearchProviderURLWithNoQuery, &result));
  EXPECT_EQ(base::string16(), result);
}

TEST_F(QueryInOmniboxTest, NonDefaultSearchProvider) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  const GURL kNonDefaultSearchProvider(
      "https://search.yahoo.com/search?ei=UTF-8&fr=crmas&p=foo+query");
  base::string16 result;
  EXPECT_FALSE(
      model()->GetDisplaySearchTerms(security_state::SecurityLevel::SECURE,
                                     kNonDefaultSearchProvider, &result));
  EXPECT_EQ(base::string16(), result);
}

TEST_F(QueryInOmniboxTest, LookalikeURL) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  const GURL kLookalikeURLQuery(
      "https://www.google.com/search?q=lookalike.com");
  base::string16 result;
  EXPECT_FALSE(model()->GetDisplaySearchTerms(
      security_state::SecurityLevel::SECURE, kLookalikeURLQuery, &result));
  EXPECT_EQ(base::string16(), result);
}
