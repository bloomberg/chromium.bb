// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/omnibox/browser/location_bar_model_delegate.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class FakeLocationBarModelDelegate : public LocationBarModelDelegate {
 public:
  void SetURL(const GURL& url) { url_ = url; }
  void SetShouldPreventElision(bool should_prevent_elision) {
    should_prevent_elision_ = should_prevent_elision;
  }
  void SetSecurityInfo(const security_state::SecurityInfo& info) {
    security_info_ = info;
  }

  // LocationBarModelDelegate:
  base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url) const override {
    return formatted_url + base::ASCIIToUTF16("/TestSuffix");
  }

  bool GetURL(GURL* url) const override {
    *url = url_;
    return true;
  }

  bool ShouldPreventElision() const override { return should_prevent_elision_; }

  void GetSecurityInfo(security_state::SecurityInfo* result) const override {
    *result = security_info_;
  }

  AutocompleteClassifier* GetAutocompleteClassifier() override {
    return omnibox_client_.GetAutocompleteClassifier();
  }

  TemplateURLService* GetTemplateURLService() override {
    return omnibox_client_.GetTemplateURLService();
  }

 private:
  GURL url_;
  security_state::SecurityInfo security_info_;
  TestOmniboxClient omnibox_client_;
  bool should_prevent_elision_ = false;
};

class LocationBarModelImplTest : public testing::Test {
 protected:
  const GURL kValidSearchResultsPage =
      GURL("https://www.google.com/search?q=foo+query");

  LocationBarModelImplTest() : model_(&delegate_, 1024) {}

  FakeLocationBarModelDelegate* delegate() { return &delegate_; }

  LocationBarModelImpl* model() { return &model_; }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  FakeLocationBarModelDelegate delegate_;
  LocationBarModelImpl model_;
};

TEST_F(LocationBarModelImplTest,
       DisplayUrlAppliesFormattedStringWithEquivalentMeaning) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({omnibox::kHideSteadyStateUrlScheme,
                                 omnibox::kHideSteadyStateUrlTrivialSubdomains},
                                {});

  delegate()->SetURL(GURL("http://www.google.com/"));

  // Verify that both the full formatted URL and the display URL add the test
  // suffix.
  EXPECT_EQ(base::ASCIIToUTF16("www.google.com/TestSuffix"),
            model()->GetFormattedFullURL());
  EXPECT_EQ(base::ASCIIToUTF16("google.com/TestSuffix"),
            model()->GetURLForDisplay());
}

TEST_F(LocationBarModelImplTest, PreventElisionWorks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {omnibox::kHideSteadyStateUrlScheme,
       omnibox::kHideSteadyStateUrlTrivialSubdomains, omnibox::kQueryInOmnibox},
      {});

  delegate()->SetShouldPreventElision(true);
  delegate()->SetURL(GURL("https://www.google.com/search?q=foo+query+unelide"));

  EXPECT_EQ(base::ASCIIToUTF16(
                "https://www.google.com/search?q=foo+query+unelide/TestSuffix"),
            model()->GetURLForDisplay());

  // Verify that query in omnibox is turned off.
  security_state::SecurityInfo info;
  info.connection_info_initialized = true;
  info.security_level = security_state::SecurityLevel::SECURE;
  delegate()->SetSecurityInfo(info);
  EXPECT_FALSE(model()->GetDisplaySearchTerms(nullptr));
}

TEST_F(LocationBarModelImplTest, QueryInOmniboxFeatureFlagWorks) {
  delegate()->SetURL(kValidSearchResultsPage);
  security_state::SecurityInfo info;
  info.connection_info_initialized = true;
  info.security_level = security_state::SecurityLevel::SECURE;
  delegate()->SetSecurityInfo(info);

  EXPECT_FALSE(model()->GetDisplaySearchTerms(nullptr));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  EXPECT_TRUE(model()->GetDisplaySearchTerms(nullptr));
}

TEST_F(LocationBarModelImplTest, QueryInOmniboxSecurityLevel) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  delegate()->SetURL(kValidSearchResultsPage);

  security_state::SecurityInfo info;
  info.connection_info_initialized = true;

  info.security_level = security_state::SecurityLevel::SECURE;
  delegate()->SetSecurityInfo(info);
  EXPECT_TRUE(model()->GetDisplaySearchTerms(nullptr));

  info.security_level = security_state::SecurityLevel::EV_SECURE;
  delegate()->SetSecurityInfo(info);
  EXPECT_TRUE(model()->GetDisplaySearchTerms(nullptr));

  // Insecure levels should not be allowed to display search terms.
  info.security_level = security_state::SecurityLevel::NONE;
  delegate()->SetSecurityInfo(info);
  EXPECT_FALSE(model()->GetDisplaySearchTerms(nullptr));

  info.security_level = security_state::SecurityLevel::DANGEROUS;
  delegate()->SetSecurityInfo(info);
  EXPECT_FALSE(model()->GetDisplaySearchTerms(nullptr));

  // But ignore the level if the connection info has not been initialized.
  info.connection_info_initialized = false;
  info.security_level = security_state::SecurityLevel::NONE;
  delegate()->SetSecurityInfo(info);
  EXPECT_TRUE(model()->GetDisplaySearchTerms(nullptr));
}

TEST_F(LocationBarModelImplTest,
       QueryInOmniboxDefaultSearchProviderWithAndWithoutQuery) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  security_state::SecurityInfo info;
  info.connection_info_initialized = true;
  info.security_level = security_state::SecurityLevel::SECURE;
  delegate()->SetSecurityInfo(info);

  delegate()->SetURL(kValidSearchResultsPage);
  base::string16 result;
  EXPECT_TRUE(model()->GetDisplaySearchTerms(&result));
  EXPECT_EQ(base::ASCIIToUTF16("foo query"), result);

  const GURL kDefaultSearchProviderURLWithNoQuery(
      "https://www.google.com/maps");
  result.clear();
  delegate()->SetURL(kDefaultSearchProviderURLWithNoQuery);
  EXPECT_FALSE(model()->GetDisplaySearchTerms(&result));
  EXPECT_EQ(base::string16(), result);
}

TEST_F(LocationBarModelImplTest, QueryInOmniboxNonDefaultSearchProvider) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  const GURL kNonDefaultSearchProvider(
      "https://search.yahoo.com/search?ei=UTF-8&fr=crmas&p=foo+query");
  delegate()->SetURL(kNonDefaultSearchProvider);
  security_state::SecurityInfo info;
  info.connection_info_initialized = true;
  info.security_level = security_state::SecurityLevel::SECURE;
  delegate()->SetSecurityInfo(info);

  base::string16 result;
  EXPECT_FALSE(model()->GetDisplaySearchTerms(&result));
  EXPECT_EQ(base::string16(), result);
}

TEST_F(LocationBarModelImplTest, QueryInOmniboxLookalikeURL) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  security_state::SecurityInfo info;
  info.connection_info_initialized = true;
  info.security_level = security_state::SecurityLevel::SECURE;
  delegate()->SetSecurityInfo(info);

  const GURL kLookalikeURLQuery(
      "https://www.google.com/search?q=lookalike.com");
  delegate()->SetURL(kLookalikeURLQuery);

  base::string16 result;
  EXPECT_FALSE(model()->GetDisplaySearchTerms(&result));
  EXPECT_EQ(base::string16(), result);
}

}  // namespace
