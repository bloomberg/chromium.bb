// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "app/theme_provider.h"
#include "base/scoped_nsobject.h"
#include "chrome/browser/browser_theme_provider.h"
#import "chrome/browser/cocoa/bookmark_bar_controller.h"
#import "chrome/browser/cocoa/bookmark_bar_toolbar_view.h"
#import "chrome/browser/cocoa/cocoa_test_helper.h"
#include "grit/theme_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgumentPointee;

// When testing the floating drawing, we need to have a source of
// theme data.
class MockThemeProvider : public ThemeProvider {
 public:
  // Cross platform methods
  MOCK_METHOD1(Init, void(Profile*));
  MOCK_CONST_METHOD1(GetBitmapNamed, SkBitmap*(int));
  MOCK_CONST_METHOD1(GetColor, SkColor(int));
  MOCK_CONST_METHOD2(GetDisplayProperty, bool(int, int*));
  MOCK_CONST_METHOD0(ShouldUseNativeFrame, bool());
  MOCK_CONST_METHOD1(HasCustomImage, bool(int));
  MOCK_CONST_METHOD1(GetRawData,  RefCountedMemory*(int));

  // OSX stuff
  MOCK_CONST_METHOD1(GetNSImageNamed, NSImage*(int));
  MOCK_CONST_METHOD1(GetNSColor, NSColor*(int));
  MOCK_CONST_METHOD1(GetNSColorTint, NSColor*(int));
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
@interface DrawFloatingFakeController :
    NSObject<BookmarkBarToolbarViewController> {
  int current_tab_contents_height_;
  ThemeProvider* theme_provider_;
  BOOL drawAsFloating_;
}
@property(assign) int currentTabContentsHeight;
@property(assign) ThemeProvider* themeProvider;
@property(assign) BOOL drawAsFloatingBar;
@end

@implementation DrawFloatingFakeController
@synthesize currentTabContentsHeight = current_tab_contents_height_;
@synthesize themeProvider = theme_provider_;
@synthesize drawAsFloatingBar = drawAsFloating_;
@end

class BookmarkBarToolbarViewTest : public PlatformTest {
 public:
  BookmarkBarToolbarViewTest() {
    controller_.reset([[DrawFloatingFakeController alloc] init]);
    NSRect frame = NSMakeRect(0, 0, 400, 40);
    view_.reset([[BookmarkBarToolbarView alloc] initWithFrame:frame]);
    [cocoa_helper_.contentView() addSubview:view_.get()];
    [view_.get() setController:controller_.get()];
  }

  CocoaTestHelper cocoa_helper_;  // Inits Cocoa, creates window, etc...
  scoped_nsobject<DrawFloatingFakeController> controller_;
  scoped_nsobject<BookmarkBarToolbarView> view_;
};

// Test adding/removing from the view hierarchy, mostly to ensure nothing
// leaks or crashes.
TEST_F(BookmarkBarToolbarViewTest, AddRemove) {
  EXPECT_EQ(cocoa_helper_.contentView(), [view_ superview]);
  [view_.get() removeFromSuperview];
  EXPECT_FALSE([view_ superview]);
}

// Test drawing (part 1), mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarToolbarViewTest, DisplayAsNormalBar) {
  [controller_.get() setDrawAsFloatingBar:NO];
  [view_ display];
}

// Test drawing (part 2), mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarToolbarViewTest, DisplayAsFloatingBarWithNoImage) {
  [controller_.get() setDrawAsFloatingBar:YES];

  // Tests where we don't have a background image, only a color.
  MockThemeProvider provider;
  EXPECT_CALL(provider, GetColor(BrowserThemeProvider::COLOR_NTP_BACKGROUND))
      .WillRepeatedly(Return(SK_ColorWHITE));
  EXPECT_CALL(provider, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillRepeatedly(Return(false));
  [controller_.get() setThemeProvider:&provider];

  [view_ display];
}

// Actions used in DisplayAsFloatingBarWithBgImage.
ACTION(SetBackgroundTiling) {
  *arg1 = BrowserThemeProvider::NO_REPEAT;
  return true;
}

ACTION(SetAlignLeft) {
  *arg1 = BrowserThemeProvider::ALIGN_LEFT;
  return true;
}

// Test drawing (part 3), mostly to ensure nothing leaks or crashes.
TEST_F(BookmarkBarToolbarViewTest, DisplayAsFloatingBarWithBgImage) {
  [controller_.get() setDrawAsFloatingBar:YES];

  // Tests where we have a background image, with positioning information.
  MockThemeProvider provider;

  // Advertise having an image.
  EXPECT_CALL(provider, GetColor(BrowserThemeProvider::COLOR_NTP_BACKGROUND))
      .WillRepeatedly(Return(SK_ColorRED));
  EXPECT_CALL(provider, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillRepeatedly(Return(true));

  // Return the correct tiling/alignment information.
  EXPECT_CALL(provider,
      GetDisplayProperty(BrowserThemeProvider::NTP_BACKGROUND_TILING, _))
      .WillRepeatedly(SetBackgroundTiling());
  EXPECT_CALL(provider,
      GetDisplayProperty(BrowserThemeProvider::NTP_BACKGROUND_ALIGNMENT, _))
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
