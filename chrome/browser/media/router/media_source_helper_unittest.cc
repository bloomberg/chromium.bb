// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(MediaSourcesTest, IsMirroringMediaSource) {
  EXPECT_TRUE(IsTabMirroringMediaSource(MediaSourceForTab(123)));
  EXPECT_TRUE(IsDesktopMirroringMediaSource(MediaSourceForDesktop()));
  EXPECT_TRUE(IsMirroringMediaSource(MediaSourceForTab(123)));
  EXPECT_TRUE(IsMirroringMediaSource(MediaSourceForDesktop()));
  EXPECT_FALSE(IsMirroringMediaSource(MediaSourceForCastApp("CastApp")));
  EXPECT_FALSE(
      IsMirroringMediaSource(MediaSourceForPresentationUrl("http://url")));
}

TEST(MediaSourcesTest, CreateMediaSource) {
  EXPECT_EQ("urn:x-org.chromium.media:source:tab:123",
            MediaSourceForTab(123).id());
  EXPECT_EQ("urn:x-org.chromium.media:source:desktop",
            MediaSourceForDesktop().id());
  EXPECT_EQ("urn:x-com.google.cast:application:DEADBEEF",
            MediaSourceForCastApp("DEADBEEF").id());
  EXPECT_EQ("http://example.com/",
            MediaSourceForPresentationUrl("http://example.com/").id());
}

TEST(MediaSourcesTest, IsValidMediaSource) {
  EXPECT_TRUE(IsValidMediaSource(MediaSourceForTab(123)));
  EXPECT_TRUE(IsValidMediaSource(MediaSourceForDesktop()));
  EXPECT_TRUE(IsValidMediaSource(MediaSourceForCastApp("DEADBEEF")));
  EXPECT_TRUE(
      IsValidMediaSource(MediaSourceForPresentationUrl("http://example.com/")));
  EXPECT_TRUE(IsValidMediaSource(
      MediaSourceForPresentationUrl("https://example.com/")));

  // Disallowed scheme
  EXPECT_FALSE(IsValidMediaSource(
      MediaSourceForPresentationUrl("file:///some/local/path")));
  // Not a URL
  EXPECT_FALSE(
      IsValidMediaSource(MediaSourceForPresentationUrl("totally not a url")));
}

TEST(MediaSourcesTest, PresentationUrlFromMediaSource) {
  EXPECT_EQ("", PresentationUrlFromMediaSource(MediaSourceForTab(123)));
  EXPECT_EQ("", PresentationUrlFromMediaSource(MediaSourceForDesktop()));
  EXPECT_EQ("http://example.com/",
            PresentationUrlFromMediaSource(
                MediaSourceForPresentationUrl("http://example.com/")));
}

}  // namespace media_router

