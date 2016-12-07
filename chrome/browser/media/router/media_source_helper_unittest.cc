// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media_router {

constexpr char kPresentationUrl[] = "http://www.example.com/presentation.html";

TEST(MediaSourcesTest, IsMirroringMediaSource) {
  EXPECT_TRUE(IsTabMirroringMediaSource(MediaSourceForTab(123)));
  EXPECT_TRUE(IsDesktopMirroringMediaSource(MediaSourceForDesktop()));
  EXPECT_TRUE(IsMirroringMediaSource(MediaSourceForTab(123)));
  EXPECT_TRUE(IsMirroringMediaSource(MediaSourceForDesktop()));
  EXPECT_FALSE(IsMirroringMediaSource(
      MediaSourceForPresentationUrl(GURL(kPresentationUrl))));
}

TEST(MediaSourcesTest, CreateMediaSource) {
  EXPECT_EQ("urn:x-org.chromium.media:source:tab:123",
            MediaSourceForTab(123).id());
  EXPECT_EQ("urn:x-org.chromium.media:source:desktop",
            MediaSourceForDesktop().id());
  EXPECT_EQ(kPresentationUrl,
            MediaSourceForPresentationUrl(GURL(kPresentationUrl)).id());
}

TEST(MediaSourcesTest, IsValidMediaSource) {
  EXPECT_TRUE(IsValidMediaSource(MediaSourceForTab(123)));
  EXPECT_TRUE(IsValidMediaSource(MediaSourceForDesktop()));
  EXPECT_TRUE(IsValidMediaSource(
      MediaSourceForPresentationUrl(GURL(kPresentationUrl))));
  EXPECT_TRUE(IsValidMediaSource(
      MediaSourceForPresentationUrl(GURL(kPresentationUrl))));

  // Disallowed scheme
  EXPECT_FALSE(IsValidMediaSource(
      MediaSourceForPresentationUrl(GURL("file:///some/local/path"))));
  // Not a URL
  EXPECT_FALSE(IsValidMediaSource(
      MediaSourceForPresentationUrl(GURL("totally not a url"))));
}

TEST(MediaSourcesTest, CanConnectToMediaSource) {
  EXPECT_TRUE(CanConnectToMediaSource(
      MediaSource(GURL("https://google.com/cast#__castAppId__=233637DE"))));
  // false scheme
  EXPECT_FALSE(CanConnectToMediaSource(
      MediaSource(GURL("http://google.com/cast#__castAppId__=233637DE"))));
  // false domain
  EXPECT_FALSE(CanConnectToMediaSource(
      MediaSource(GURL("https://google2.com/cast#__castAppId__=233637DE"))));
  // empty path
  EXPECT_FALSE(
      CanConnectToMediaSource(MediaSource(GURL("https://www.google.com"))));
  // false path
  EXPECT_FALSE(CanConnectToMediaSource(
      MediaSource(GURL("https://www.google.com/path"))));

  EXPECT_FALSE(CanConnectToMediaSource(MediaSource(GURL(""))));
}

}  // namespace media_router

