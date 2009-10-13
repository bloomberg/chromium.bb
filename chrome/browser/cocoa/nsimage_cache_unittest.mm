// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/file_path.h"
#include "base/mac_util.h"
#include "base/nsimage_cache_mac.h"
#include "base/path_service.h"
#include "chrome/common/chrome_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// This tests nsimage_cache, which lives in base/.  The unit test is in
// chrome/ because it depends on having a built-up Chrome present.

namespace {

class NSImageCacheTest : public PlatformTest {
 public:
  NSImageCacheTest() {
    // Look in the framework bundle for resources.
    FilePath path;
    PathService::Get(base::DIR_EXE, &path);
    path = path.Append(chrome::kFrameworkName);
    mac_util::SetOverrideAppBundlePath(path);
  }
  virtual ~NSImageCacheTest() {
    mac_util::SetOverrideAppBundle(nil);
  }
};

TEST_F(NSImageCacheTest, LookupFound) {
  EXPECT_TRUE(nsimage_cache::ImageNamed(@"back_Template.pdf") != nil)
      << "Failed to find the toolbar image?";
}

TEST_F(NSImageCacheTest, LookupCached) {
  EXPECT_EQ(nsimage_cache::ImageNamed(@"back_Template.pdf"),
            nsimage_cache::ImageNamed(@"back_Template.pdf"))
    << "Didn't get the same NSImage back?";
}

TEST_F(NSImageCacheTest, LookupMiss) {
  EXPECT_TRUE(nsimage_cache::ImageNamed(@"should_not.exist") == nil)
      << "There shouldn't be an image with this name?";
}

TEST_F(NSImageCacheTest, LookupFoundAndClear) {
  NSImage *first = nsimage_cache::ImageNamed(@"back_Template.pdf");
  EXPECT_TRUE(first != nil)
      << "Failed to find the toolbar image?";
  nsimage_cache::Clear();
  EXPECT_NE(first, nsimage_cache::ImageNamed(@"back_Template.pdf"))
      << "how'd we get the same image after a cache clear?";
}

TEST_F(NSImageCacheTest, AutoTemplating) {
  NSImage *templateImage = nsimage_cache::ImageNamed(@"back_Template.pdf");
  EXPECT_TRUE([templateImage isTemplate] == YES)
      << "Image ending in 'Template' should be marked as being a template";
  NSImage *nonTemplateImage = nsimage_cache::ImageNamed(@"aliasCursor.png");
  EXPECT_FALSE([nonTemplateImage isTemplate] == YES)
      << "Image not ending in 'Template' should not be marked as being a "
         "template";
}

}  // namespace
