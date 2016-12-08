// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/chrome_browser_application_mac.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/mac/call_with_eh_frame.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/trace_event/trace_event.h"
#import "chrome/browser/app_controller_mac.h"
#import "chrome/browser/mac/exception_processor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#import "components/crash/core/common/objc_zombie.h"
#include "content/public/browser/browser_accessibility_state.h"

namespace chrome_browser_application_mac {

void RegisterBrowserCrApp() {
  [BrowserCrApplication sharedApplication];
};

void Terminate() {
  [NSApp terminate:nil];
}

void CancelTerminate() {
  [NSApp cancelTerminate:nil];
}

}  // namespace chrome_browser_application_mac

// Method exposed for the purposes of overriding.
// Used to determine when a Panel window can become the key window.
@interface NSApplication (PanelsCanBecomeKey)
- (void)_cycleWindowsReversed:(BOOL)arg1;
@end

@implementation BrowserCrApplication

+ (void)initialize {
  // Turn all deallocated Objective-C objects into zombies, keeping
  // the most recent 10,000 of them on the treadmill.
  ObjcEvilDoers::ZombieEnable(true, 10000);

  chrome::InstallObjcExceptionPreprocessor();
}

- (id)init {
  self = [super init];

  // Sanity check to alert if overridden methods are not supported.
  DCHECK([NSApplication
      instancesRespondToSelector:@selector(_cycleWindowsReversed:)]);
  DCHECK([NSApplication
      instancesRespondToSelector:@selector(_removeWindow:)]);
  DCHECK([NSApplication
      instancesRespondToSelector:@selector(_setKeyWindow:)]);

  return self;
}

// Initialize NSApplication using the custom subclass.  Check whether NSApp
// was already initialized using another class, because that would break
// some things.
+ (NSApplication*)sharedApplication {
  NSApplication* app = [super sharedApplication];

  // +sharedApplication initializes the global NSApp, so if a specific
  // NSApplication subclass is requested, require that to be the one
  // delivered.  The practical effect is to require a consistent NSApp
  // across the executable.
  CHECK([NSApp isKindOfClass:self])
      << "NSApp must be of type " << [[self className] UTF8String]
      << ", not " << [[NSApp className] UTF8String];

  // If the message loop was initialized before NSApp is setup, the
  // message pump will be setup incorrectly.  Failing this implies
  // that RegisterBrowserCrApp() should be called earlier.
  CHECK(base::MessagePumpMac::UsingCrApp())
      << "MessagePumpMac::Create() is using the wrong pump implementation"
      << " for " << [[self className] UTF8String];

  return app;
}

////////////////////////////////////////////////////////////////////////////////
// HISTORICAL COMMENT (by viettrungluu, from
// http://codereview.chromium.org/1520006 with mild editing):
//
// A quick summary of the state of things (before the changes to shutdown):
//
// Currently, we are totally hosed (put in a bad state in which Cmd-W does the
// wrong thing, and which will probably eventually lead to a crash) if we begin
// quitting but termination is aborted for some reason.
//
// I currently know of two ways in which termination can be aborted:
// (1) Common case: a window has an onbeforeunload handler which pops up a
//     "leave web page" dialog, and the user answers "no, don't leave".
// (2) Uncommon case: popups are enabled (in Content Settings, i.e., the popup
//     blocker is disabled), and some nasty web page pops up a new window on
//     closure.
//
// I don't know of other ways in which termination can be aborted, but they may
// exist (or may be added in the future, for that matter).
//
// My CL [see above] does the following:
// a. Should prevent being put in a bad state (which breaks Cmd-W and leads to
//    crash) under all circumstances.
// b. Should completely handle (1) properly.
// c. Doesn't (yet) handle (2) properly and puts it in a weird state (but not
//    that bad).
// d. Any other ways of aborting termination would put it in that weird state.
//
// c. can be fixed by having the global flag reset on browser creation or
// similar (and doing so might also fix some possible d.'s as well). I haven't
// done this yet since I haven't thought about it carefully and since it's a
// corner case.
//
// The weird state: a state in which closing the last window quits the browser.
// This might be a bit annoying, but it's not dangerous in any way.
////////////////////////////////////////////////////////////////////////////////

// |-terminate:| is the entry point for orderly "quit" operations in Cocoa. This
// includes the application menu's quit menu item and keyboard equivalent, the
// application's dock icon menu's quit menu item, "quit" (not "force quit") in
// the Activity Monitor, and quits triggered by user logout and system restart
// and shutdown.
//
// The default |-terminate:| implementation ends the process by calling exit(),
// and thus never leaves the main run loop. This is unsuitable for Chrome since
// Chrome depends on leaving the main run loop to perform an orderly shutdown.
// We support the normal |-terminate:| interface by overriding the default
// implementation. Our implementation, which is very specific to the needs of
// Chrome, works by asking the application delegate to terminate using its
// |-tryToTerminateApplication:| method.
//
// |-tryToTerminateApplication:| differs from the standard
// |-applicationShouldTerminate:| in that no special event loop is run in the
// case that immediate termination is not possible (e.g., if dialog boxes
// allowing the user to cancel have to be shown). Instead, this method sets a
// flag and tries to close all browsers. This flag causes the closure of the
// final browser window to begin actual tear-down of the application.
// Termination is cancelled by resetting this flag. The standard
// |-applicationShouldTerminate:| is not supported, and code paths leading to it
// must be redirected.
//
// When the last browser has been destroyed, the BrowserList calls
// chrome::OnAppExiting(), which is the point of no return. That will cause
// the NSApplicationWillTerminateNotification to be posted, which ends the
// NSApplication event loop, so final post- MessageLoop::Run() work is done
// before exiting.
- (void)terminate:(id)sender {
  AppController* appController = static_cast<AppController*>([NSApp delegate]);
  [appController tryToTerminateApplication:self];
  // Return, don't exit. The application is responsible for exiting on its own.
}

- (void)cancelTerminate:(id)sender {
  AppController* appController = static_cast<AppController*>([NSApp delegate]);
  [appController stopTryingToTerminateApplication:self];
}

// The event |mask| has historically been declared as an NSUInteger
// (unsigned long). Starting in the 10.12 SDK, the mask type changed to
// NSEventMask (unsigned long long) if __LP64__ and NSUInteger otherwise.
// These types are incompatible, which creates an issue for suppporting
// both 10.10/10.11 and 10.12 SDKs. Work around it using the #if below.
- (NSEvent*)nextEventMatchingMask:
#if !defined(MAC_OS_X_VERSION_10_12) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12 || \
    !defined(__LP64__)
                                  (NSUInteger)mask
#else
                                  (NSEventMask)mask
#endif
                        untilDate:(NSDate*)expiration
                           inMode:(NSString*)mode
                          dequeue:(BOOL)dequeue {
  __block NSEvent* event = nil;
  base::mac::CallWithEHFrame(^{
      event = [super nextEventMatchingMask:mask
                                 untilDate:expiration
                                    inMode:mode
                                   dequeue:dequeue];
  });
  return event;
}

- (BOOL)sendAction:(SEL)anAction to:(id)aTarget from:(id)sender {
  // The Dock menu contains an automagic section where you can select
  // amongst open windows.  If a window is closed via JavaScript while
  // the menu is up, the menu item for that window continues to exist.
  // When a window is selected this method is called with the
  // now-freed window as |aTarget|.  Short-circuit the call if
  // |aTarget| is not a valid window.
  if (anAction == @selector(_selectWindow:)) {
    // Not using -[NSArray containsObject:] because |aTarget| may be a
    // freed object.
    BOOL found = NO;
    for (NSWindow* window in [self windows]) {
      if (window == aTarget) {
        found = YES;
        break;
      }
    }
    if (!found) {
      return NO;
    }
  }

  // When a Cocoa control is wired to a freed object, we get crashers
  // in the call to |super| with no useful information in the
  // backtrace.  Attempt to add some useful information.

  // If the action is something generic like -commandDispatch:, then
  // the tag is essential.
  NSInteger tag = 0;
  if ([sender isKindOfClass:[NSControl class]]) {
    tag = [sender tag];
    if (tag == 0 || tag == -1) {
      tag = [sender selectedTag];
    }
  } else if ([sender isKindOfClass:[NSMenuItem class]]) {
    tag = [sender tag];
  }

  NSString* actionString = NSStringFromSelector(anAction);
  std::string value = base::StringPrintf("%s tag %ld sending %s to %p",
      [[sender className] UTF8String],
      static_cast<long>(tag),
      [actionString UTF8String],
      aTarget);
  base::debug::ScopedCrashKey key(crash_keys::mac::kSendAction, value);

  __block BOOL rv;
  base::mac::CallWithEHFrame(^{
    rv = [super sendAction:anAction to:aTarget from:sender];
  });
  return rv;
}

- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  TRACE_EVENT0("toplevel", "BrowserCrApplication::sendEvent");
  base::debug::ScopedCrashKey crash_key(
      crash_keys::mac::kNSEvent, base::SysNSStringToUTF8([event description]));

  base::mac::CallWithEHFrame(^{
    switch (event.type) {
      case NSLeftMouseDown:
      case NSRightMouseDown: {
        // In kiosk mode, we want to prevent context menus from appearing,
        // so simply discard menu-generating events instead of passing them
        // along.
        bool kioskMode = base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kKioskMode);
        bool ctrlDown = [event modifierFlags] & NSControlKeyMask;
        if (kioskMode && ([event type] == NSRightMouseDown || ctrlDown))
          break;
      }

      default: {
        base::mac::ScopedSendingEvent sendingEventScoper;
        [super sendEvent:event];
      }
    }
  });
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  // This is an undocument attribute that's set when VoiceOver is turned on/off.
  if ([attribute isEqualToString:@"AXEnhancedUserInterface"]) {
    content::BrowserAccessibilityState* accessibility_state =
        content::BrowserAccessibilityState::GetInstance();
    if ([value intValue] == 1)
      accessibility_state->OnScreenReaderDetected();
    else
      accessibility_state->DisableAccessibility();
  }
  return [super accessibilitySetValue:value forAttribute:attribute];
}

- (void)_cycleWindowsReversed:(BOOL)arg1 {
  base::AutoReset<BOOL> pin(&cyclingWindows_, YES);
  [super _cycleWindowsReversed:arg1];
}

- (BOOL)isCyclingWindows {
  return cyclingWindows_;
}

@end
