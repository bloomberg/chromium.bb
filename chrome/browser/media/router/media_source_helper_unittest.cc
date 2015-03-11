// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_source.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

TEST(MediaSourcesTest, IsMirroringMediaSource) {
  EXPECT_TRUE(IsMirroringMediaSource(ForTabMediaSource(123)));
  EXPECT_TRUE(IsMirroringMediaSource(ForDesktopMediaSource()));
  EXPECT_FALSE(IsMirroringMediaSource(ForCastAppMediaSource("CastApp")));
  EXPECT_FALSE(IsMirroringMediaSource(ForPresentationUrl("http://url")));
}

TEST(MediaSourcesTest, CreateMediaSource) {
  EXPECT_EQ("urn:x-org.chromium.media:source:tab:123",
            ForTabMediaSource(123).id());
  EXPECT_EQ("urn:x-org.chromium.media:source:desktop",
            ForDesktopMediaSource().id());
  EXPECT_EQ("urn:x-com.google.cast:application:DEADBEEF",
            ForCastAppMediaSource("DEADBEEF").id());
  EXPECT_EQ("http://example.com/",
            ForPresentationUrl("http://example.com/").id());
}

TEST(MediaSourcesTest, IsValidMediaSource) {
  EXPECT_TRUE(IsValidMediaSource(ForTabMediaSource(123)));
  EXPECT_TRUE(IsValidMediaSource(ForDesktopMediaSource()));
  EXPECT_TRUE(IsValidMediaSource(ForCastAppMediaSource("DEADBEEF")));
  EXPECT_TRUE(IsValidMediaSource(ForPresentationUrl("http://example.com/")));
  EXPECT_TRUE(IsValidMediaSource(ForPresentationUrl("https://example.com/")));

  // Disallowed scheme
  EXPECT_FALSE(
      IsValidMediaSource(ForPresentationUrl("file:///some/local/path")));
  // Not a URL
  EXPECT_FALSE(IsValidMediaSource(ForPresentationUrl("totally not a url")));
}

}  // namespace media_router

