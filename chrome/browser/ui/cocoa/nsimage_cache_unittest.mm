// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/mac/nsimage_cache.h"
#include "base/file_path.h"
#include "base/mac/mac_util.h"
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
    base::mac::SetOverrideAppBundlePath(path);
  }
  virtual ~NSImageCacheTest() {
    base::mac::SetOverrideAppBundle(nil);
  }
};

TEST_F(NSImageCacheTest, LookupFound) {
  EXPECT_TRUE(app::mac::GetCachedImageWithName(@"back_Template.pdf") != nil)
      << "Failed to find the toolbar image?";
}

TEST_F(NSImageCacheTest, LookupCached) {
  EXPECT_EQ(app::mac::GetCachedImageWithName(@"back_Template.pdf"),
            app::mac::GetCachedImageWithName(@"back_Template.pdf"))
    << "Didn't get the same NSImage back?";
}

TEST_F(NSImageCacheTest, LookupMiss) {
  EXPECT_TRUE(app::mac::GetCachedImageWithName(@"should_not.exist") == nil)
      << "There shouldn't be an image with this name?";
}

TEST_F(NSImageCacheTest, LookupFoundAndClear) {
  NSImage *first = app::mac::GetCachedImageWithName(@"back_Template.pdf");
  // Hang on to the first image so that the second one doesn't get allocated
  // in the same location by (bad) luck.
  [[first retain] autorelease];
  EXPECT_TRUE(first != nil)
      << "Failed to find the toolbar image?";
  app::mac::ClearCachedImages();
  NSImage *second = app::mac::GetCachedImageWithName(@"back_Template.pdf");
  EXPECT_TRUE(second != nil)
      << "Failed to find the toolbar image...again?";
  EXPECT_NE(second, first)
      << "how'd we get the same image after a cache clear?";
}

TEST_F(NSImageCacheTest, AutoTemplating) {
  NSImage *templateImage =
      app::mac::GetCachedImageWithName(@"back_Template.pdf");
  EXPECT_TRUE([templateImage isTemplate] == YES)
      << "Image ending in 'Template' should be marked as being a template";
  NSImage *nonTemplateImage =
      app::mac::GetCachedImageWithName(@"aliasCursor.png");
  EXPECT_FALSE([nonTemplateImage isTemplate] == YES)
      << "Image not ending in 'Template' should not be marked as being a "
         "template";
}

}  // namespace
