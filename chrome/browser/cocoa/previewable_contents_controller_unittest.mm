// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_test_helper.h"
#include "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/previewable_contents_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface PreviewableContentsController (ExposedForTesting)
- (NSButton*)closeButton;
@end

@implementation PreviewableContentsController (ExposedForTesting)
- (NSButton*)closeButton {
  return closeButton_;
}
@end

namespace {

class PreviewableContentsControllerTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    controller_.reset([[PreviewableContentsController alloc] init]);
    [[test_window() contentView] addSubview:[controller_ view]];
  }

  scoped_nsobject<PreviewableContentsController> controller_;
};

TEST_VIEW(PreviewableContentsControllerTest, [controller_ view])

// Adds the view to a window and displays it.
TEST_F(PreviewableContentsControllerTest, TestImagesLoadedProperly) {
  EXPECT_TRUE([[[controller_ closeButton] image] isValid]);
}

// TODO(rohitrao): Test showing and hiding the preview.  This may require
// changing the interface to take in a TabContentsView* instead of a
// TabContents*.

}  // namespace

