// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

// Test that the object's getters match the constructor parameters.
TEST(MediaSourceTest, Constructor) {
  MediaSource source1("urn:x-com.google.cast:application:DEADBEEF");
  EXPECT_EQ("urn:x-com.google.cast:application:DEADBEEF", source1.id());
}

}  // namespace media_router
