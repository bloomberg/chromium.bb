// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/download/download_item_button.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// Make sure nothing leaks.
TEST(DownloadItemButtonTest, Create) {
  base::scoped_nsobject<DownloadItemButton> button;
  button.reset([[DownloadItemButton alloc]
      initWithFrame:NSMakeRect(0,0,500,500)]);

  // Test setter
  base::FilePath path("foo");
  [button.get() setDownload:path];
  EXPECT_EQ(path.value(), [button.get() download].value());
}
