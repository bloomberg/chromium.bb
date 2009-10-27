// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/mac_util.h"

#include "base/file_path.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

typedef PlatformTest MacUtilTest;

TEST_F(MacUtilTest, TestFSRef) {
  FSRef ref;
  std::string path("/System/Library");

  ASSERT_TRUE(mac_util::FSRefFromPath(path, &ref));
  EXPECT_EQ(path, mac_util::PathFromFSRef(ref));
}

TEST_F(MacUtilTest, TestLibraryPath) {
  FilePath library_dir = mac_util::GetUserLibraryPath();
  // Make sure the string isn't empty.
  EXPECT_FALSE(library_dir.value().empty());
}

TEST_F(MacUtilTest, TestGrabWindowSnapshot) {
  // Launch a test window so we can take a snapshot.
  [NSApplication sharedApplication];
  NSRect frame = NSMakeRect(0, 0, 400, 400);
  scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:frame
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  [window setBackgroundColor:[NSColor whiteColor]];
  [window makeKeyAndOrderFront:NSApp];

  scoped_ptr<std::vector<unsigned char> > png_representation(
      new std::vector<unsigned char>);
  mac_util::GrabWindowSnapshot(window, png_representation.get());

  // Copy png back into NSData object so we can make sure we grabbed a png.
  scoped_nsobject<NSData> image_data(
      [[NSData alloc] initWithBytes:&(*png_representation)[0]
                             length:png_representation->size()]);
  NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData:image_data.get()];
  EXPECT_TRUE([rep isKindOfClass:[NSBitmapImageRep class]]);
  EXPECT_TRUE(CGImageGetWidth([rep CGImage]) == 400);
  NSColor* color = [rep colorAtX:200 y:200];
  CGFloat red = 0, green = 0, blue = 0, alpha = 0;
  [color getRed:&red green:&green blue:&blue alpha:&alpha];
  EXPECT_GE(red + green + blue, 3.0);
}
