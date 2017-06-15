// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen_controller.h"

#import "ios/web/public/test/fakes/test_web_view_content_view.h"
#import "ios/web/public/web_state/ui/crw_web_view_content_view.h"
#import "ios/web/public/web_state/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/ui/crw_web_view_proxy_impl.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/ocmock_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

CGFloat kContentHeight = 5000.0;
CGFloat kHeaderHeight = 42.0;
}

@interface MockFullScreenControllerDelegate
    : NSObject<FullScreenControllerDelegate>
@property(nonatomic, readonly) float currentPosition;
@property(nonatomic, assign) BOOL fakeIsTabWithIDCurrentFlag;
@end

@implementation MockFullScreenControllerDelegate
@synthesize currentPosition = currentPosition_;
@synthesize fakeIsTabWithIDCurrentFlag = fakeIsTabWithIDCurrentFlag_;

- (id)init {
  if ((self = [super init])) {
    currentPosition_ = 0.0;
    fakeIsTabWithIDCurrentFlag_ = YES;
  }
  return self;
}

- (void)fullScreenController:(FullScreenController*)fullscreenController
    drawHeaderViewFromOffset:(CGFloat)headerOffset
                     animate:(BOOL)animate {
  currentPosition_ = headerOffset;
}

- (void)fullScreenController:(FullScreenController*)fullScreenController
    drawHeaderViewFromOffset:(CGFloat)headerOffset
              onWebViewProxy:(id<CRWWebViewProxy>)webViewProxy
     changeTopContentPadding:(BOOL)changeTopContentPadding
           scrollingToOffset:(CGFloat)contentOffset {
  currentPosition_ = headerOffset;
  webViewProxy.scrollViewProxy.contentOffset = CGPointMake(0.0f, contentOffset);
}

- (BOOL)isTabWithIDCurrent:(NSString*)sessionID {
  return [self fakeIsTabWithIDCurrentFlag];
}

- (CGFloat)currentHeaderOffset {
  return currentPosition_;
}

- (CGFloat)headerHeight {
  return kHeaderHeight;
}

@end

namespace {

NSString* const kFakeSessionId = @"fake-session-id";

class FullscreenControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    CGRect frame = CGRectMake(0.0, 0.0, 300.0, 900.0);
    PlatformTest::SetUp();

    scrollview_ = [[UIScrollView alloc] initWithFrame:frame];
    scrollview_.contentInset = UIEdgeInsetsZero;
    [GetWindow() addSubview:scrollview_];

    CGRect contentSize = CGRectMake(0.0, 0.0, frame.size.width, kContentHeight);
    scrollview_.contentSize = contentSize.size;
    mockWebController_ =
        [OCMockObject niceMockForClass:[CRWWebController class]];
    mockDelegate_ = [[MockFullScreenControllerDelegate alloc] init];
    mockWebView_ = [[UIView alloc] init];
    mockContentView_ =
        [[TestWebViewContentView alloc] initWithMockWebView:mockWebView_
                                                 scrollView:scrollview_];
    webViewProxy_ =
        [[CRWWebViewProxyImpl alloc] initWithWebController:mockWebController_];
    [webViewProxy_ setContentView:mockContentView_];
    webViewScrollViewProxy_ = [webViewProxy_ scrollViewProxy];
    controller_ =
        [[FullScreenController alloc] initWithDelegate:mockDelegate_
                                     navigationManager:NULL
                                             sessionID:kFakeSessionId];
    DCHECK(controller_);
    [webViewScrollViewProxy_ addObserver:controller_];
    // Simulate a CRWWebControllerObserver callback.
    [controller_ setWebViewProxy:webViewProxy_ controller:mockWebController_];
    [controller_ moveHeaderToRestingPosition:YES];

    UIView* awesome_view = [[UIView alloc] initWithFrame:contentSize];
    [scrollview_ addSubview:awesome_view];

    EXPECT_TRUE(IsHeaderVisible());
    EXPECT_EQ(0.0f, webViewScrollViewProxy_.contentOffset.y);
  }

  void TearDown() override {
    [webViewScrollViewProxy_ removeObserver:controller_];
    [webViewScrollViewProxy_ setScrollView:nil];
    [scrollview_ removeFromSuperview];
    PlatformTest::TearDown();
  }

  void MoveMiddle() {
    // Move somewhere in the middle.
    CGFloat middle_point = kContentHeight / 2.0;
    webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, middle_point);
  }

  void MoveTop() {
    webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, -kHeaderHeight);
  }

  void MoveMiddleAndHide() {
    MoveMiddle();
    [controller_ moveHeaderToRestingPosition:NO];
    EXPECT_TRUE(IsHeaderHidden());
  }

  UIWindow* GetWindow() {
    UIWindow* window = [[UIApplication sharedApplication] keyWindow];
    if (!window)
      window = [[[UIApplication sharedApplication] windows] lastObject];
    EXPECT_TRUE(window != nil);
    return window;
  }

  bool IsHeaderVisible() { return [mockDelegate_ currentPosition] == 0.0; }

  bool IsHeaderHidden() {
    return [mockDelegate_ currentPosition] == kHeaderHeight;
  }

  // Adds |view| as a sub view to the underlying |scrollview_|.
  void AddSubViewToScrollView(UIView* view) { [scrollview_ addSubview:view]; }

  FullScreenController* controller_;
  MockFullScreenControllerDelegate* mockDelegate_;
  CRWWebViewScrollViewProxy* webViewScrollViewProxy_;
  id mockWebView_;
  id mockWebController_;
  TestWebViewContentView* mockContentView_;
  CRWWebViewProxyImpl* webViewProxy_;

 private:
  UIScrollView* scrollview_;
};

#pragma mark - Programmatic moves

TEST_F(FullscreenControllerTest, ForceHidden) {
  [controller_ moveHeaderToRestingPosition:NO];
  EXPECT_TRUE(IsHeaderHidden());
  EXPECT_EQ(0.0, webViewScrollViewProxy_.contentOffset.y);
}

#pragma mark - Simulated user moves.

TEST_F(FullscreenControllerTest, LargeManualScrollUpHides) {
  MoveMiddle();

  [controller_ webViewScrollViewWillBeginDragging:webViewScrollViewProxy_];

  CGFloat middle_point = webViewScrollViewProxy_.contentOffset.y;
  // Move up a bit, multiple times
  for (float i = 0.0; i < kHeaderHeight * 2.0; i++) {
    webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, middle_point + i);
  }
  [controller_ webViewScrollViewDidEndDragging:webViewScrollViewProxy_
                                willDecelerate:NO];
  EXPECT_TRUE(IsHeaderHidden());
}

TEST_F(FullscreenControllerTest, PartialManualScrollUpHides) {
  MoveMiddle();

  [controller_ webViewScrollViewWillBeginDragging:webViewScrollViewProxy_];

  CGFloat middle_point = webViewScrollViewProxy_.contentOffset.y;
  // Move up a bit, multiple times
  for (float i = 0.0; i < (kHeaderHeight * (2.0 / 3.0)); i++) {
    webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, middle_point + i);
  }
  [controller_ webViewScrollViewDidEndDragging:webViewScrollViewProxy_
                                willDecelerate:NO];
  EXPECT_TRUE(IsHeaderHidden());
}

TEST_F(FullscreenControllerTest, SmallPartialManualScrollUpNoEffect) {
  MoveMiddle();

  [controller_ webViewScrollViewWillBeginDragging:webViewScrollViewProxy_];

  CGFloat middle_point = webViewScrollViewProxy_.contentOffset.y;
  // Move up a bit, multiple times
  for (float i = 0.0; i < (kHeaderHeight / 3.0); i++) {
    webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, middle_point + i);
  }
  [controller_ webViewScrollViewDidEndDragging:webViewScrollViewProxy_
                                willDecelerate:NO];
  EXPECT_TRUE(IsHeaderVisible());
}

TEST_F(FullscreenControllerTest, LargeManualScrollDownShows) {
  MoveMiddleAndHide();

  [controller_ webViewScrollViewWillBeginDragging:webViewScrollViewProxy_];

  CGFloat middle_point = webViewScrollViewProxy_.contentOffset.y;
  // Move down a bit, multiple times
  for (float i = 0.0; i < kHeaderHeight * 2.0; i++) {
    webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, middle_point - i);
  }
  [controller_ webViewScrollViewDidEndDragging:webViewScrollViewProxy_
                                willDecelerate:NO];
  EXPECT_TRUE(IsHeaderVisible());
}

TEST_F(FullscreenControllerTest, PartialManualScrollDownShows) {
  MoveMiddleAndHide();

  [controller_ webViewScrollViewWillBeginDragging:webViewScrollViewProxy_];

  CGFloat middle_point = webViewScrollViewProxy_.contentOffset.y;
  // Move down a bit, multiple times
  for (float i = 0.0; i < (kHeaderHeight * (2.0 / 3.0)); i++) {
    webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, middle_point - i);
  }
  [controller_ webViewScrollViewDidEndDragging:webViewScrollViewProxy_
                                willDecelerate:NO];
  EXPECT_TRUE(IsHeaderVisible());
}

TEST_F(FullscreenControllerTest, SmallPartialManualScrollDownNoEffect) {
  MoveMiddleAndHide();

  [controller_ webViewScrollViewWillBeginDragging:webViewScrollViewProxy_];

  CGFloat middle_point = webViewScrollViewProxy_.contentOffset.y;
  // Move up a bit, multiple times
  for (float i = 0.0; i < (kHeaderHeight / 3.0); i++) {
    webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, middle_point - i);
  }
  [controller_ webViewScrollViewDidEndDragging:webViewScrollViewProxy_
                                willDecelerate:NO];
  EXPECT_TRUE(IsHeaderHidden());
}

#pragma mark - Page load.

TEST_F(FullscreenControllerTest, NoHideOnEnable) {
  [controller_ disableFullScreen];
  webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, 10.0);
  EXPECT_TRUE(IsHeaderVisible());

  [controller_ enableFullScreen];

  EXPECT_TRUE(IsHeaderVisible());
}

TEST_F(FullscreenControllerTest, NoHideOnEnableDueToManualScroll) {
  [controller_ disableFullScreen];
  webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, 1.0);
  EXPECT_TRUE(IsHeaderVisible());

  [controller_ webViewScrollViewWillBeginDragging:webViewScrollViewProxy_];
  webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, 10.0);
  EXPECT_TRUE(IsHeaderVisible());
  [controller_ webViewScrollViewDidEndDragging:webViewScrollViewProxy_
                                willDecelerate:NO];

  [controller_ enableFullScreen];

  EXPECT_TRUE(IsHeaderVisible());
}

#pragma mark - Keyboard.

TEST_F(FullscreenControllerTest, KeyboardAppearanceOnNonFullscreenPage) {
  // Add a textfield.
  UITextField* textField = [[UITextField alloc] init];
  AddSubViewToScrollView(textField);
  EXPECT_TRUE(IsHeaderVisible());

  // Show the keyboard.
  [textField becomeFirstResponder];
  EXPECT_TRUE(IsHeaderVisible());

  // Hide the keyboard.
  [textField resignFirstResponder];
  EXPECT_TRUE(IsHeaderVisible());
}

// TODO(lliabraa): Fails on Xcode 6 simulator (crbug.com/392433).
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_KeyboardAppearanceOnFullscreenPage \
  KeyboardAppearanceOnFullscreenPage
#else
#define MAYBE_KeyboardAppearanceOnFullscreenPage \
  KeyboardAppearanceOnFullscreenPage
#endif
TEST_F(FullscreenControllerTest, MAYBE_KeyboardAppearanceOnFullscreenPage) {
  // Scroll a bit to hide the toolbar.
  MoveMiddleAndHide();

  // Add a textfield.
  UITextField* textField = [[UITextField alloc] init];
  AddSubViewToScrollView(textField);
  EXPECT_TRUE(IsHeaderHidden());

  // Show the keyboard.
  [textField becomeFirstResponder];
  EXPECT_TRUE(IsHeaderVisible());

  // Hide the keyboard.
  [textField resignFirstResponder];

  // Toolbars are never auto-hidden.
  EXPECT_FALSE(IsHeaderHidden());
}

// TODO(lliabraa): Fails on Xcode 6 simulator (crbug.com/392433).
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_KeyboardStayOnUserScrollOnNonFullscreenPage \
  KeyboardStayOnUserScrollOnNonFullscreenPage
#else
#define MAYBE_KeyboardStayOnUserScrollOnNonFullscreenPage \
  KeyboardStayOnUserScrollOnNonFullscreenPage
#endif
TEST_F(FullscreenControllerTest,
       MAYBE_KeyboardStayOnUserScrollOnNonFullscreenPage) {
  // Add a textfield.
  UITextField* textField = [[UITextField alloc] init];
  AddSubViewToScrollView(textField);
  EXPECT_TRUE(IsHeaderVisible());

  // Show the keyboard.
  [textField becomeFirstResponder];
  EXPECT_TRUE(IsHeaderVisible());

  // Scroll.
  [controller_ webViewScrollViewWillBeginDragging:webViewScrollViewProxy_];
  webViewScrollViewProxy_.contentOffset = CGPointMake(0.0, 100.0);
  [controller_ webViewScrollViewDidEndDragging:webViewScrollViewProxy_
                                willDecelerate:NO];
  EXPECT_TRUE(IsHeaderVisible());

  // Hide the keyboard.
  [textField resignFirstResponder];
  EXPECT_TRUE(IsHeaderVisible());
}

}  // namespace
