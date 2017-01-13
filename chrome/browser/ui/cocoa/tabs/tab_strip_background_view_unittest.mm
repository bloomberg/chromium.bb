// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/tabs/tab_strip_background_view.h"

#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "ui/base/default_theme_provider.h"

@class NSVisualEffectView;

namespace {

bool ContainsViewOfClass(NSView* view, Class cls) {
  if ([view isKindOfClass:cls])
    return true;
  for (NSView* subview in view.subviews) {
    if (ContainsViewOfClass(subview, cls))
      return true;
  }
  return false;
}

class TabStripBackgroundViewTest : public CocoaTest {
 protected:
  const base::scoped_nsobject<TabStripBackgroundView>
      tab_strip_background_view_{
          [[TabStripBackgroundView alloc] initWithFrame:NSZeroRect]};
};

class MockThemeProvider : public ui::DefaultThemeProvider {
 public:
  bool UsingSystemTheme() const override { return using_system_theme_; }

  void SetUsingSystemTheme(bool using_system_theme) {
    using_system_theme_ = using_system_theme;
  }

 private:
  bool using_system_theme_ = true;
};

}  // namespace

@interface TabStripBackgroundViewTestWindow : NSWindow
@property(nonatomic) const ui::ThemeProvider* themeProvider;
@end

@implementation TabStripBackgroundViewTestWindow
@synthesize themeProvider = themeProvider_;
@end

TEST_F(TabStripBackgroundViewTest, TestVisualEffectView) {
  // TODO(sdy): Remove once we no longer support 10.9.
  // Skip this test when running on an OS without NSVisualEffectView.
  if (![NSVisualEffectView class])
    return;

  auto has_visual_effect_view = [&]() {
    return ContainsViewOfClass(tab_strip_background_view_,
                               [NSVisualEffectView class]);
  };

  EXPECT_FALSE(has_visual_effect_view());

  base::scoped_nsobject<TabStripBackgroundViewTestWindow>
      scoped_window([[TabStripBackgroundViewTestWindow alloc] init]);
  TabStripBackgroundViewTestWindow* window = scoped_window.get();

  MockThemeProvider theme_provider;
  window.themeProvider = &theme_provider;

  [window.contentView addSubview:tab_strip_background_view_];

  [window makeKeyAndOrderFront:nil];
  [tab_strip_background_view_ windowDidChangeActive];
  EXPECT_TRUE(has_visual_effect_view());

  window.styleMask |= NSFullScreenWindowMask;
  EXPECT_FALSE(has_visual_effect_view());

  window.styleMask &= ~NSFullScreenWindowMask;
  EXPECT_TRUE(has_visual_effect_view());

  theme_provider.SetUsingSystemTheme(false);
  [tab_strip_background_view_ windowDidChangeTheme];
  EXPECT_FALSE(has_visual_effect_view());

  theme_provider.SetUsingSystemTheme(true);
  [tab_strip_background_view_ windowDidChangeTheme];
  EXPECT_TRUE(has_visual_effect_view());
}
