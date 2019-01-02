// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/flash_embed_rewrite.h"

#include "base/metrics/histogram_samples.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using YouTubeRewriteStatus = FlashEmbedRewrite::YouTubeRewriteStatus;

class FlashEmbedRewriteTest : public testing::Test {
 public:
  FlashEmbedRewriteTest() = default;

  std::unique_ptr<base::HistogramSamples> GetHistogramSamples() {
    return histogram_tester_.GetHistogramSamplesSinceCreation(
        FlashEmbedRewrite::kFlashYouTubeRewriteUMA);
  }

 private:
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(FlashEmbedRewriteTest);
};

TEST_F(FlashEmbedRewriteTest, YouTubeRewriteEmbed) {
  struct TestData {
    std::string original;
    std::string expected;
  } test_data[] = {
      // { original, expected }
      {"http://youtube.com", ""},
      {"http://www.youtube.com", ""},
      {"https://www.youtube.com", ""},
      {"http://www.foo.youtube.com", ""},
      {"https://www.foo.youtube.com", ""},
      // Non-YouTube domains shouldn't be modified
      {"http://www.plus.google.com", ""},
      // URL isn't using Flash
      {"http://www.youtube.com/embed/deadbeef", ""},
      // URL isn't using Flash, no www
      {"http://youtube.com/embed/deadbeef", ""},
      // URL isn't using Flash, invalid parameter construct
      {"http://www.youtube.com/embed/deadbeef&start=4", ""},
      // URL is using Flash, no www
      {"http://youtube.com/v/deadbeef", "http://youtube.com/embed/deadbeef"},
      // URL is using Flash, is valid, https
      {"https://www.youtube.com/v/deadbeef",
       "https://www.youtube.com/embed/deadbeef"},
      // URL is using Flash, is valid, http
      {"http://www.youtube.com/v/deadbeef",
       "http://www.youtube.com/embed/deadbeef"},
      // URL is using Flash, valid
      {"https://www.foo.youtube.com/v/deadbeef",
       "https://www.foo.youtube.com/embed/deadbeef"},
      // URL is using Flash, is valid, has one parameter
      {"http://www.youtube.com/v/deadbeef?start=4",
       "http://www.youtube.com/embed/deadbeef?start=4"},
      // URL is using Flash, is valid, has multiple parameters
      {"http://www.youtube.com/v/deadbeef?start=4&fs=1",
       "http://www.youtube.com/embed/deadbeef?start=4&fs=1"},
      // URL is using Flash, invalid parameter construct, has one parameter
      {"http://www.youtube.com/v/deadbeef&start=4",
       "http://www.youtube.com/embed/deadbeef?start=4"},
      // URL is using Flash, invalid parameter construct, has multiple
      // parameters
      {"http://www.youtube.com/v/deadbeef&start=4&fs=1?foo=bar",
       "http://www.youtube.com/embed/deadbeef?start=4&fs=1&foo=bar"},
      // URL is using Flash, invalid parameter construct, has multiple
      // parameters
      {"http://www.youtube.com/v/deadbeef&start=4&fs=1",
       "http://www.youtube.com/embed/deadbeef?start=4&fs=1"},
      // Invalid parameter construct
      {"http://www.youtube.com/abcd/v/deadbeef", ""},
      // Invalid parameter construct
      {"http://www.youtube.com/v/abcd/", "http://www.youtube.com/embed/abcd/"},
      // Invalid parameter construct
      {"http://www.youtube.com/v/123/", "http://www.youtube.com/embed/123/"},
      // youtube-nocookie.com
      {"http://www.youtube-nocookie.com/v/123/",
       "http://www.youtube-nocookie.com/embed/123/"},
      // youtube-nocookie.com, isn't using flash
      {"http://www.youtube-nocookie.com/embed/123/", ""},
      // youtube-nocookie.com, has one parameter
      {"http://www.youtube-nocookie.com/v/123?start=foo",
       "http://www.youtube-nocookie.com/embed/123?start=foo"},
      // youtube-nocookie.com, has multiple parameters
      {"http://www.youtube-nocookie.com/v/123?start=foo&bar=baz",
       "http://www.youtube-nocookie.com/embed/123?start=foo&bar=baz"},
      // youtube-nocookie.com, invalid parameter construct, has one parameter
      {"http://www.youtube-nocookie.com/v/123&start=foo",
       "http://www.youtube-nocookie.com/embed/123?start=foo"},
      // youtube-nocookie.com, invalid parameter construct, has multiple
      // parameters
      {"http://www.youtube-nocookie.com/v/123&start=foo&bar=baz",
       "http://www.youtube-nocookie.com/embed/123?start=foo&bar=baz"},
      // youtube-nocookie.com, https
      {"https://www.youtube-nocookie.com/v/123/",
       "https://www.youtube-nocookie.com/embed/123/"},
      // URL isn't using Flash, has JS API enabled
      {"http://www.youtube.com/embed/deadbeef?enablejsapi=1", ""},
      // URL is using Flash, has JS API enabled
      {"http://www.youtube.com/v/deadbeef?enablejsapi=1",
       "http://www.youtube.com/embed/deadbeef?enablejsapi=1"},
      // youtube-nocookie.com, has JS API enabled
      {"http://www.youtube-nocookie.com/v/123?enablejsapi=1",
       "http://www.youtube-nocookie.com/embed/123?enablejsapi=1"},
      // URL is using Flash, has JS API enabled, invalid parameter construct
      {"http://www.youtube.com/v/deadbeef&enablejsapi=1",
       "http://www.youtube.com/embed/deadbeef?enablejsapi=1"},
      // URL is using Flash, has JS API enabled, invalid parameter construct,
      // has multiple parameters
      {"http://www.youtube.com/v/deadbeef&start=4&enablejsapi=1",
       "http://www.youtube.com/embed/deadbeef?start=4&enablejsapi=1"},
  };

  for (const auto& data : test_data) {
    EXPECT_EQ(GURL(data.expected),
              FlashEmbedRewrite::RewriteFlashEmbedURL(GURL(data.original)));
  }
}

TEST_F(FlashEmbedRewriteTest, YouTubeRewriteEmbedIneligibleURL) {
  std::unique_ptr<base::HistogramSamples> samples = GetHistogramSamples();
  EXPECT_EQ(0, samples->TotalCount());

  const std::string test_data[] = {
      // HTTP, www, no flash
      "http://www.youtube.com",
      // No flash, subdomain
      "http://www.foo.youtube.com",
      // Not youtube
      "http://www.plus.google.com",
      // Already using HTML5
      "http://youtube.com/embed/deadbeef",
      // Already using HTML5, enablejsapi=1
      "http://www.youtube.com/embed/deadbeef?enablejsapi=1"};

  for (const auto& data : test_data) {
    FlashEmbedRewrite::RewriteFlashEmbedURL(GURL(data));
    samples = GetHistogramSamples();
    EXPECT_EQ(0, samples->GetCount((int)YouTubeRewriteStatus::kSuccess));
    EXPECT_EQ(0, samples->TotalCount());
  }
}

TEST_F(FlashEmbedRewriteTest, YouTubeRewriteEmbedSuccess) {
  std::unique_ptr<base::HistogramSamples> samples = GetHistogramSamples();
  auto total_count = 0;
  EXPECT_EQ(total_count, samples->TotalCount());

  const std::string test_data[] = {
      // HTTP, www, flash
      "http://www.youtube.com/v/deadbeef",
      // HTTP, no www, flash
      "http://youtube.com/v/deadbeef",
      // HTTPS, www, flash
      "https://www.youtube.com/v/deadbeef",
      // HTTPS, no www, flash
      "https://youtube.com/v/deadbeef",
      // Invalid parameter construct
      "http://www.youtube.com/v/abcd/",
      // Invalid parameter construct
      "http://www.youtube.com/v/1234/",
  };

  for (const auto& data : test_data) {
    ++total_count;
    FlashEmbedRewrite::RewriteFlashEmbedURL(GURL(data));
    samples = GetHistogramSamples();
    EXPECT_EQ(total_count,
              samples->GetCount((int)YouTubeRewriteStatus::kSuccess));
    EXPECT_EQ(total_count, samples->TotalCount());
  }

  // Invalid parameter construct
  FlashEmbedRewrite::RewriteFlashEmbedURL(
      GURL("http://www.youtube.com/abcd/v/deadbeef"));
  samples = GetHistogramSamples();
  EXPECT_EQ(total_count,
            samples->GetCount((int)YouTubeRewriteStatus::kSuccess));
  EXPECT_EQ(total_count, samples->TotalCount());
}

TEST_F(FlashEmbedRewriteTest, YouTubeRewriteEmbedSuccessRewrite) {
  std::unique_ptr<base::HistogramSamples> samples = GetHistogramSamples();
  auto total_count = 0;
  EXPECT_EQ(total_count, samples->TotalCount());

  const std::string test_data[] = {
      // Invalid parameter construct, one parameter
      "http://www.youtube.com/v/deadbeef&start=4",
      // Invalid parameter construct, has multiple parameters
      "http://www.youtube.com/v/deadbeef&start=4&fs=1?foo=bar",
  };

  for (const auto& data : test_data) {
    ++total_count;
    FlashEmbedRewrite::RewriteFlashEmbedURL(GURL(data));
    samples = GetHistogramSamples();
    EXPECT_EQ(
        total_count,
        samples->GetCount((int)YouTubeRewriteStatus::kSuccessParamsRewrite));
    EXPECT_EQ(total_count, samples->TotalCount());
  }

  // Invalid parameter construct, not flash
  FlashEmbedRewrite::RewriteFlashEmbedURL(
      GURL("http://www.youtube.com/embed/deadbeef&start=4"));
  samples = GetHistogramSamples();
  EXPECT_EQ(total_count, samples->GetCount(
                             (int)YouTubeRewriteStatus::kSuccessParamsRewrite));
  EXPECT_EQ(total_count, samples->TotalCount());
}

TEST_F(FlashEmbedRewriteTest, YouTubeRewriteEmbedJSAPI) {
  std::unique_ptr<base::HistogramSamples> samples = GetHistogramSamples();
  auto total_count = 0;
  EXPECT_EQ(total_count, samples->TotalCount());

  const std::string test_data[] = {
      // Valid parameter construct, one parameter
      "http://www.youtube.com/v/deadbeef?enablejsapi=1",
      // Valid parameter construct, has multiple parameters
      "http://www.youtube.com/v/deadbeef?enablejsapi=1&foo=2",
      // Invalid parameter construct, one parameter
      "http://www.youtube.com/v/deadbeef&enablejsapi=1",
      // Invalid parameter construct, has multiple parameters
      "http://www.youtube.com/v/deadbeef&enablejsapi=1&foo=2"};

  for (const auto& data : test_data) {
    ++total_count;
    FlashEmbedRewrite::RewriteFlashEmbedURL(GURL(data));
    samples = GetHistogramSamples();
    EXPECT_EQ(total_count, samples->GetCount(
                               (int)YouTubeRewriteStatus::kSuccessEnableJSAPI));
    EXPECT_EQ(total_count, samples->TotalCount());
  }
}

TEST_F(FlashEmbedRewriteTest, DailymotionRewriteEmbed) {
  struct TestData {
    std::string original;
    std::string expected;
  } test_data[] = {
      // { original, expected }
      {"http://dailymotion.com", ""},
      {"http://www.dailymotion.com", ""},
      {"https://www.dailymotion.com", ""},
      {"http://www.foo.dailymotion.com", ""},
      {"https://www.foo.dailymotion.com", ""},
      // URL isn't using Flash
      {"http://www.dailymotion.com/embed/video/deadbeef", ""},
      // URL isn't using Flash, no www
      {"http://dailymotion.com/embed/video/deadbeef", ""},
      // URL isn't using Flash, invalid parameter construct
      {"http://www.dailymotion.com/embed/video/deadbeef&start=4", ""},
      // URL is using Flash, no www
      {"http://dailymotion.com/swf/deadbeef",
       "http://dailymotion.com/embed/video/deadbeef"},
      // URL is using Flash, is valid, https
      {"https://www.dailymotion.com/swf/deadbeef",
       "https://www.dailymotion.com/embed/video/deadbeef"},
      // URL is using Flash, is valid, http
      {"http://www.dailymotion.com/swf/deadbeef",
       "http://www.dailymotion.com/embed/video/deadbeef"},
      // URL is using Flash, valid
      {"https://www.foo.dailymotion.com/swf/deadbeef",
       "https://www.foo.dailymotion.com/embed/video/deadbeef"},
      // URL is using Flash, is valid, has one parameter
      {"http://www.dailymotion.com/swf/deadbeef?start=4",
       "http://www.dailymotion.com/embed/video/deadbeef?start=4"},
      // URL is using Flash, is valid, has multiple parameters
      {"http://www.dailymotion.com/swf/deadbeef?start=4&fs=1",
       "http://www.dailymotion.com/embed/video/deadbeef?start=4&fs=1"},
      // URL is using Flash, invalid parameter construct, has one parameter
      {"http://www.dailymotion.com/swf/deadbeef&start=4",
       "http://www.dailymotion.com/embed/video/deadbeef&start=4"},
      // Invalid URL.
      {"http://www.dailymotion.com/abcd/swf/deadbeef", ""},
      // Uses /swf/video/
      {"http://www.dailymotion.com/swf/video/deadbeef",
       "http://www.dailymotion.com/embed/video/deadbeef"}};

  for (const auto& data : test_data) {
    EXPECT_EQ(GURL(data.expected),
              FlashEmbedRewrite::RewriteFlashEmbedURL(GURL(data.original)));
  }
}
