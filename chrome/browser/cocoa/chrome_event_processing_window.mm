// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/chrome_event_processing_window.h"

#include "base/logging.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/browser_frame_view.h"
#import "chrome/browser/cocoa/tab_strip_controller.h"
#import "chrome/browser/renderer_host/render_widget_host_view_mac.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"

typedef int (*KeyToCommandMapper)(bool, bool, bool, bool, int);

@implementation ChromeEventProcessingWindow

- (BOOL)handleExtraKeyboardShortcut:(NSEvent*)event fromTable:
    (KeyToCommandMapper)commandForKeyboardShortcut {
  // Extract info from |event|.
  NSUInteger modifers = [event modifierFlags];
  const bool cmdKey = modifers & NSCommandKeyMask;
  const bool shiftKey = modifers & NSShiftKeyMask;
  const bool cntrlKey = modifers & NSControlKeyMask;
  const bool optKey = modifers & NSAlternateKeyMask;
  const int keyCode = [event keyCode];

  int cmdNum = commandForKeyboardShortcut(cmdKey, shiftKey, cntrlKey, optKey,
      keyCode);

  BrowserWindowController* controller =
      (BrowserWindowController*)[self delegate];
  // A bit of sanity.
  DCHECK([controller isKindOfClass:[BrowserWindowController class]]);
  DCHECK([controller respondsToSelector:@selector(executeCommand:)]);

  if (cmdNum != -1) {
    [controller executeCommand:cmdNum];
    return YES;
  }
  return NO;
}

- (BOOL)handleExtraWindowKeyboardShortcut:(NSEvent*)event {
  return [self handleExtraKeyboardShortcut:event
                                 fromTable:CommandForWindowKeyboardShortcut];
}

- (BOOL)handleExtraBrowserKeyboardShortcut:(NSEvent*)event {
  return [self handleExtraKeyboardShortcut:event
                                 fromTable:CommandForBrowserKeyboardShortcut];
}

- (BOOL)shortcircuitEvent:(NSEvent*)event {
  if (!redispatchingEvent_ &&
      ([event type] == NSKeyDown || [event type] == NSKeyUp)) {
    if ([[self firstResponder]
        isKindOfClass:[RenderWidgetHostViewCocoa class]]) {
      // No other mac browser sends keyup() for keyboard equivalents, so let's
      // suppress this.
      if (([event modifierFlags] & NSCommandKeyMask) && [event type] == NSKeyUp)
        return YES;

      RenderWidgetHostViewCocoa* rwhv = static_cast<RenderWidgetHostViewCocoa*>(
          [self firstResponder]);
      [rwhv keyEvent:event];
      return YES;
    }
  }
  return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent*)event {
  if (redispatchingEvent_)
    return NO;

  // |shortcircuitEvent:| should handle all events directed to the RWHV.
  DCHECK(![[self firstResponder]
      isKindOfClass:[RenderWidgetHostViewCocoa class]]);

  // Handle per-window shortcuts like cmd-1, but do not handle browser-level
  // shortcuts like cmd-left (else, cmd-left would do history navigation even
  // if e.g. the Omnibox has focus). If the web has focus, don't do this here,
  // since the web needs to get a chance at swallowing the event first.
  if ([self handleExtraWindowKeyboardShortcut:event])
    return YES;
  return [super performKeyEquivalent:event];
}

- (void)redispatchEvent:(NSEvent*)event {
  DCHECK(event);
  DCHECK([event window] == self);
  redispatchingEvent_ = YES;
  [NSApp sendEvent:event];
  redispatchingEvent_ = NO;
}

- (void)sendEvent:(NSEvent*)event {
  if (!redispatchingEvent_)
    [super sendEvent:event];
}

@end  // ChromeEventProcessingWindow

