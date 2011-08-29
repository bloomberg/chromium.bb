// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "media/filters/adaptive_demuxer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(ParseAdaptiveUrlTest, BackwardsCompatible) {
  std::string manifest = "http://youtube.com/video.webm";
  int audio_index;
  int video_index;
  std::vector<std::string> urls;
  ASSERT_TRUE(ParseAdaptiveUrl(manifest, &audio_index, &video_index, &urls));
  EXPECT_EQ(audio_index, 0);
  EXPECT_EQ(video_index, 0);
  ASSERT_EQ(urls.size(), 1u);
  EXPECT_EQ(urls[0], manifest);
}

TEST(ParseAdaptiveUrlTest, CombinedManifest) {
  std::string video1 = "http://youtube.com/video1.webm";
  std::string video2 = "http://youtube.com/video2.webm";
  for (int a = 0; a <= 1; ++a) {
    for (int v = 0; v <= 1; ++v) {
      std::string manifest = "x-adaptive:" +
          base::IntToString(a) + ":" + base::IntToString(v) + ":" +
          video1 + "^" + video2;
      int audio_index;
      int video_index;
      std::vector<std::string> urls;
      ASSERT_TRUE(ParseAdaptiveUrl(
          manifest, &audio_index, &video_index, &urls));
      EXPECT_EQ(audio_index, a);
      EXPECT_EQ(video_index, v);
      ASSERT_EQ(urls.size(), 2u);
      EXPECT_EQ(urls[0], video1);
      EXPECT_EQ(urls[1], video2);
    }
  }
}

TEST(ParseAdaptiveUrlTest, Errors) {
  int audio_index;
  int video_index;
  std::vector<std::string> urls;
  // Indexes are required on x-adaptive URLs.
  EXPECT_FALSE(ParseAdaptiveUrl(
      "x-adaptive:file.webm", &audio_index, &video_index, &urls));
  // Empty URL list is not allowed.
  EXPECT_FALSE(ParseAdaptiveUrl(
      "x-adaptive:0:0:", &audio_index, &video_index, &urls));
  EXPECT_FALSE(ParseAdaptiveUrl(
      "x-adaptive:0:0", &audio_index, &video_index, &urls));
  // Empty URL is not allowed.
  EXPECT_FALSE(ParseAdaptiveUrl(
      "", &audio_index, &video_index, &urls));
  // Malformed indexes parameter.
  EXPECT_FALSE(ParseAdaptiveUrl(
      "x-adaptive:0:file.webm", &audio_index, &video_index, &urls));
  EXPECT_FALSE(ParseAdaptiveUrl(
      "x-adaptive::0:file.webm", &audio_index, &video_index, &urls));
  // Invalid index for URL list.
  EXPECT_FALSE(ParseAdaptiveUrl(
      "x-adaptive:1:0:file.webm", &audio_index, &video_index, &urls));
}

}  // namespace media
