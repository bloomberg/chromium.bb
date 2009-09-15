// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/focus_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class FocusTrackerTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    viewA_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
    viewB_.reset([[NSView alloc] initWithFrame:NSZeroRect]);
    [helper_.contentView() addSubview:viewA_.get()];
    [helper_.contentView() addSubview:viewB_.get()];
  }

 protected:
  CocoaTestHelper helper_;
  scoped_nsobject<NSView> viewA_;
  scoped_nsobject<NSView> viewB_;
};

TEST_F(FocusTrackerTest, SaveRestore) {
  NSWindow* window = helper_.window();
  ASSERT_TRUE([window makeFirstResponder:viewA_.get()]);
  FocusTracker* tracker =
    [[[FocusTracker alloc] initWithWindow:window] autorelease];

  // Give focus to |viewB_|, then try and restore it to view1.
  ASSERT_TRUE([window makeFirstResponder:viewB_.get()]);
  EXPECT_TRUE([tracker restoreFocusInWindow:window]);
  EXPECT_EQ(viewA_.get(), [window firstResponder]);
}

TEST_F(FocusTrackerTest, SaveRestoreWithTextView) {
  // Valgrind will complain if the text field has zero size.
  NSRect frame = NSMakeRect(0, 0, 100, 20);
  NSWindow* window = helper_.window();
  NSTextField* text =
    [[[NSTextField alloc] initWithFrame:frame] autorelease];
  [helper_.contentView() addSubview:text];

  ASSERT_TRUE([window makeFirstResponder:text]);
  FocusTracker* tracker =
    [[[FocusTracker alloc] initWithWindow:window] autorelease];

  // Give focus to |viewB_|, then try and restore it to the text field.
  ASSERT_TRUE([window makeFirstResponder:viewB_.get()]);
  EXPECT_TRUE([tracker restoreFocusInWindow:window]);
  EXPECT_TRUE([[window firstResponder] isKindOfClass:[NSTextView class]]);
}

TEST_F(FocusTrackerTest, DontRestoreToViewNotInWindow) {
  NSWindow* window = helper_.window();
  NSView* view3 = [[[NSView alloc] initWithFrame:NSZeroRect] autorelease];
  [helper_.contentView() addSubview:view3];

  ASSERT_TRUE([window makeFirstResponder:view3]);
  FocusTracker* tracker =
    [[[FocusTracker alloc] initWithWindow:window] autorelease];

  // Give focus to |viewB_|, then remove view3 from the hierarchy and try
  // to restore focus.  The restore should fail.
  ASSERT_TRUE([window makeFirstResponder:viewB_.get()]);
  [view3 removeFromSuperview];
  EXPECT_FALSE([tracker restoreFocusInWindow:window]);
}

TEST_F(FocusTrackerTest, DontRestoreFocusToViewInDifferentWindow) {
  NSWindow* window = helper_.window();
  ASSERT_TRUE([window makeFirstResponder:viewA_.get()]);
  FocusTracker* tracker =
    [[[FocusTracker alloc] initWithWindow:window] autorelease];

  // Give focus to |viewB_|, then try and restore focus in a different
  // window.  It is ok to pass a nil NSWindow here because we only use
  // it for direct comparison.
  ASSERT_TRUE([window makeFirstResponder:viewB_.get()]);
  EXPECT_FALSE([tracker restoreFocusInWindow:nil]);
}


}  // namespace
