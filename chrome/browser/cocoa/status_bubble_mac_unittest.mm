// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "chrome/browser/cocoa/status_bubble_mac.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

@interface StatusBubbleMacTestWindowDelegate : NSObject <GTMThemeDelegate>;
@end
@implementation StatusBubbleMacTestWindowDelegate
- (GTMTheme*)gtm_themeForWindow:(NSWindow*)window {
  return [[[GTMTheme alloc] init] autorelease];
}

- (NSPoint)gtm_themePatternPhaseForWindow:(NSWindow*)window {
  return NSZeroPoint;
}
@end

class StatusBubbleMacTest : public PlatformTest {
 public:
  StatusBubbleMacTest() {
    NSWindow* window = cocoa_helper_.window();
    bubble_.reset(new StatusBubbleMac(window, nil));
    EXPECT_TRUE(bubble_.get());
    EXPECT_FALSE(bubble_->window_);  // lazily creates window
  }

  bool IsVisible() {
    return [bubble_->window_ isVisible] ? true: false;
  }
  NSString* GetText() {
    return bubble_->status_text_;
  }
  NSString* GetURLText() {
    return bubble_->url_text_;
  }
  NSWindow* GetWindow() {
    return bubble_->window_;
  }
  NSWindow* GetParent() {
    return bubble_->parent_;
  }
  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_ptr<StatusBubbleMac> bubble_;
};

TEST_F(StatusBubbleMacTest, Theme) {
  bubble_->SetStatus(L"Theme test");  // Creates the window
  [GetParent() setDelegate:
      [[[StatusBubbleMacTestWindowDelegate alloc] init] autorelease]];
  EXPECT_TRUE([GetParent() gtm_theme] != nil);
  EXPECT_TRUE([[GetWindow() contentView] gtm_theme] != nil);
}

TEST_F(StatusBubbleMacTest, SetStatus) {
  bubble_->SetStatus(L"");
  bubble_->SetStatus(L"This is a test");
  EXPECT_TRUE([GetText() isEqualToString:@"This is a test"]);
  EXPECT_TRUE(IsVisible());

  // Set the status to the exact same thing again
  bubble_->SetStatus(L"This is a test");
  EXPECT_TRUE([GetText() isEqualToString:@"This is a test"]);

  // Hide it
  bubble_->SetStatus(L"");
  EXPECT_FALSE(IsVisible());
  EXPECT_FALSE(GetText());
}

TEST_F(StatusBubbleMacTest, SetURL) {
  bubble_->SetURL(GURL(), L"");
  EXPECT_FALSE(IsVisible());
  bubble_->SetURL(GURL("bad url"), L"");
  EXPECT_FALSE(IsVisible());
  bubble_->SetURL(GURL("http://"), L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetURLText() isEqualToString:@"http:"]);
  bubble_->SetURL(GURL("about:blank"), L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetURLText() isEqualToString:@"about:blank"]);
  bubble_->SetURL(GURL("foopy://"), L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetURLText() isEqualToString:@"foopy:"]);
  bubble_->SetURL(GURL("http://www.cnn.com"), L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetURLText() isEqualToString:@"http://www.cnn.com/"]);
}

// Test hiding bubble that's already hidden.
TEST_F(StatusBubbleMacTest, Hides) {
  bubble_->SetStatus(L"Showing");
  EXPECT_TRUE(IsVisible());
  bubble_->Hide();
  EXPECT_FALSE(IsVisible());
  bubble_->Hide();
  EXPECT_FALSE(IsVisible());
}

TEST_F(StatusBubbleMacTest, MouseMove) {
  // TODO(pinkerton): Not sure what to do here since it relies on
  // [NSEvent currentEvent] and the current mouse location.
}

TEST_F(StatusBubbleMacTest, Delete) {
  NSWindow* window = cocoa_helper_.window();
  // Create and delete immediately.
  StatusBubbleMac* bubble = new StatusBubbleMac(window, nil);
  delete bubble;

  // Create then delete while visible.
  bubble = new StatusBubbleMac(window, nil);
  bubble->SetStatus(L"showing");
  delete bubble;
}

TEST_F(StatusBubbleMacTest, UpdateSizeAndPosition) {
  // Test |UpdateSizeAndPosition()| when status bubble does not exist (shouldn't
  // crash; shouldn't create window).
  EXPECT_FALSE(GetWindow());
  bubble_->UpdateSizeAndPosition();
  EXPECT_FALSE(GetWindow());

  // Create a status bubble (with contents) and call resize (without actually
  // resizing); the frame size shouldn't change.
  bubble_->SetStatus(L"UpdateSizeAndPosition test");
  ASSERT_TRUE(GetWindow());
  NSRect rect_before = [GetWindow() frame];
  bubble_->UpdateSizeAndPosition();
  NSRect rect_after = [GetWindow() frame];
  EXPECT_TRUE(NSEqualRects(rect_before, rect_after));

  // Move the window and call resize; only the origin should change.
  NSWindow* window = cocoa_helper_.window();
  ASSERT_TRUE(window);
  NSRect frame = [window frame];
  rect_before = [GetWindow() frame];
  frame.origin.x += 10.0;  // (fairly arbitrary nonzero value)
  frame.origin.y += 10.0;  // (fairly arbitrary nonzero value)
  [window setFrame:frame display:YES];
  bubble_->UpdateSizeAndPosition();
  rect_after = [GetWindow() frame];
  EXPECT_NE(rect_before.origin.x, rect_after.origin.x);
  EXPECT_NE(rect_before.origin.y, rect_after.origin.y);
  EXPECT_EQ(rect_before.size.width, rect_after.size.width);
  EXPECT_EQ(rect_before.size.height, rect_after.size.height);

  // Resize the window (without moving). The origin shouldn't change. The width
  // should change (in the current implementation), but not the height.
  frame = [window frame];
  rect_before = [GetWindow() frame];
  frame.size.width += 50.0;   // (fairly arbitrary nonzero value)
  frame.size.height += 50.0;  // (fairly arbitrary nonzero value)
  [window setFrame:frame display:YES];
  bubble_->UpdateSizeAndPosition();
  rect_after = [GetWindow() frame];
  EXPECT_EQ(rect_before.origin.x, rect_after.origin.x);
  EXPECT_EQ(rect_before.origin.y, rect_after.origin.y);
  EXPECT_NE(rect_before.size.width, rect_after.size.width);
  EXPECT_EQ(rect_before.size.height, rect_after.size.height);
}
