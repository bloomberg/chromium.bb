// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_provider_logos/google_logo_api.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace search_provider_logos {

TEST(GoogleNewLogoApiTest, AppendsQueryParams) {
  const GURL logo_url("https://base.doo/target");

  EXPECT_EQ(
      GURL("https://base.doo/target?async=ntp:1"),
      GoogleNewAppendQueryparamsToLogoURL(false, logo_url, std::string()));
  EXPECT_EQ(GURL("https://base.doo/target?async=ntp:1,graybg:1"),
            GoogleNewAppendQueryparamsToLogoURL(true, logo_url, std::string()));
  EXPECT_EQ(
      GURL("https://base.doo/target?async=ntp:1,es_dfp:fingerprint"),
      GoogleNewAppendQueryparamsToLogoURL(false, logo_url, "fingerprint"));
  EXPECT_EQ(
      GURL("https://base.doo/target?async=ntp:1,graybg:1,es_dfp:fingerprint"),
      GoogleNewAppendQueryparamsToLogoURL(true, logo_url, "fingerprint"));
}

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

TEST(GoogleNewLogoApiTest, ParsesCapturedApiResult) {
  const GURL base_url("https://base.doo/");

  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
  test_data_dir = test_data_dir.AppendASCII("components")
                      .AppendASCII("test")
                      .AppendASCII("data")
                      .AppendASCII("search_provider_logos");

  const struct TestCase {
    const char* file;
    bool has_image_data;
  } test_cases[] = {
      {"ddljson_android0.json", true}, {"ddljson_android0_fp.json", false},
      {"ddljson_android1.json", true}, {"ddljson_android1_fp.json", false},
      {"ddljson_android2.json", true}, {"ddljson_android2_fp.json", false},
      {"ddljson_android3.json", true}, {"ddljson_android3_fp.json", false},
      {"ddljson_android4.json", true}, {"ddljson_android4_fp.json", false},
      {"ddljson_ios0.json", true},     {"ddljson_ios0_fp.json", false},
      {"ddljson_ios1.json", true},     {"ddljson_ios1_fp.json", false},
      {"ddljson_ios2.json", true},     {"ddljson_ios2_fp.json", false},
      {"ddljson_ios3.json", true},     {"ddljson_ios3_fp.json", false},
      {"ddljson_ios4.json", true},     {"ddljson_ios4_fp.json", false},
  };

  for (const TestCase& test_case : test_cases) {
    std::string json;
    ASSERT_TRUE(base::ReadFileToString(
        test_data_dir.AppendASCII(test_case.file), &json))
        << test_case.file;

    bool failed = false;
    std::unique_ptr<EncodedLogo> logo = GoogleNewParseLogoResponse(
        base_url, base::MakeUnique<std::string>(json), base::Time(), &failed);

    EXPECT_FALSE(failed) << test_case.file;
    EXPECT_TRUE(logo) << test_case.file;
    bool has_image_data = logo && logo->encoded_image.get();
    EXPECT_EQ(has_image_data, test_case.has_image_data) << test_case.file;
  }
}

}  // namespace search_provider_logos
