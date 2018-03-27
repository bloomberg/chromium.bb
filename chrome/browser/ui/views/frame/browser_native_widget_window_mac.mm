// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/frame/browser_native_widget_window_mac.h"

#if !defined(GOOGLE_CHROME_BUILD)
#import "chrome/browser/ui/views/frame/macviews_under_construction_window_mac.h"
#endif

#import <AppKit/AppKit.h>

namespace {
constexpr NSInteger kWindowButtonsOffsetFromBottom = 9;
constexpr NSInteger kWindowButtonsOffsetFromLeft = 11;
constexpr NSInteger kTitleBarHeight = 37;
}  // namespace

@interface NSWindow (PrivateAPI)
+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle;
@end

// Weak lets Chrome launch even if a future macOS doesn't have NSThemeFrame.
WEAK_IMPORT_ATTRIBUTE
@interface NSThemeFrame : NSView
@end

@interface BrowserWindowFrame : NSThemeFrame
@end

@implementation BrowserWindowFrame

// NSThemeFrame overrides.

- (CGFloat)_minXTitlebarWidgetInset {
  return kWindowButtonsOffsetFromLeft;
}

- (CGFloat)_minYTitlebarButtonsOffset {
  return -kWindowButtonsOffsetFromBottom;
}

- (CGFloat)_titlebarHeight {
  return kTitleBarHeight;
}

// The base implementation justs tests [self class] == [NSThemeFrame class].
- (BOOL)_shouldFlipTrafficLightsForRTL API_AVAILABLE(macos(10.12)) {
  return [[self window] windowTitlebarLayoutDirection] ==
         NSUserInterfaceLayoutDirectionRightToLeft;
}

@end

@implementation BrowserNativeWidgetWindow

// NSWindow (PrivateAPI) overrides.

+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle {
  // - NSThemeFrame and its subclasses will be nil if it's missing at runtime.
  if ([BrowserWindowFrame class])
    // TODO(crbug/825968): fullscreen should have a reduced titlebar height.
    return [BrowserWindowFrame class];
  return [super frameViewClassForStyleMask:windowStyle];
}

// The base implementation returns YES if the window's frame view is a custom
// class, which causes undesirable changes in behavior. AppKit NSWindow
// subclasses are known to override it and return NO.
- (BOOL)_usesCustomDrawing {
  return NO;
}

// NSWindow overrides.

- (void)orderWindow:(NSWindowOrderingMode)place relativeTo:(NSInteger)otherWin {
  [super orderWindow:place relativeTo:otherWin];
#if !defined(GOOGLE_CHROME_BUILD)
  if (place != NSWindowOut)
    [MacViewsUnderConstructionWindow attachToWindow:self];
#endif
}

@end
