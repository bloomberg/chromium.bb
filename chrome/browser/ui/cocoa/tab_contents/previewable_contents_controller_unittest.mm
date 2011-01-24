// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/tab_contents/previewable_contents_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

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

// TODO(rohitrao): Test showing and hiding the preview.  This may require
// changing the interface to take in a TabContentsView* instead of a
// TabContents*.

}  // namespace

