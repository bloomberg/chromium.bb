// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/features.h"

#include <map>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ntp_snippets {

namespace {
const char kTestFeedURL[] = "https://test.google.com/";
}  // namespace

TEST(FeaturesTest, GetContentSuggestionsReferrerURL_DefaultValue) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kArticleSuggestionsFeature);
  EXPECT_EQ("https://discover.google.com/", GetContentSuggestionsReferrerURL());

  // In code this will be often used inside of a GURL.
  EXPECT_EQ("https://discover.google.com/",
            GURL(GetContentSuggestionsReferrerURL()));
  EXPECT_EQ("https://discover.google.com/",
            GURL(GetContentSuggestionsReferrerURL()).spec());
}

TEST(FeaturesTest, GetContentSuggestionsReferrerURL_ParamValue) {
  base::test::ScopedFeatureList feature_list;

  std::map<std::string, std::string> parameters;
  parameters["referrer_url"] = kTestFeedURL;
  feature_list.InitAndEnableFeatureWithParameters(kArticleSuggestionsFeature,
                                                  parameters);
  EXPECT_EQ(kTestFeedURL, GetContentSuggestionsReferrerURL());
}

}  // namespace ntp_snippets
