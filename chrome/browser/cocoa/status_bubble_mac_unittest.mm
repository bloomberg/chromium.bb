// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/bubble_view.h"
#import "chrome/browser/cocoa/browser_test_helper.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#import "chrome/browser/cocoa/status_bubble_mac.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/GTM/AppKit/GTMTheme.h"

@interface StatusBubbleMacTestWindowDelegate : NSObject <GTMThemeDelegate>
@end
@implementation StatusBubbleMacTestWindowDelegate
- (GTMTheme*)gtm_themeForWindow:(NSWindow*)window {
  return [[[GTMTheme alloc] init] autorelease];
}

- (NSPoint)gtm_themePatternPhaseForWindow:(NSWindow*)window {
  return NSZeroPoint;
}
@end

// The test delegate records all of the status bubble object's state
// transitions.
@interface StatusBubbleMacTestDelegate : NSObject {
 @private
  std::vector<StatusBubbleMac::StatusBubbleState> states_;
}
- (void)statusBubbleWillEnterState:(StatusBubbleMac::StatusBubbleState)state;
@end
@implementation StatusBubbleMacTestDelegate
- (void)statusBubbleWillEnterState:(StatusBubbleMac::StatusBubbleState)state {
  states_.push_back(state);
}
- (std::vector<StatusBubbleMac::StatusBubbleState>*)states {
  return &states_;
}
@end

// This class implements, for testing purposes, a subclass of |StatusBubbleMac|
// whose |MouseMoved()| method does nothing. (Ideally, we'd have a way of
// controlling the "mouse" location, but the current implementation of
// |StatusBubbleMac| uses |[NSEvent mouseLocation]| directly.) Without this,
// tests can be flaky since results may depend on the mouse location.
class StatusBubbleMacIgnoreMouseMoved : public StatusBubbleMac {
 public:
  StatusBubbleMacIgnoreMouseMoved(NSWindow* parent, id delegate)
      : StatusBubbleMac(parent, delegate) {}

  virtual void MouseMoved(const gfx::Point& location, bool left_content) {}
};

class StatusBubbleMacTest : public CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    NSWindow* window = test_window();
    EXPECT_TRUE(window);
    delegate_.reset([[StatusBubbleMacTestDelegate alloc] init]);
    EXPECT_TRUE(delegate_.get());
    bubble_ = new StatusBubbleMacIgnoreMouseMoved(window, delegate_);
    EXPECT_TRUE(bubble_);

    // Turn off delays and transitions for test mode.  This doesn't just speed
    // things along, it's actually required to get StatusBubbleMac to behave
    // synchronously, because the tests here don't know how to wait for
    // results.  This allows these tests to be much more complete with a
    // minimal loss of coverage and without any heinous rearchitecting.
    bubble_->immediate_ = true;

    EXPECT_FALSE(bubble_->window_);  // lazily creates window
  }

  virtual void TearDown() {
    // Not using a scoped_ptr because bubble must be deleted before calling
    // TearDown to get rid of bubble's window.
    delete bubble_;
    CocoaTest::TearDown();
  }

  bool IsVisible() {
    if (![bubble_->window_ isVisible])
      return false;
    return [bubble_->window_ alphaValue] > 0.0;
  }
  NSString* GetText() {
    return bubble_->status_text_;
  }
  NSString* GetURLText() {
    return bubble_->url_text_;
  }
  NSString* GetBubbleViewText() {
    BubbleView* bubbleView = [bubble_->window_ contentView];
    return [bubbleView content];
  }
  NSWindow* GetWindow() {
    return bubble_->window_;
  }
  NSWindow* GetParent() {
    return bubble_->parent_;
  }
  StatusBubbleMac::StatusBubbleState GetState() {
    return bubble_->state_;
  }
  void SetState(StatusBubbleMac::StatusBubbleState state) {
    bubble_->SetState(state);
  }
  std::vector<StatusBubbleMac::StatusBubbleState>* States() {
    return [delegate_ states];
  }
  StatusBubbleMac::StatusBubbleState StateAt(int index) {
    return (*States())[index];
  }
  BrowserTestHelper browser_helper_;
  scoped_nsobject<StatusBubbleMacTestDelegate> delegate_;
  StatusBubbleMac* bubble_;  // Strong.
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

// Test the "main"/"backup" behavior in StatusBubbleMac::SetText().
TEST_F(StatusBubbleMacTest, SetStatusAndURL) {
  EXPECT_FALSE(IsVisible());
  bubble_->SetStatus(L"Status");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetBubbleViewText() isEqualToString:@"Status"]);
  bubble_->SetURL(GURL("http://www.nytimes.com/"), L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetBubbleViewText() isEqualToString:@"http://www.nytimes.com/"]);
  bubble_->SetURL(GURL(), L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetBubbleViewText() isEqualToString:@"Status"]);
  bubble_->SetStatus(L"");
  EXPECT_FALSE(IsVisible());
  bubble_->SetURL(GURL("http://www.nytimes.com/"), L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetBubbleViewText() isEqualToString:@"http://www.nytimes.com/"]);
  bubble_->SetStatus(L"Status");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetBubbleViewText() isEqualToString:@"Status"]);
  bubble_->SetStatus(L"");
  EXPECT_TRUE(IsVisible());
  EXPECT_TRUE([GetBubbleViewText() isEqualToString:@"http://www.nytimes.com/"]);
  bubble_->SetURL(GURL(), L"");
  EXPECT_FALSE(IsVisible());
}

// Test that the status bubble goes through the correct delay and fade states.
// The delay and fade duration are simulated and not actually experienced
// during the test because StatusBubbleMacTest sets immediate_ mode.
TEST_F(StatusBubbleMacTest, StateTransitions) {
  // First, some sanity

  EXPECT_FALSE(IsVisible());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());

  States()->clear();
  EXPECT_TRUE(States()->empty());

  bubble_->SetStatus(L"");
  EXPECT_FALSE(IsVisible());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_TRUE(States()->empty());  // no change from initial kBubbleHidden state

  // Next, a few ordinary cases

  // Test StartShowing from kBubbleHidden
  bubble_->SetStatus(L"Status");
  EXPECT_TRUE(IsVisible());
  // Check GetState before checking States to make sure that all state
  // transitions have been flushed to States.
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, GetState());
  EXPECT_EQ(3u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleShowingTimer, StateAt(0));
  EXPECT_EQ(StatusBubbleMac::kBubbleShowingFadeIn, StateAt(1));
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, StateAt(2));

  // Test StartShowing from kBubbleShown with the same message
  States()->clear();
  bubble_->SetStatus(L"Status");
  EXPECT_TRUE(IsVisible());
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, GetState());
  EXPECT_TRUE(States()->empty());

  // Test StartShowing from kBubbleShown with a different message
  bubble_->SetStatus(L"New Status");
  EXPECT_TRUE(IsVisible());
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, GetState());
  EXPECT_TRUE(States()->empty());

  // Test StartHiding from kBubbleShown
  bubble_->SetStatus(L"");
  EXPECT_FALSE(IsVisible());
  // Check GetState before checking States to make sure that all state
  // transitions have been flushed to States.
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(3u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidingTimer, StateAt(0));
  EXPECT_EQ(StatusBubbleMac::kBubbleHidingFadeOut, StateAt(1));
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(2));

  // Test StartHiding from kBubbleHidden
  States()->clear();
  bubble_->SetStatus(L"");
  EXPECT_FALSE(IsVisible());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_TRUE(States()->empty());

  // Now, the edge cases

  // Test StartShowing from kBubbleShowingTimer
  bubble_->SetStatus(L"Status");
  SetState(StatusBubbleMac::kBubbleShowingTimer);
  [GetWindow() setAlphaValue:0.0];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(L"Status");
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, GetState());
  EXPECT_EQ(2u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleShowingFadeIn, StateAt(0));
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, StateAt(1));

  // Test StartShowing from kBubbleShowingFadeIn
  bubble_->SetStatus(L"Status");
  SetState(StatusBubbleMac::kBubbleShowingFadeIn);
  [GetWindow() setAlphaValue:0.5];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(L"Status");
  // The actual state values can't be tested in immediate_ mode because
  // the window wasn't actually fading in.  Without immediate_ mode,
  // expect kBubbleShown.
  bubble_->SetStatus(L"");  // Go back to a deterministic state.

  // Test StartShowing from kBubbleHidingTimer
  bubble_->SetStatus(L"");
  SetState(StatusBubbleMac::kBubbleHidingTimer);
  [GetWindow() setAlphaValue:1.0];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(L"Status");
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, StateAt(0));

  // Test StartShowing from kBubbleHidingFadeOut
  bubble_->SetStatus(L"");
  SetState(StatusBubbleMac::kBubbleHidingFadeOut);
  [GetWindow() setAlphaValue:0.5];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(L"Status");
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, GetState());
  EXPECT_EQ(2u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleShowingFadeIn, StateAt(0));
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, StateAt(1));

  // Test StartHiding from kBubbleShowingTimer
  bubble_->SetStatus(L"Status");
  SetState(StatusBubbleMac::kBubbleShowingTimer);
  [GetWindow() setAlphaValue:0.0];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(L"");
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(0));

  // Test StartHiding from kBubbleShowingFadeIn
  bubble_->SetStatus(L"Status");
  SetState(StatusBubbleMac::kBubbleShowingFadeIn);
  [GetWindow() setAlphaValue:0.5];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(L"");
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(2u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidingFadeOut, StateAt(0));
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(1));

  // Test StartHiding from kBubbleHidingTimer
  bubble_->SetStatus(L"");
  SetState(StatusBubbleMac::kBubbleHidingTimer);
  [GetWindow() setAlphaValue:1.0];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(L"");
  // The actual state values can't be tested in immediate_ mode because
  // the timer wasn't actually running.  Without immediate_ mode, expect
  // kBubbleHidingFadeOut and kBubbleHidden.
  bubble_->SetStatus(L"Status");  // Go back to a deterministic state.

  // Test StartHiding from kBubbleHidingFadeOut
  bubble_->SetStatus(L"");
  SetState(StatusBubbleMac::kBubbleHidingFadeOut);
  [GetWindow() setAlphaValue:0.5];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(L"");
  // The actual state values can't be tested in immediate_ mode because
  // the window wasn't actually fading out.  Without immediate_ mode, expect
  // kBubbleHidden.
  bubble_->SetStatus(L"Status");  // Go back to a deterministic state.

  // Test Hide from kBubbleHidden
  bubble_->SetStatus(L"");
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->Hide();
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_TRUE(States()->empty());

  // Test Hide from kBubbleShowingTimer
  bubble_->SetStatus(L"Status");
  SetState(StatusBubbleMac::kBubbleShowingTimer);
  [GetWindow() setAlphaValue:0.0];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->Hide();
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(0));

  // Test Hide from kBubbleShowingFadeIn
  bubble_->SetStatus(L"Status");
  SetState(StatusBubbleMac::kBubbleShowingFadeIn);
  [GetWindow() setAlphaValue:0.5];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->Hide();
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(2u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidingFadeOut, StateAt(0));
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(1));

  // Test Hide from kBubbleShown
  bubble_->SetStatus(L"Status");
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->Hide();
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(0));

  // Test Hide from kBubbleHidingTimer
  bubble_->SetStatus(L"Status");
  SetState(StatusBubbleMac::kBubbleHidingTimer);
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->Hide();
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(0));

  // Test Hide from kBubbleHidingFadeOut
  bubble_->SetStatus(L"Status");
  SetState(StatusBubbleMac::kBubbleHidingFadeOut);
  [GetWindow() setAlphaValue:0.5];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->Hide();
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(0));
}

TEST_F(StatusBubbleMacTest, MouseMove) {
  // TODO(pinkerton): Not sure what to do here since it relies on
  // [NSEvent currentEvent] and the current mouse location.
}

TEST_F(StatusBubbleMacTest, Delete) {
  NSWindow* window = test_window();
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
  NSWindow* window = test_window();
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
