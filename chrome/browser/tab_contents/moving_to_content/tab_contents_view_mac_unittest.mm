// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/tab_contents/moving_to_content/tab_contents_view_mac.h"

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class TabContentsViewCocoaTest : public CocoaTest {
};

TEST_F(TabContentsViewCocoaTest, NonWebDragSourceTest) {
  scoped_nsobject<TabContentsViewCocoa>
      view([[TabContentsViewCocoa alloc] init]);

  // Tests that |draggingSourceOperationMaskForLocal:| returns the expected mask
  // when dragging without a WebDragSource - i.e. when |startDragWithDropData:|
  // hasn't yet been called. Dragging a file from the downloads manager, for
  // example, requires this to work.
  EXPECT_EQ(NSDragOperationCopy,
      [view draggingSourceOperationMaskForLocal:YES]);
  EXPECT_EQ(NSDragOperationCopy,
      [view draggingSourceOperationMaskForLocal:NO]);
}

}  // namespace
