// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"

#include "base/logging.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"
#import "chrome/browser/ui/cocoa/browser_window_views_mac.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#include "content/public/browser/native_web_keyboard_event.h"

@implementation ChromeCommandDispatcherDelegate

- (BOOL)handleExtraKeyboardShortcut:(NSEvent*)event window:(NSWindow*)window {
  int cmd = CommandForKeyEvent(event);
  if (cmd == -1)
    return false;

  chrome::ExecuteCommand(chrome::FindBrowserWithWindow(window), cmd);
  return true;
}

- (BOOL)eventHandledByExtensionCommand:(NSEvent*)event
                              priority:(ui::AcceleratorManager::HandlerPriority)
                                           priority {
  if ([event window]) {
    BrowserWindowController* controller =
        BrowserWindowControllerForWindow([event window]);
    // |controller| is only set in Cocoa. In toolkit-views extension commands
    // are handled by BrowserView.
    if ([controller respondsToSelector:@selector(handledByExtensionCommand:
                                                                  priority:)]) {
      if ([controller handledByExtensionCommand:event priority:priority])
        return YES;
    }
  }
  return NO;
}

- (BOOL)prePerformKeyEquivalent:(NSEvent*)event window:(NSWindow*)window {
  // TODO(erikchen): Detect symbolic hot keys, and force control to be passed
  // back to AppKit so that it can handle it correctly.
  // https://crbug.com/846893.

  NSResponder* responder = [window firstResponder];
  if ([responder conformsToProtocol:@protocol(CommandDispatcherTarget)]) {
    NSObject<CommandDispatcherTarget>* target =
        static_cast<NSObject<CommandDispatcherTarget>*>(responder);
    if ([target isKeyLocked:event])
      return NO;
  }

  if ([self eventHandledByExtensionCommand:event
                                  priority:ui::AcceleratorManager::
                                               kHighPriority]) {
    return YES;
  }

  // The specification for this private extensions API is incredibly vague. For
  // now, we avoid triggering chrome commands prior to giving the firstResponder
  // a chance to handle the event.
  if (extensions::GlobalShortcutListener::GetInstance()
          ->IsShortcutHandlingSuspended()) {
    return NO;
  }

  // If this keyEquivalent corresponds to a Chrome command, trigger it directly
  // via chrome::ExecuteCommand. We avoid going through the NSMenu for two
  // reasons:
  //  * consistency - some commands are not present in the NSMenu. Furthermore,
  //  the NSMenu's contents can be dynamically updated, so there's no guarantee
  //  that passing the event to NSMenu will even do what we think it will do.
  //  * Avoiding sleeps. By default, the implementation of NSMenu
  //  performKeyEquivalent: has a nested run loop that spins for 100ms. If we
  //  avoid that by spinning our task runner in their private mode, there's a
  //  built in nanosleep. See https://crbug.com/836947#c8.
  //
  // By not passing the event to AppKit, we do lose out on the brief
  // highlighting of the NSMenu.
  //
  // TODO(erikchen): Add a throttle. Otherwise, it's possible for a user holding
  // down a hotkey [e.g. cmd + w] to accidentally close too many tabs!
  // https://crbug.com/846893.
  int cmd = CommandForKeyEvent(event);
  if (cmd != -1) {
    Browser* browser = chrome::FindBrowserWithWindow(window);
    if (browser && browser->command_controller()->IsReservedCommandOrKey(
                       cmd, content::NativeWebKeyboardEvent(event))) {
      chrome::ExecuteCommand(browser, cmd);
      return YES;
    }
  }

  return NO;
}

- (BOOL)postPerformKeyEquivalent:(NSEvent*)event window:(NSWindow*)window {
  if ([self eventHandledByExtensionCommand:event
                                  priority:ui::AcceleratorManager::
                                               kNormalPriority]) {
    return true;
  }

  int cmd = CommandForKeyEvent(event);
  if (cmd != -1) {
    Browser* browser = chrome::FindBrowserWithWindow(window);
    if (browser) {
      chrome::ExecuteCommand(browser, cmd);
      return true;
    }
  }

  return false;
}

@end  // ChromeCommandDispatchDelegate
