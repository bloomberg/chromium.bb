// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chrome_browser_window.h"

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/themed_window.h"
#include "ui/base/theme_provider.h"

@implementation ChromeBrowserWindow

- (void)underlaySurfaceAdded {
  DCHECK_GE(underlaySurfaceCount_, 0);
  ++underlaySurfaceCount_;

  // We're having the OpenGL surface render under the window, so the window
  // needs to be not opaque.
  if (underlaySurfaceCount_ == 1)
    [self setOpaque:NO];
}

- (void)underlaySurfaceRemoved {
  --underlaySurfaceCount_;
  DCHECK_GE(underlaySurfaceCount_, 0);

  if (underlaySurfaceCount_ == 0)
    [self setOpaque:YES];
}

- (ui::ThemeProvider*)themeProvider {
  id delegate = [self delegate];
  if (![delegate respondsToSelector:@selector(themeProvider)])
    return NULL;
  return [delegate themeProvider];
}

- (ThemedWindowStyle)themedWindowStyle {
  id delegate = [self delegate];
  if (![delegate respondsToSelector:@selector(themedWindowStyle)])
    return THEMED_NORMAL;
  return [delegate themedWindowStyle];
}

- (NSPoint)themePatternPhase {
  id delegate = [self delegate];
  if (![delegate respondsToSelector:@selector(themePatternPhase)])
    return NSMakePoint(0, 0);
  return [delegate themePatternPhase];
}

- (BOOL)performCloseShouldRouteToCommandDispatch:(id)sender {
  return [[self delegate] respondsToSelector:@selector(commandDispatch:)] &&
         [sender isKindOfClass:[NSMenuItem class]] &&
         [sender tag] == IDC_CLOSE_TAB;
}

- (void)performClose:(id)sender {
  // Route -performClose: to -commandDispatch: on the delegate when coming from
  // the "close tab" menu item. This is done here, rather than simply connecting
  // the menu item to -commandDispatch: in IB because certain parts of AppKit,
  // such as the Lion dictionary pop up, expect Cmd-W to send -performClose:.
  // See http://crbug.com/104931 for details.
  if ([self performCloseShouldRouteToCommandDispatch:sender]) {
    id delegate = [self delegate];
    [delegate commandDispatch:sender];
    return;
  }

  [super performClose:sender];
}

@end
