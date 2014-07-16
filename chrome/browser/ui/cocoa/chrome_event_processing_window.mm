// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"

#include "base/logging.h"
#import "chrome/browser/ui/cocoa/browser_command_executor.h"
#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#import "content/public/browser/render_widget_host_view_mac_base.h"

typedef int (*KeyToCommandMapper)(bool, bool, bool, bool, int, unichar);

@interface ChromeEventProcessingWindow ()
// Duplicate the given key event, but changing the associated window.
- (NSEvent*)keyEventForWindow:(NSWindow*)window fromKeyEvent:(NSEvent*)event;
@end

@implementation ChromeEventProcessingWindow

- (BOOL)handleExtraKeyboardShortcut:(NSEvent*)event fromTable:
    (KeyToCommandMapper)commandForKeyboardShortcut {
  // Extract info from |event|.
  NSUInteger modifers = [event modifierFlags];
  const bool cmdKey = modifers & NSCommandKeyMask;
  const bool shiftKey = modifers & NSShiftKeyMask;
  const bool cntrlKey = modifers & NSControlKeyMask;
  const bool optKey = modifers & NSAlternateKeyMask;
  const unichar keyCode = [event keyCode];
  const unichar keyChar = KeyCharacterForEvent(event);

  int cmdNum = commandForKeyboardShortcut(cmdKey, shiftKey, cntrlKey, optKey,
      keyCode, keyChar);

  if (cmdNum != -1) {
    id executor = [self delegate];
    // A bit of sanity.
    DCHECK([executor conformsToProtocol:@protocol(BrowserCommandExecutor)]);
    DCHECK([executor respondsToSelector:@selector(executeCommand:)]);
    [executor executeCommand:cmdNum];
    return YES;
  }
  return NO;
}

- (BOOL)handleExtraWindowKeyboardShortcut:(NSEvent*)event {
  return [self handleExtraKeyboardShortcut:event
                                 fromTable:CommandForWindowKeyboardShortcut];
}

- (BOOL)handleDelayedWindowKeyboardShortcut:(NSEvent*)event {
  return [self handleExtraKeyboardShortcut:event
                         fromTable:CommandForDelayedWindowKeyboardShortcut];
}

- (BOOL)handleExtraBrowserKeyboardShortcut:(NSEvent*)event {
  return [self handleExtraKeyboardShortcut:event
                                 fromTable:CommandForBrowserKeyboardShortcut];
}

- (BOOL)performKeyEquivalent:(NSEvent*)event {
  // Some extension commands have higher priority than web content, and some
  // have lower priority. Regardless of whether the event is being
  // redispatched, let the extension system try to handle the event.
  NSWindow* window = event.window;
  if (window) {
    BrowserWindowController* controller = [window windowController];
    if ([controller respondsToSelector:@selector(handledByExtensionCommand:
                                                                  priority:)]) {
      ui::AcceleratorManager::HandlerPriority priority =
          redispatchingEvent_ ? ui::AcceleratorManager::kNormalPriority
                              : ui::AcceleratorManager::kHighPriority;
      if ([controller handledByExtensionCommand:event priority:priority])
        return YES;
    }
  }

  if (redispatchingEvent_)
    return NO;

  // Give the web site a chance to handle the event. If it doesn't want to
  // handle it, it will call us back with one of the |handle*| methods above.
  NSResponder* r = [self firstResponder];
  if ([r conformsToProtocol:@protocol(RenderWidgetHostViewMacBase)])
    return [r performKeyEquivalent:event];

  // If the delegate does not implement the BrowserCommandExecutor protocol,
  // then we don't need to handle browser specific shortcut keys.
  if (![[self delegate] conformsToProtocol:@protocol(BrowserCommandExecutor)])
    return [super performKeyEquivalent:event];

  // Handle per-window shortcuts like cmd-1, but do not handle browser-level
  // shortcuts like cmd-left (else, cmd-left would do history navigation even
  // if e.g. the Omnibox has focus).
  if ([self handleExtraWindowKeyboardShortcut:event])
    return YES;

  if ([super performKeyEquivalent:event])
    return YES;

  // Handle per-window shortcuts like Esc after giving everybody else a chance
  // to handle them
  return [self handleDelayedWindowKeyboardShortcut:event];
}

- (BOOL)redispatchKeyEvent:(NSEvent*)event {
  DCHECK(event);
  NSEventType eventType = [event type];
  if (eventType != NSKeyDown &&
      eventType != NSKeyUp &&
      eventType != NSFlagsChanged) {
    NOTREACHED();
    return YES;  // Pretend it's been handled in an effort to limit damage.
  }

  // Ordinarily, the event's window should be this window. However, when
  // switching between normal and fullscreen mode, we switch out the window, and
  // the event's window might be the previous window (or even an earlier one if
  // the renderer is running slowly and several mode switches occur). In this
  // rare case, we synthesize a new key event so that its associate window
  // (number) is our own.
  if ([event window] != self)
    event = [self keyEventForWindow:self fromKeyEvent:event];

  // Redispatch the event.
  eventHandled_ = YES;
  redispatchingEvent_ = YES;
  [NSApp sendEvent:event];
  redispatchingEvent_ = NO;

  // If the event was not handled by [NSApp sendEvent:], the sendEvent:
  // method below will be called, and because |redispatchingEvent_| is YES,
  // |eventHandled_| will be set to NO.
  return eventHandled_;
}

- (void)sendEvent:(NSEvent*)event {
  if (!redispatchingEvent_)
    [super sendEvent:event];
  else
    eventHandled_ = NO;
}

- (NSEvent*)keyEventForWindow:(NSWindow*)window fromKeyEvent:(NSEvent*)event {
  NSEventType eventType = [event type];

  // Convert the event's location from the original window's coordinates into
  // our own.
  NSPoint eventLoc = [event locationInWindow];
  eventLoc = [[event window] convertBaseToScreen:eventLoc];
  eventLoc = [self convertScreenToBase:eventLoc];

  // Various things *only* apply to key down/up.
  BOOL eventIsARepeat = NO;
  NSString* eventCharacters = nil;
  NSString* eventUnmodCharacters = nil;
  if (eventType == NSKeyDown || eventType == NSKeyUp) {
    eventIsARepeat = [event isARepeat];
    eventCharacters = [event characters];
    eventUnmodCharacters = [event charactersIgnoringModifiers];
  }

  // This synthesis may be slightly imperfect: we provide nil for the context,
  // since I (viettrungluu) am sceptical that putting in the original context
  // (if one is given) is valid.
  return [NSEvent keyEventWithType:eventType
                          location:eventLoc
                     modifierFlags:[event modifierFlags]
                         timestamp:[event timestamp]
                      windowNumber:[window windowNumber]
                           context:nil
                        characters:eventCharacters
       charactersIgnoringModifiers:eventUnmodCharacters
                         isARepeat:eventIsARepeat
                           keyCode:[event keyCode]];
}

@end  // ChromeEventProcessingWindow
