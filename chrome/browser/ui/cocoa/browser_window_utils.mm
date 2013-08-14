// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_utils.h"

#include <Carbon/Carbon.h>

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/nsmenuitem_additions.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#include "content/public/browser/native_web_keyboard_event.h"

using content::NativeWebKeyboardEvent;

@interface MenuWalker : NSObject
+ (NSMenuItem*)itemForKeyEquivalent:(NSEvent*)key
                               menu:(NSMenu*)menu;
@end

@implementation MenuWalker
+ (NSMenuItem*)itemForKeyEquivalent:(NSEvent*)key
                               menu:(NSMenu*)menu {
  NSMenuItem* result = nil;

  for (NSMenuItem* item in [menu itemArray]) {
    NSMenu* submenu = [item submenu];
    if (submenu) {
      if (submenu != [NSApp servicesMenu])
        result = [self itemForKeyEquivalent:key
                                       menu:submenu];
    } else if ([item cr_firesForKeyEventIfEnabled:key]) {
      result = item;
    }

    if (result)
      break;
  }

  return result;
}
@end

@implementation BrowserWindowUtils
+ (BOOL)shouldHandleKeyboardEvent:(const NativeWebKeyboardEvent&)event {
  if (event.skip_in_browser || event.type == NativeWebKeyboardEvent::Char)
    return NO;
  DCHECK(event.os_event != NULL);
  return YES;
}

+ (int)getCommandId:(const NativeWebKeyboardEvent&)event {
  if ([event.os_event type] != NSKeyDown)
    return -1;

  // Look in menu.
  NSMenuItem* item = [MenuWalker itemForKeyEquivalent:event.os_event
                                                 menu:[NSApp mainMenu]];

  if (item && [item action] == @selector(commandDispatch:) && [item tag] > 0)
    return [item tag];

  // "Close window" doesn't use the |commandDispatch:| mechanism. Menu items
  // that do not correspond to IDC_ constants need no special treatment however,
  // as they can't be blacklisted in
  // |BrowserCommandController::IsReservedCommandOrKey()| anyhow.
  if (item && [item action] == @selector(performClose:))
    return IDC_CLOSE_WINDOW;

  // "Exit" doesn't use the |commandDispatch:| mechanism either.
  if (item && [item action] == @selector(terminate:))
    return IDC_EXIT;

  // Look in secondary keyboard shortcuts.
  NSUInteger modifiers = [event.os_event modifierFlags];
  const bool cmdKey = (modifiers & NSCommandKeyMask) != 0;
  const bool shiftKey = (modifiers & NSShiftKeyMask) != 0;
  const bool cntrlKey = (modifiers & NSControlKeyMask) != 0;
  const bool optKey = (modifiers & NSAlternateKeyMask) != 0;
  const int keyCode = [event.os_event keyCode];
  const unichar keyChar = KeyCharacterForEvent(event.os_event);

  int cmdNum = CommandForWindowKeyboardShortcut(
      cmdKey, shiftKey, cntrlKey, optKey, keyCode, keyChar);
  if (cmdNum != -1)
    return cmdNum;

  cmdNum = CommandForBrowserKeyboardShortcut(
      cmdKey, shiftKey, cntrlKey, optKey, keyCode, keyChar);
  if (cmdNum != -1)
    return cmdNum;

  return -1;
}

+ (BOOL)handleKeyboardEvent:(NSEvent*)event
                   inWindow:(NSWindow*)window {
  ChromeEventProcessingWindow* event_window =
      static_cast<ChromeEventProcessingWindow*>(window);
  DCHECK([event_window isKindOfClass:[ChromeEventProcessingWindow class]]);

  // Do not fire shortcuts on key up.
  if ([event type] == NSKeyDown) {
    // Send the event to the menu before sending it to the browser/window
    // shortcut handling, so that if a user configures cmd-left to mean
    // "previous tab", it takes precedence over the built-in "history back"
    // binding. Other than that, the |-redispatchKeyEvent:| call would take care
    // of invoking the original menu item shortcut as well.

    if ([[NSApp mainMenu] performKeyEquivalent:event])
      return true;

    if ([event_window handleExtraBrowserKeyboardShortcut:event])
      return true;

    if ([event_window handleExtraWindowKeyboardShortcut:event])
      return true;

    if ([event_window handleDelayedWindowKeyboardShortcut:event])
      return true;
  }

  return [event_window redispatchKeyEvent:event];
}

+ (NSString*)scheduleReplaceOldTitle:(NSString*)oldTitle
                        withNewTitle:(NSString*)newTitle
                           forWindow:(NSWindow*)window {
  if (oldTitle)
    [[NSRunLoop currentRunLoop]
        cancelPerformSelector:@selector(setTitle:)
                       target:window
                     argument:oldTitle];

  [[NSRunLoop currentRunLoop]
      performSelector:@selector(setTitle:)
               target:window
             argument:newTitle
                order:0
                modes:[NSArray arrayWithObject:NSDefaultRunLoopMode]];
  return [newTitle copy];
}

// The titlebar/tabstrip header on the mac is slightly smaller than on Windows.
// There is also no window frame to the left and right of the web contents on
// mac.
// To keep:
// - the window background pattern (IDR_THEME_FRAME.*) lined up vertically with
// the tab and toolbar patterns
// - the toolbar pattern lined up horizontally with the NTP background.
// we have to shift the pattern slightly, rather than drawing from the top left
// corner of the frame / tabstrip. The offsets below were empirically determined
// in order to line these patterns up.
//
// This will make the themes look slightly different than in Windows/Linux
// because of the differing heights between window top and tab top, but this has
// been approved by UI.
const CGFloat kPatternHorizontalOffset = -5;
// Without tab strip, offset an extra pixel (determined by experimentation).
const CGFloat kPatternVerticalOffset = 2;
const CGFloat kPatternVerticalOffsetNoTabStrip = 3;

+ (NSPoint)themeImagePositionFor:(NSView*)windowView
                    withTabStrip:(NSView*)tabStripView
                       alignment:(ThemeImageAlignment)alignment {
  if (!tabStripView) {
    return NSMakePoint(kPatternHorizontalOffset,
                       NSHeight([windowView bounds]) +
                           kPatternVerticalOffsetNoTabStrip);
  }

  NSPoint position =
      [BrowserWindowUtils themeImagePositionInTabStripCoords:tabStripView
                                                   alignment:alignment];
  return [tabStripView convertPoint:position toView:windowView];
}

+ (NSPoint)themeImagePositionInTabStripCoords:(NSView*)tabStripView
                                    alignment:(ThemeImageAlignment)alignment {
  DCHECK(tabStripView);

  if (alignment == THEME_IMAGE_ALIGN_WITH_TAB_STRIP) {
    // The theme image is lined up with the top of the tab which is below the
    // top of the tab strip.
    return NSMakePoint(kPatternHorizontalOffset,
                       [TabStripController defaultTabHeight] +
                           kPatternVerticalOffset);
  }
  // The theme image is lined up with the top of the tab strip (as opposed to
  // the top of the tab above). This is the same as lining up with the top of
  // the window's root view when not in presentation mode.
  return NSMakePoint(kPatternHorizontalOffset,
                     NSHeight([tabStripView bounds]) +
                         kPatternVerticalOffsetNoTabStrip);
}

+ (void)activateWindowForController:(NSWindowController*)controller {
  // Per http://crbug.com/73779 and http://crbug.com/75223, we need this to
  // properly activate windows if Chrome is not the active application.
  [[controller window] makeKeyAndOrderFront:controller];
  ProcessSerialNumber psn;
  GetCurrentProcess(&psn);
  SetFrontProcessWithOptions(&psn, kSetFrontProcessFrontWindowOnly);
}

@end
