// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/mac_util.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_cftyperef.h"
#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace mac_util {

namespace {

typedef PlatformTest MacUtilTest;

TEST_F(MacUtilTest, TestFSRef) {
  FSRef ref;
  std::string path("/System/Library");

  ASSERT_TRUE(FSRefFromPath(path, &ref));
  EXPECT_EQ(path, PathFromFSRef(ref));
}

TEST_F(MacUtilTest, TestLibraryPath) {
  FilePath library_dir = GetUserLibraryPath();
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
  GrabWindowSnapshot(window, png_representation.get());

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

TEST_F(MacUtilTest, TestGetAppBundlePath) {
  FilePath out;

  // Make sure it doesn't crash.
  out = GetAppBundlePath(FilePath());
  EXPECT_TRUE(out.empty());

  // Some more invalid inputs.
  const char* invalid_inputs[] = {
    "/", "/foo", "foo", "/foo/bar.", "foo/bar.", "/foo/bar./bazquux",
    "foo/bar./bazquux", "foo/.app", "//foo",
  };
  for (size_t i = 0; i < arraysize(invalid_inputs); i++) {
    out = GetAppBundlePath(FilePath(invalid_inputs[i]));
    EXPECT_TRUE(out.empty()) << "loop: " << i;
  }

  // Some valid inputs; this and |expected_outputs| should be in sync.
  struct {
    const char *in;
    const char *expected_out;
  } valid_inputs[] = {
    { "FooBar.app/", "FooBar.app" },
    { "/FooBar.app", "/FooBar.app" },
    { "/FooBar.app/", "/FooBar.app" },
    { "//FooBar.app", "//FooBar.app" },
    { "/Foo/Bar.app", "/Foo/Bar.app" },
    { "/Foo/Bar.app/", "/Foo/Bar.app" },
    { "/F/B.app", "/F/B.app" },
    { "/F/B.app/", "/F/B.app" },
    { "/Foo/Bar.app/baz", "/Foo/Bar.app" },
    { "/Foo/Bar.app/baz/", "/Foo/Bar.app" },
    { "/Foo/Bar.app/baz/quux.app/quuux", "/Foo/Bar.app" },
    { "/Applications/Google Foo.app/bar/Foo Helper.app/quux/Foo Helper",
        "/Applications/Google Foo.app" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(valid_inputs); i++) {
    out = GetAppBundlePath(FilePath(valid_inputs[i].in));
    EXPECT_FALSE(out.empty()) << "loop: " << i;
    EXPECT_STREQ(valid_inputs[i].expected_out,
        out.value().c_str()) << "loop: " << i;
  }
}

TEST_F(MacUtilTest, TestExcludeFileFromBackups) {
  NSString* homeDirectory = NSHomeDirectory();
  NSString* dummyFilePath =
      [homeDirectory stringByAppendingPathComponent:@"DummyFile"];
  const char* dummy_file_path = [dummyFilePath fileSystemRepresentation];
  ASSERT_TRUE(dummy_file_path);
  FilePath file_path(dummy_file_path);
  // It is not actually necessary to have a physical file in order to
  // set its exclusion property.
  NSURL* fileURL = [NSURL URLWithString:dummyFilePath];
  // Reset the exclusion in case it was set previously.
  SetFileBackupExclusion(file_path, false);
  Boolean excludeByPath;
  // Initial state should be non-excluded.
  EXPECT_FALSE(CSBackupIsItemExcluded((CFURLRef)fileURL, &excludeByPath));
  // Exclude the file.
  EXPECT_TRUE(SetFileBackupExclusion(file_path, true));
  EXPECT_TRUE(CSBackupIsItemExcluded((CFURLRef)fileURL, &excludeByPath));
  // Un-exclude the file.
  EXPECT_TRUE(SetFileBackupExclusion(file_path, false));
  EXPECT_FALSE(CSBackupIsItemExcluded((CFURLRef)fileURL, &excludeByPath));
}

TEST_F(MacUtilTest, TestGetValueFromDictionary) {
  scoped_cftyperef<CFMutableDictionaryRef> dict(
      CFDictionaryCreateMutable(0, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(dict.get(), CFSTR("key"), CFSTR("value"));

  EXPECT_TRUE(CFEqual(CFSTR("value"),
                      GetValueFromDictionary(
                          dict, CFSTR("key"), CFStringGetTypeID())));
  EXPECT_FALSE(GetValueFromDictionary(dict, CFSTR("key"), CFNumberGetTypeID()));
  EXPECT_FALSE(GetValueFromDictionary(
                   dict, CFSTR("no-exist"), CFStringGetTypeID()));
}

}  // namespace

}  // namespace mac_util
