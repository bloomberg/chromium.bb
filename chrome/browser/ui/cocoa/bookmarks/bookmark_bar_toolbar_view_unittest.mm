// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/themes/theme_service.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_controller.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_bar_toolbar_view.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/theme_provider.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgumentPointee;

// When testing the floating drawing, we need to have a source of theme data.
class MockThemeProvider : public ui::ThemeProvider {
 public:
  // Cross platform methods
  MOCK_METHOD1(Init, void(Profile*));
  MOCK_CONST_METHOD1(GetBitmapNamed, SkBitmap*(int));
  MOCK_CONST_METHOD1(GetColor, SkColor(int));
  MOCK_CONST_METHOD2(GetDisplayProperty, bool(int, int*));
  MOCK_CONST_METHOD0(ShouldUseNativeFrame, bool());
  MOCK_CONST_METHOD1(HasCustomImage, bool(int));
  MOCK_CONST_METHOD1(GetRawData, RefCountedMemory*(int));

  // OSX stuff
  MOCK_CONST_METHOD2(GetNSImageNamed, NSImage*(int, bool));
  MOCK_CONST_METHOD2(GetNSImageColorNamed, NSColor*(int, bool));
  MOCK_CONST_METHOD2(GetNSColor, NSColor*(int, bool));
  MOCK_CONST_METHOD2(GetNSColorTint, NSColor*(int, bool));
  MOCK_CONST_METHOD1(GetNSGradient, NSGradient*(int));
};

// Allows us to inject our fake controller below.
@interface BookmarkBarToolbarView (TestingAPI)
-(void)setController:(id<BookmarkBarToolbarViewController>)controller;
@end

@implementation BookmarkBarToolbarView (TestingAPI)
-(void)setController:(id<BookmarkBarToolbarViewController>)controller {
  controller_ = controller;
}
@end

// Allows us to control which way the view is rendered.
@interface DrawDetachedBarFakeController :
    NSObject<BookmarkBarState, BookmarkBarToolbarViewController> {
 @private
  int currentTabContentsHeight_;
  ui::ThemeProvider* themeProvider_;
  bookmarks::VisualState visualState_;
}
@property (nonatomic, assign) int currentTabContentsHeight;
@property (nonatomic, assign) ui::ThemeProvider* themeProvider;
@property (nonatomic, assign) bookmarks::VisualState visualState;

// |BookmarkBarState| protocol:
- (BOOL)isVisible;
- (BOOL)isAnimationRunning;
- (BOOL)isInState:(bookmarks::VisualState)state;
- (BOOL)isAnimatingToState:(bookmarks::VisualState)state;
- (BOOL)isAnimatingFromState:(bookmarks::VisualState)state;
- (BOOL)isAnimatingFromState:(bookmarks::VisualState)fromState
                     toState:(bookmarks::VisualState)toState;
- (BOOL)isAnimatingBetweenState:(bookmarks::VisualState)fromState
                       andState:(bookmarks::VisualState)toState;
- (CGFloat)detachedMorphProgress;
@end

@implementation DrawDetachedBarFakeController
@synthesize currentTabContentsHeight = currentTabContentsHeight_;
@synthesize themeProvider = themeProvider_;
@synthesize visualState = visualState_;

- (id)init {
  if ((self = [super init])) {
    [self setVisualState:bookmarks::kHiddenState];
  }
  return self;
}

- (BOOL)isVisible { return YES; }
- (BOOL)isAnimationRunning { return NO; }
- (BOOL)isInState:(bookmarks::VisualState)state
    { return ([self visualState] == state) ? YES : NO; }
- (BOOL)isAnimatingToState:(bookmarks::VisualState)state { return NO; }
- (BOOL)isAnimatingFromState:(bookmarks::VisualState)state { return NO; }
- (BOOL)isAnimatingFromState:(bookmarks::VisualState)fromState
                     toState:(bookmarks::VisualState)toState { return NO; }
- (BOOL)isAnimatingBetweenState:(bookmarks::VisualState)fromState
                       andState:(bookmarks::VisualState)toState { return NO; }
- (CGFloat)detachedMorphProgress { return 1; }
@end

class BookmarkBarToolbarViewTest : public CocoaTest {
 public:
  BookmarkBarToolbarViewTest() {
    controller_.reset([[DrawDetachedBarFakeController alloc] init]);
    NSRect frame = NSMakeRect(0, 0, 400, 40);
    scoped_nsobject<BookmarkBarToolbarView> view(
        [[BookmarkBarToolbarView alloc] initWithFrame:frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
    [view_ setController:controller_.get()];
  }

  scoped_nsobject<DrawDetachedBarFakeController> controller_;
  BookmarkBarToolbarView* view_;
};

TEST_VIEW(BookmarkBarToolbarViewTest, view_)

// Test drawing (part 1), mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarToolbarViewTest, DisplayAsNormalBar) {
  [controller_.get() setVisualState:bookmarks::kShowingState];
  [view_ display];
}

// Test drawing (part 2), mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarToolbarViewTest, DisplayAsDetachedBarWithNoImage) {
  [controller_.get() setVisualState:bookmarks::kDetachedState];

  // Tests where we don't have a background image, only a color.
  MockThemeProvider provider;
  EXPECT_CALL(provider, GetColor(ThemeService::COLOR_NTP_BACKGROUND))
      .WillRepeatedly(Return(SK_ColorWHITE));
  EXPECT_CALL(provider, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillRepeatedly(Return(false));
  [controller_.get() setThemeProvider:&provider];

  [view_ display];
}

// Actions used in DisplayAsDetachedBarWithBgImage.
ACTION(SetBackgroundTiling) {
  *arg1 = ThemeService::NO_REPEAT;
  return true;
}

ACTION(SetAlignLeft) {
  *arg1 = ThemeService::ALIGN_LEFT;
  return true;
}

// Test drawing (part 3), mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarToolbarViewTest, DisplayAsDetachedBarWithBgImage) {
  [controller_.get() setVisualState:bookmarks::kDetachedState];

  // Tests where we have a background image, with positioning information.
  MockThemeProvider provider;

  // Advertise having an image.
  EXPECT_CALL(provider, GetColor(ThemeService::COLOR_NTP_BACKGROUND))
      .WillRepeatedly(Return(SK_ColorRED));
  EXPECT_CALL(provider, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillRepeatedly(Return(true));

  // Return the correct tiling/alignment information.
  EXPECT_CALL(provider,
      GetDisplayProperty(ThemeService::NTP_BACKGROUND_TILING, _))
      .WillRepeatedly(SetBackgroundTiling());
  EXPECT_CALL(provider,
      GetDisplayProperty(ThemeService::NTP_BACKGROUND_ALIGNMENT, _))
      .WillRepeatedly(SetAlignLeft());

  // Create a dummy bitmap full of not-red to blit with.
  SkBitmap fake_bg;
  fake_bg.setConfig(SkBitmap::kARGB_8888_Config, 800, 800);
  fake_bg.allocPixels();
  fake_bg.eraseColor(SK_ColorGREEN);
  EXPECT_CALL(provider, GetBitmapNamed(IDR_THEME_NTP_BACKGROUND))
      .WillRepeatedly(Return(&fake_bg));

  [controller_.get() setThemeProvider:&provider];
  [controller_.get() setCurrentTabContentsHeight:200];

  [view_ display];
}

// TODO(viettrungluu): write more unit tests, especially after my refactoring.
