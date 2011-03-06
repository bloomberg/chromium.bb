// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/bubble_view.h"
#import "chrome/browser/ui/cocoa/browser_test_helper.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/status_bubble_mac.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

// The test delegate records all of the status bubble object's state
// transitions.
@interface StatusBubbleMacTestDelegate : NSObject {
 @private
  NSWindow* window_;  // Weak.
  NSPoint baseFrameOffset_;
  std::vector<StatusBubbleMac::StatusBubbleState> states_;
}
- (id)initWithWindow:(NSWindow*)window;
- (void)forceBaseFrameOffset:(NSPoint)baseFrameOffset;
- (NSRect)statusBubbleBaseFrame;
- (void)statusBubbleWillEnterState:(StatusBubbleMac::StatusBubbleState)state;
@end
@implementation StatusBubbleMacTestDelegate
- (id)initWithWindow:(NSWindow*)window {
  if ((self = [super init])) {
    window_ = window;
    baseFrameOffset_ = NSMakePoint(0, 0);
  }
  return self;
}
- (void)forceBaseFrameOffset:(NSPoint)baseFrameOffset {
  baseFrameOffset_ = baseFrameOffset;
}
- (NSRect)statusBubbleBaseFrame {
  NSView* contentView = [window_ contentView];
  NSRect baseFrame = [contentView convertRect:[contentView frame] toView:nil];
  if (baseFrameOffset_.x > 0 || baseFrameOffset_.y > 0) {
    baseFrame = NSOffsetRect(baseFrame, baseFrameOffset_.x, baseFrameOffset_.y);
    baseFrame.size.width -= baseFrameOffset_.x;
    baseFrame.size.height -= baseFrameOffset_.y;
  }
  return baseFrame;
}
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
    delegate_.reset(
        [[StatusBubbleMacTestDelegate alloc] initWithWindow: window]);
    EXPECT_TRUE(delegate_.get());
    bubble_ = new StatusBubbleMacIgnoreMouseMoved(window, delegate_);
    EXPECT_TRUE(bubble_);

    // Turn off delays and transitions for test mode.  This doesn't just speed
    // things along, it's actually required to get StatusBubbleMac to behave
    // synchronously, because the tests here don't know how to wait for
    // results.  This allows these tests to be much more complete with a
    // minimal loss of coverage and without any heinous rearchitecting.
    bubble_->immediate_ = true;

    EXPECT_TRUE(bubble_->window_);  // immediately creates window
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

TEST_F(StatusBubbleMacTest, SetStatus) {
  bubble_->SetStatus(string16());
  bubble_->SetStatus(UTF8ToUTF16("This is a test"));
  EXPECT_NSEQ(@"This is a test", GetText());
  EXPECT_TRUE(IsVisible());

  // Set the status to the exact same thing again
  bubble_->SetStatus(UTF8ToUTF16("This is a test"));
  EXPECT_NSEQ(@"This is a test", GetText());

  // Hide it
  bubble_->SetStatus(string16());
  EXPECT_FALSE(IsVisible());
}

TEST_F(StatusBubbleMacTest, SetURL) {
  bubble_->SetURL(GURL(), string16());
  EXPECT_FALSE(IsVisible());
  bubble_->SetURL(GURL("bad url"), string16());
  EXPECT_FALSE(IsVisible());
  bubble_->SetURL(GURL("http://"), string16());
  EXPECT_TRUE(IsVisible());
  EXPECT_NSEQ(@"http:", GetURLText());
  bubble_->SetURL(GURL("about:blank"), string16());
  EXPECT_TRUE(IsVisible());
  EXPECT_NSEQ(@"about:blank", GetURLText());
  bubble_->SetURL(GURL("foopy://"), string16());
  EXPECT_TRUE(IsVisible());
  EXPECT_NSEQ(@"foopy://", GetURLText());
  bubble_->SetURL(GURL("http://www.cnn.com"), string16());
  EXPECT_TRUE(IsVisible());
  EXPECT_NSEQ(@"www.cnn.com", GetURLText());
}

// Test hiding bubble that's already hidden.
TEST_F(StatusBubbleMacTest, Hides) {
  bubble_->SetStatus(UTF8ToUTF16("Showing"));
  EXPECT_TRUE(IsVisible());
  bubble_->Hide();
  EXPECT_FALSE(IsVisible());
  bubble_->Hide();
  EXPECT_FALSE(IsVisible());
}

// Test the "main"/"backup" behavior in StatusBubbleMac::SetText().
TEST_F(StatusBubbleMacTest, SetStatusAndURL) {
  EXPECT_FALSE(IsVisible());
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  EXPECT_TRUE(IsVisible());
  EXPECT_NSEQ(@"Status", GetBubbleViewText());
  bubble_->SetURL(GURL("http://www.nytimes.com"), string16());
  EXPECT_TRUE(IsVisible());
  EXPECT_NSEQ(@"www.nytimes.com", GetBubbleViewText());
  bubble_->SetURL(GURL(), string16());
  EXPECT_TRUE(IsVisible());
  EXPECT_NSEQ(@"Status", GetBubbleViewText());
  bubble_->SetStatus(string16());
  EXPECT_FALSE(IsVisible());
  bubble_->SetURL(GURL("http://www.nytimes.com"), string16());
  EXPECT_TRUE(IsVisible());
  EXPECT_NSEQ(@"www.nytimes.com", GetBubbleViewText());
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  EXPECT_TRUE(IsVisible());
  EXPECT_NSEQ(@"Status", GetBubbleViewText());
  bubble_->SetStatus(string16());
  EXPECT_TRUE(IsVisible());
  EXPECT_NSEQ(@"www.nytimes.com", GetBubbleViewText());
  bubble_->SetURL(GURL(), string16());
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

  bubble_->SetStatus(string16());
  EXPECT_FALSE(IsVisible());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_TRUE(States()->empty());  // no change from initial kBubbleHidden state

  // Next, a few ordinary cases

  // Test StartShowing from kBubbleHidden
  bubble_->SetStatus(UTF8ToUTF16("Status"));
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
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  EXPECT_TRUE(IsVisible());
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, GetState());
  EXPECT_TRUE(States()->empty());

  // Test StartShowing from kBubbleShown with a different message
  bubble_->SetStatus(UTF8ToUTF16("New Status"));
  EXPECT_TRUE(IsVisible());
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, GetState());
  EXPECT_TRUE(States()->empty());

  // Test StartHiding from kBubbleShown
  bubble_->SetStatus(string16());
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
  bubble_->SetStatus(string16());
  EXPECT_FALSE(IsVisible());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_TRUE(States()->empty());

  // Now, the edge cases

  // Test StartShowing from kBubbleShowingTimer
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  SetState(StatusBubbleMac::kBubbleShowingTimer);
  [GetWindow() setAlphaValue:0.0];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, GetState());
  EXPECT_EQ(2u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleShowingFadeIn, StateAt(0));
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, StateAt(1));

  // Test StartShowing from kBubbleShowingFadeIn
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  SetState(StatusBubbleMac::kBubbleShowingFadeIn);
  [GetWindow() setAlphaValue:0.5];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  // The actual state values can't be tested in immediate_ mode because
  // the window wasn't actually fading in.  Without immediate_ mode,
  // expect kBubbleShown.
  bubble_->SetStatus(string16());  // Go back to a deterministic state.

  // Test StartShowing from kBubbleHidingTimer
  bubble_->SetStatus(string16());
  SetState(StatusBubbleMac::kBubbleHidingTimer);
  [GetWindow() setAlphaValue:1.0];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, StateAt(0));

  // Test StartShowing from kBubbleHidingFadeOut
  bubble_->SetStatus(string16());
  SetState(StatusBubbleMac::kBubbleHidingFadeOut);
  [GetWindow() setAlphaValue:0.5];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, GetState());
  EXPECT_EQ(2u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleShowingFadeIn, StateAt(0));
  EXPECT_EQ(StatusBubbleMac::kBubbleShown, StateAt(1));

  // Test StartHiding from kBubbleShowingTimer
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  SetState(StatusBubbleMac::kBubbleShowingTimer);
  [GetWindow() setAlphaValue:0.0];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(string16());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(0));

  // Test StartHiding from kBubbleShowingFadeIn
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  SetState(StatusBubbleMac::kBubbleShowingFadeIn);
  [GetWindow() setAlphaValue:0.5];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(string16());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(2u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidingFadeOut, StateAt(0));
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(1));

  // Test StartHiding from kBubbleHidingTimer
  bubble_->SetStatus(string16());
  SetState(StatusBubbleMac::kBubbleHidingTimer);
  [GetWindow() setAlphaValue:1.0];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(string16());
  // The actual state values can't be tested in immediate_ mode because
  // the timer wasn't actually running.  Without immediate_ mode, expect
  // kBubbleHidingFadeOut and kBubbleHidden.
  // Go back to a deterministic state.
  bubble_->SetStatus(UTF8ToUTF16("Status"));

  // Test StartHiding from kBubbleHidingFadeOut
  bubble_->SetStatus(string16());
  SetState(StatusBubbleMac::kBubbleHidingFadeOut);
  [GetWindow() setAlphaValue:0.5];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->SetStatus(string16());
  // The actual state values can't be tested in immediate_ mode because
  // the window wasn't actually fading out.  Without immediate_ mode, expect
  // kBubbleHidden.
  // Go back to a deterministic state.
  bubble_->SetStatus(UTF8ToUTF16("Status"));

  // Test Hide from kBubbleHidden
  bubble_->SetStatus(string16());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->Hide();
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_TRUE(States()->empty());

  // Test Hide from kBubbleShowingTimer
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  SetState(StatusBubbleMac::kBubbleShowingTimer);
  [GetWindow() setAlphaValue:0.0];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->Hide();
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(0));

  // Test Hide from kBubbleShowingFadeIn
  bubble_->SetStatus(UTF8ToUTF16("Status"));
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
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->Hide();
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(0));

  // Test Hide from kBubbleHidingTimer
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  SetState(StatusBubbleMac::kBubbleHidingTimer);
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->Hide();
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(0));

  // Test Hide from kBubbleHidingFadeOut
  bubble_->SetStatus(UTF8ToUTF16("Status"));
  SetState(StatusBubbleMac::kBubbleHidingFadeOut);
  [GetWindow() setAlphaValue:0.5];
  States()->clear();
  EXPECT_TRUE(States()->empty());
  bubble_->Hide();
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, GetState());
  EXPECT_EQ(1u, States()->size());
  EXPECT_EQ(StatusBubbleMac::kBubbleHidden, StateAt(0));
}

TEST_F(StatusBubbleMacTest, Delete) {
  NSWindow* window = test_window();
  // Create and delete immediately.
  StatusBubbleMac* bubble = new StatusBubbleMac(window, nil);
  delete bubble;

  // Create then delete while visible.
  bubble = new StatusBubbleMac(window, nil);
  bubble->SetStatus(UTF8ToUTF16("showing"));
  delete bubble;
}

TEST_F(StatusBubbleMacTest, UpdateSizeAndPosition) {
  // Test |UpdateSizeAndPosition()| when status bubble does not exist (shouldn't
  // crash; shouldn't create window).
  EXPECT_TRUE(GetWindow());
  bubble_->UpdateSizeAndPosition();
  EXPECT_TRUE(GetWindow());

  // Create a status bubble (with contents) and call resize (without actually
  // resizing); the frame size shouldn't change.
  bubble_->SetStatus(UTF8ToUTF16("UpdateSizeAndPosition test"));
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

TEST_F(StatusBubbleMacTest, MovingWindowUpdatesPosition) {
  NSWindow* window = test_window();

  // Show the bubble and make sure it has the same origin as |window|.
  bubble_->SetStatus(UTF8ToUTF16("Showing"));
  NSWindow* child = GetWindow();
  EXPECT_TRUE(NSEqualPoints([window frame].origin, [child frame].origin));

  // Hide the bubble, move the window, and show it again.
  bubble_->Hide();
  NSRect frame = [window frame];
  frame.origin.x += 50;
  [window setFrame:frame display:YES];
  bubble_->SetStatus(UTF8ToUTF16("Reshowing"));

  // The bubble should reattach in the correct location.
  child = GetWindow();
  EXPECT_TRUE(NSEqualPoints([window frame].origin, [child frame].origin));
}

TEST_F(StatusBubbleMacTest, StatuBubbleRespectsBaseFrameLimits) {
  NSWindow* window = test_window();

  // Show the bubble and make sure it has the same origin as |window|.
  bubble_->SetStatus(UTF8ToUTF16("Showing"));
  NSWindow* child = GetWindow();
  EXPECT_TRUE(NSEqualPoints([window frame].origin, [child frame].origin));

  // Hide the bubble, change base frame offset, and show it again.
  bubble_->Hide();

  NSPoint baseFrameOffset = NSMakePoint(0, [window frame].size.height / 3);
  EXPECT_GT(baseFrameOffset.y, 0);
  [delegate_ forceBaseFrameOffset:baseFrameOffset];

  bubble_->SetStatus(UTF8ToUTF16("Reshowing"));

  // The bubble should reattach in the correct location.
  child = GetWindow();
  NSPoint expectedOrigin = [window frame].origin;
  expectedOrigin.x += baseFrameOffset.x;
  expectedOrigin.y += baseFrameOffset.y;
  EXPECT_TRUE(NSEqualPoints(expectedOrigin, [child frame].origin));
}

TEST_F(StatusBubbleMacTest, ExpandBubble) {
  NSWindow* window = test_window();
  ASSERT_TRUE(window);
  NSRect window_frame = [window frame];
  window_frame.size.width = 600.0;
  [window setFrame:window_frame display:YES];

  // Check basic expansion
  bubble_->SetStatus(UTF8ToUTF16("Showing"));
  EXPECT_TRUE(IsVisible());
  bubble_->SetURL(GURL("http://www.battersbox.com/peter_paul_and_mary.html"),
                  string16());
  EXPECT_TRUE([GetURLText() hasSuffix:@"\u2026"]);
  bubble_->ExpandBubble();
  EXPECT_TRUE(IsVisible());
  EXPECT_NSEQ(@"www.battersbox.com/peter_paul_and_mary.html", GetURLText());
  bubble_->Hide();

  // Make sure bubble resets after hide.
  bubble_->SetStatus(UTF8ToUTF16("Showing"));
  bubble_->SetURL(GURL("http://www.snickersnee.com/pioneer_fishstix.html"),
                  string16());
  EXPECT_TRUE([GetURLText() hasSuffix:@"\u2026"]);
  // ...and that it expands again properly.
  bubble_->ExpandBubble();
  EXPECT_NSEQ(@"www.snickersnee.com/pioneer_fishstix.html", GetURLText());
  // ...again, again!
  bubble_->SetURL(GURL("http://www.battersbox.com/peter_paul_and_mary.html"),
                  string16());
  bubble_->ExpandBubble();
  EXPECT_NSEQ(@"www.battersbox.com/peter_paul_and_mary.html", GetURLText());
  bubble_->Hide();

  window_frame = [window frame];
  window_frame.size.width = 300.0;
  [window setFrame:window_frame display:YES];

  // Very long URL's will be cut off even in the expanded state.
  bubble_->SetStatus(UTF8ToUTF16("Showing"));
  const char veryLongUrl[] =
      "http://www.diewahrscheinlichlaengstepralinederwelt.com/duuuuplo.html";
  bubble_->SetURL(GURL(veryLongUrl), string16());
  EXPECT_TRUE([GetURLText() hasSuffix:@"\u2026"]);
  bubble_->ExpandBubble();
  EXPECT_TRUE([GetURLText() hasSuffix:@"\u2026"]);
}


