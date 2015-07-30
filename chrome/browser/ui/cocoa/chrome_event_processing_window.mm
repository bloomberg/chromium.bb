// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chrome_event_processing_window.h"

#include "base/logging.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/browser_window_controller_private.h"
#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"
#import "content/public/browser/render_widget_host_view_mac_base.h"

namespace {

// Type of functions listed in global_keyboard_shortcuts_mac.h.
typedef int (*KeyToCommandMapper)(bool, bool, bool, bool, int, unichar);

// If the event is for a Browser window, and the key combination has an
// associated command, execute the command.
bool HandleExtraKeyboardShortcut(
    NSEvent* event,
    NSWindow* window,
    KeyToCommandMapper command_for_keyboard_shortcut) {
  // Extract info from |event|.
  NSUInteger modifers = [event modifierFlags];
  const bool command = modifers & NSCommandKeyMask;
  const bool shift = modifers & NSShiftKeyMask;
  const bool control = modifers & NSControlKeyMask;
  const bool option = modifers & NSAlternateKeyMask;
  const int key_code = [event keyCode];
  const unichar key_char = KeyCharacterForEvent(event);

  int cmd = command_for_keyboard_shortcut(command, shift, control, option,
                                          key_code, key_char);

  if (cmd == -1)
    return false;

  // Only handle event if this is a browser window.
  Browser* browser = chrome::FindBrowserWithWindow(window);
  if (!browser)
    return false;

  chrome::ExecuteCommand(browser, cmd);
  return true;
}

bool HandleExtraWindowKeyboardShortcut(NSEvent* event, NSWindow* window) {
  return HandleExtraKeyboardShortcut(event, window,
                                     CommandForWindowKeyboardShortcut);
}

bool HandleDelayedWindowKeyboardShortcut(NSEvent* event, NSWindow* window) {
  return HandleExtraKeyboardShortcut(event, window,
                                     CommandForDelayedWindowKeyboardShortcut);
}

bool HandleExtraBrowserKeyboardShortcut(NSEvent* event, NSWindow* window) {
  return HandleExtraKeyboardShortcut(event, window,
                                     CommandForBrowserKeyboardShortcut);
}

// Duplicate the given key event, but changing the associated window.
NSEvent* KeyEventForWindow(NSWindow* window, NSEvent* event) {
  NSEventType event_type = [event type];

  // Convert the event's location from the original window's coordinates into
  // our own.
  NSPoint location = [event locationInWindow];
  location = [[event window] convertBaseToScreen:location];
  location = [window convertScreenToBase:location];

  // Various things *only* apply to key down/up.
  bool is_a_repeat = false;
  NSString* characters = nil;
  NSString* charactors_ignoring_modifiers = nil;
  if (event_type == NSKeyDown || event_type == NSKeyUp) {
    is_a_repeat = [event isARepeat];
    characters = [event characters];
    charactors_ignoring_modifiers = [event charactersIgnoringModifiers];
  }

  // This synthesis may be slightly imperfect: we provide nil for the context,
  // since I (viettrungluu) am sceptical that putting in the original context
  // (if one is given) is valid.
  return [NSEvent keyEventWithType:event_type
                          location:location
                     modifierFlags:[event modifierFlags]
                         timestamp:[event timestamp]
                      windowNumber:[window windowNumber]
                           context:nil
                        characters:characters
       charactersIgnoringModifiers:charactors_ignoring_modifiers
                         isARepeat:is_a_repeat
                           keyCode:[event keyCode]];
}

}  // namespace

@implementation ChromeEventProcessingWindow

- (BOOL)handleExtraKeyboardShortcut:(NSEvent*)event {
  return HandleExtraBrowserKeyboardShortcut(event, self) ||
         HandleExtraWindowKeyboardShortcut(event, self) ||
         HandleDelayedWindowKeyboardShortcut(event, self);
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

  // Handle per-window shortcuts like cmd-1, but do not handle browser-level
  // shortcuts like cmd-left (else, cmd-left would do history navigation even
  // if e.g. the Omnibox has focus).
  if (HandleExtraWindowKeyboardShortcut(event, self))
    return YES;

  if ([super performKeyEquivalent:event])
    return YES;

  // Handle per-window shortcuts like Esc after giving everybody else a chance
  // to handle them
  return HandleDelayedWindowKeyboardShortcut(event, self);
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
    event = KeyEventForWindow(self, event);

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

@end  // ChromeEventProcessingWindow
