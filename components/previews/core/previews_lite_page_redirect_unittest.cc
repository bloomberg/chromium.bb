// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_lite_page_redirect.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/previews/core/previews_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

TEST(PreviewsLitePageURLHandlerTest,
     TestExtractOriginalURLFromLitePageRedirectURL) {
  struct TestCase {
    std::string previews_host;
    std::string original_url;
    std::string previews_url;
    bool want_ok;
  };
  const TestCase kTestCases[]{
      // Use https://play.golang.org/p/HUM2HxmUTOW to compute |previews_url|.
      {
          "https://previews.host.com",
          "https://original.host.com/path/path/path?query=yes",
          "https://shta44dh4bi7rc6fnpjnkrtytwlabygjhk53v2trlot2wddylwua."
          "previews.host.com/p?u="
          "https%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          true,
      },
      {
          "https://previews.host.com",
          "http://original.host.com/path/path/path?query=yes",
          "https://6p7dar4ju6r4ynz7x3pucmlcltuqsf7z5auhvckzln7voglkt56q."
          "previews.host.com/p?u="
          "http%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes",
          true,
      },
      {
          "https://previews.host.com",
          "http://original.host.com/path/path/path?query=yes#fragment",
          "https://6p7dar4ju6r4ynz7x3pucmlcltuqsf7z5auhvckzln7voglkt56q."
          "previews.host.com/p?u="
          "http%3A%2F%2Foriginal.host.com%2Fpath%2Fpath%2Fpath%3Fquery%3Dyes"
          "#fragment",
          true,
      },
  };

  for (const TestCase& test_case : kTestCases) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kLitePageServerPreviews,
        {{"previews_host", test_case.previews_host}});

    std::string original_url;
    bool got_ok = ExtractOriginalURLFromLitePageRedirectURL(
        GURL(test_case.previews_url), &original_url);
    EXPECT_EQ(got_ok, test_case.want_ok);
    EXPECT_EQ(original_url, test_case.original_url);
  }
}

}  // namespace previews
