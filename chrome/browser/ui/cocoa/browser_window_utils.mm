// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/browser_window_utils.h"

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"
#import "chrome/browser/ui/cocoa/nsmenuitem_additions.h"
#include "content/common/native_web_keyboard_event.h"

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

  // "Close window" doesn't use the |commandDispatch:| mechanism. Menu items
  // that do not correspond to IDC_ constants need no special treatment however,
  // as they can't be blacklisted in |Browser::IsReservedCommandOrKey()| anyhow.
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

@end
