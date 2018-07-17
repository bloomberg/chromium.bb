// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/frame/browser_native_widget_window_mac.h"

#import <AppKit/AppKit.h>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/widget/widget.h"

@interface NSWindow (PrivateBrowserNativeWidgetAPI)
+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle;
@end

@interface NSThemeFrame (PrivateBrowserNativeWidgetAPI)
- (CGFloat)_titlebarHeight;
- (void)setStyleMask:(NSUInteger)styleMask;
@end

@interface BrowserWindowFrame : NativeWidgetMacNSWindowTitledFrame
@end

@implementation BrowserWindowFrame {
  BOOL _inFullScreen;
}

// NSThemeFrame overrides.

- (CGFloat)_titlebarHeight {
  if (_inFullScreen)
    return [super _titlebarHeight];

  if (views::Widget* widget = views::Widget::GetWidgetForNativeView(self)) {
    if (views::NonClientView* nonClientView = widget->non_client_view()) {
      auto* frameView = static_cast<const BrowserNonClientFrameView*>(
          nonClientView->frame_view());
      BrowserView* browserView = frameView->browser_view();
      return browserView->GetTabStripHeight() + frameView->GetTopInset(true);
    }
  }
  return [super _titlebarHeight];
}

- (void)setStyleMask:(NSUInteger)styleMask {
  _inFullScreen = (styleMask & NSWindowStyleMaskFullScreen) != 0;
  [super setStyleMask:styleMask];
}

- (BOOL)_shouldCenterTrafficLights {
  return YES;
}

// The base implementation justs tests [self class] == [NSThemeFrame class].
- (BOOL)_shouldFlipTrafficLightsForRTL API_AVAILABLE(macos(10.12)) {
  return [[self window] windowTitlebarLayoutDirection] ==
         NSUserInterfaceLayoutDirectionRightToLeft;
}

// Returning nil from _copyDragRegion prevents browser windows from being
// server-side draggable in the tab strip area. The area occupied by the title
// bar is normally draggable except where a view underlaps it which overrides
// -mouseDown: *and* returns YES from -acceptsFirstResponder. Currently, the
// tab strip is shown by a BridgedContentView which only sometimes returns YES
// from -acceptsFirstResponder. With this override, a window drag only starts
// after falling through -hitTest:.
//
// It would be ideal to avoid that round trip: right now, for example, browser
// windows aren't draggable while Chrome is hung or paused in a debugger.
// Another approach would be to expose an NSView for each views::View which
// exposes it to AppKit it as a non-draggable region. Cocoa app windows did
// this for draggable regions. (Tracked under https://crbug.com/830962.)
- (id)_copyDragRegion {
  return nil;
}

// Same as _copyDragRegion, but for 10.10.
- (NSRect)_draggableFrame NS_DEPRECATED_MAC(10_10, 10_11) {
  return NSZeroRect;
}

@end

@implementation BrowserNativeWidgetWindow

// NSWindow (PrivateAPI) overrides.

+ (Class)frameViewClassForStyleMask:(NSUInteger)windowStyle {
  // - NSThemeFrame and its subclasses will be nil if it's missing at runtime.
  if ([BrowserWindowFrame class])
    return [BrowserWindowFrame class];
  return [super frameViewClassForStyleMask:windowStyle];
}

// The base implementation returns YES if the window's frame view is a custom
// class, which causes undesirable changes in behavior. AppKit NSWindow
// subclasses are known to override it and return NO.
- (BOOL)_usesCustomDrawing {
  return NO;
}

// Handle "Move focus to the window toolbar" configured in System Preferences ->
// Keyboard -> Shortcuts -> Keyboard. Usually Ctrl+F5. The argument (|unknown|)
// tends to just be nil.
- (void)_handleFocusToolbarHotKey:(id)unknown {
  Browser* browser = chrome::FindBrowserWithWindow(self);
  if (browser)
    chrome::ExecuteCommand(browser, IDC_FOCUS_TOOLBAR);
}

@end
