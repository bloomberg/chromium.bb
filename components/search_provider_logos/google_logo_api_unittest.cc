// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/google_logo_api.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Eq;

namespace search_provider_logos {

TEST(GoogleNewLogoApiTest, ResolvesRelativeUrl) {
  const GURL base_url("https://base.doo/");
  const std::string json = R"json()]}'
{
  "ddljson": {
    "target_url": "/target"
  }
})json";

  bool failed = false;
  std::unique_ptr<EncodedLogo> logo = GoogleNewParseLogoResponse(
      base_url, base::MakeUnique<std::string>(json), base::Time(), &failed);

  ASSERT_FALSE(failed);
  ASSERT_TRUE(logo);
  EXPECT_EQ(GURL("https://base.doo/target"), logo->metadata.on_click_url);
}

TEST(GoogleNewLogoApiTest, DoesNotResolveAbsoluteUrl) {
  const GURL base_url("https://base.doo/");
  const std::string json = R"json()]}'
{
  "ddljson": {
    "target_url": "https://www.doodle.com/target"
  }
})json";

  bool failed = false;
  std::unique_ptr<EncodedLogo> logo = GoogleNewParseLogoResponse(
      base_url, base::MakeUnique<std::string>(json), base::Time(), &failed);

  ASSERT_FALSE(failed);
  ASSERT_TRUE(logo);
  EXPECT_EQ(GURL("https://www.doodle.com/target"), logo->metadata.on_click_url);
}

TEST(GoogleNewLogoApiTest, ParsesStaticImage) {
  const GURL base_url("https://base.doo/");
  // Note: The base64 encoding of "abc" is "YWJj".
  const std::string json = R"json()]}'
{
  "ddljson": {
    "target_url": "/target",
    "data_uri": "data:image/png;base64,YWJj"
  }
})json";

  bool failed = false;
  std::unique_ptr<EncodedLogo> logo = GoogleNewParseLogoResponse(
      base_url, base::MakeUnique<std::string>(json), base::Time(), &failed);

  ASSERT_FALSE(failed);
  ASSERT_TRUE(logo);
  EXPECT_EQ("abc", logo->encoded_image->data());
}

TEST(GoogleNewLogoApiTest, ParsesAnimatedImage) {
  const GURL base_url("https://base.doo/");
  // Note: The base64 encoding of "abc" is "YWJj".
  const std::string json = R"json()]}'
{
  "ddljson": {
    "target_url": "/target",
    "large_image": {
      "is_animated_gif": true,
      "url": "https://www.doodle.com/image.gif"
    },
    "cta_data_uri": "data:image/png;base64,YWJj"
  }
})json";

  bool failed = false;
  std::unique_ptr<EncodedLogo> logo = GoogleNewParseLogoResponse(
      base_url, base::MakeUnique<std::string>(json), base::Time(), &failed);

  ASSERT_FALSE(failed);
  ASSERT_TRUE(logo);
  EXPECT_EQ(GURL("https://www.doodle.com/image.gif"),
            logo->metadata.animated_url);
  EXPECT_EQ("abc", logo->encoded_image->data());
}

}  // namespace search_provider_logos
