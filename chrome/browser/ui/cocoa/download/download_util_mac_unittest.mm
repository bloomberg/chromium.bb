// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Download utility test for Mac OS X.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/download/download_util_mac.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace {

class DownloadUtilMacTest : public CocoaTest {
 public:
  DownloadUtilMacTest() {
    pasteboard_ = [NSPasteboard pasteboardWithUniqueName];
  }

  virtual ~DownloadUtilMacTest() {
    [pasteboard_ releaseGlobally];
  }

  NSPasteboard* pasteboard() { return pasteboard_; }

 private:
  NSPasteboard* pasteboard_;
};

// Ensure adding files to the pasteboard methods works as expected.
TEST_F(DownloadUtilMacTest, AddFileToPasteboardTest) {
  // Get a download test file for addition to the pasteboard.
  base::FilePath testPath;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &testPath));
  base::FilePath testFile(FILE_PATH_LITERAL("download-test1.lib"));
  testPath = testPath.Append(testFile);

  // Add a test file to the pasteboard via the download_util method.
  download_util::AddFileToPasteboard(pasteboard(), testPath);

  // Test to see that the object type for dragging files is available.
  NSArray* types =  [NSArray arrayWithObject:NSFilenamesPboardType];
  NSString* available = [pasteboard() availableTypeFromArray:types];
  EXPECT_TRUE(available != nil);

  // Ensure the path is what we expect.
  NSArray* files = [pasteboard() propertyListForType:NSFilenamesPboardType];
  ASSERT_TRUE(files != nil);
  NSString* expectedPath = [files objectAtIndex:0];
  NSString* realPath = base::SysUTF8ToNSString(testPath.value());
  EXPECT_NSEQ(expectedPath, realPath);
}

}  // namespace
