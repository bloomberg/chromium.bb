// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/chrome_browser_application_mac.h"

#include <objc/objc-exception.h>

#import "base/auto_reset.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#import "base/logging.h"
#include "base/mac/call_with_eh_frame.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_objc_class_swizzler.h"
#import "base/metrics/histogram.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/crash_keys.h"
#import "components/crash/core/common/objc_zombie.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace chrome_browser_application_mac {

// Maximum number of known named exceptions we'll support.  There is
// no central registration, but I only find about 75 possibilities in
// the system frameworks, and many of them are probably not
// interesting to track in aggregate (those relating to distributed
// objects, for instance).
const size_t kKnownNSExceptionCount = 25;

const size_t kUnknownNSException = kKnownNSExceptionCount;

size_t BinForException(NSException* exception) {
  // A list of common known exceptions.  The list position will
  // determine where they live in the histogram, so never move them
  // around, only add to the end.
  static NSString* const kKnownNSExceptionNames[] = {
    // Grab-bag exception, not very common.  CFArray (or other
    // container) mutated while being enumerated is one case seen in
    // production.
    NSGenericException,

    // Out-of-range on NSString or NSArray.  Quite common.
    NSRangeException,

    // Invalid arg to method, unrecognized selector.  Quite common.
    NSInvalidArgumentException,

    // malloc() returned null in object creation, I think.  Turns out
    // to be very uncommon in production, because of the OOM killer.
    NSMallocException,

    // This contains things like windowserver errors, trying to draw
    // views which aren't in windows, unable to read nib files.  By
    // far the most common exception seen on the crash server.
    NSInternalInconsistencyException,

    nil
  };

  // Make sure our array hasn't outgrown our abilities to track it.
  DCHECK_LE(arraysize(kKnownNSExceptionNames), kKnownNSExceptionCount);

  NSString* name = [exception name];
  for (int i = 0; kKnownNSExceptionNames[i]; ++i) {
    if (name == kKnownNSExceptionNames[i]) {
      return i;
    }
  }
  return kUnknownNSException;
}

void RecordExceptionWithUma(NSException* exception) {
  UMA_HISTOGRAM_ENUMERATION("OSX.NSException",
      BinForException(exception), kUnknownNSException);
}

namespace {

objc_exception_preprocessor g_next_preprocessor = nullptr;

id ExceptionPreprocessor(id exception) {
  static bool seen_first_exception = false;

  RecordExceptionWithUma(exception);

  const char* const kExceptionKey =
      seen_first_exception ? crash_keys::mac::kLastNSException
                           : crash_keys::mac::kFirstNSException;
  NSString* value = [NSString stringWithFormat:@"%@ reason %@",
      [exception name], [exception reason]];
  base::debug::SetCrashKeyValue(kExceptionKey, [value UTF8String]);

  const char* const kExceptionTraceKey =
      seen_first_exception ? crash_keys::mac::kLastNSExceptionTrace
                           : crash_keys::mac::kFirstNSExceptionTrace;
  // This exception preprocessor runs prior to the one in libobjc, which sets
  // the -[NSException callStackReturnAddresses].
  base::debug::SetCrashKeyToStackTrace(kExceptionTraceKey,
                                       base::debug::StackTrace());

  seen_first_exception = true;

  // Forward to the original version.
  if (g_next_preprocessor)
    return g_next_preprocessor(exception);
  return exception;
}

}  // namespace

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

// These methods are being exposed for the purposes of overriding.
// Used to determine when a Panel window can become the key window.
@interface NSApplication (PanelsCanBecomeKey)
- (void)_cycleWindowsReversed:(BOOL)arg1;
- (id)_removeWindow:(NSWindow*)window;
- (id)_setKeyWindow:(NSWindow*)window;
@end

@interface BrowserCrApplication (PrivateInternal)

// This must be called under the protection of previousKeyWindowsLock_.
- (void)removePreviousKeyWindow:(NSWindow*)window;

@end

@implementation BrowserCrApplication

+ (void)initialize {
  // Turn all deallocated Objective-C objects into zombies, keeping
  // the most recent 10,000 of them on the treadmill.
  ObjcEvilDoers::ZombieEnable(true, 10000);

  if (!chrome_browser_application_mac::g_next_preprocessor) {
    chrome_browser_application_mac::g_next_preprocessor =
        objc_setExceptionPreprocessor(
            &chrome_browser_application_mac::ExceptionPreprocessor);
  }
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

  return [super sendAction:anAction to:aTarget from:sender];
}

- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  base::mac::CallWithEHFrame(^{
    // tracked_objects::ScopedTracker does not support parameterized
    // instrumentations, so a big switch with each bunch instrumented is
    // required.
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
      // FALL THROUGH
      case NSLeftMouseUp:
      case NSRightMouseUp:
      case NSMouseMoved:
      case NSLeftMouseDragged:
      case NSRightMouseDragged:
      case NSMouseEntered:
      case NSMouseExited:
      case NSOtherMouseDown:
      case NSOtherMouseUp:
      case NSOtherMouseDragged: {
        tracked_objects::ScopedTracker tracking_profile(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "463272 -[BrowserCrApplication sendEvent:] Mouse"));
        base::mac::ScopedSendingEvent sendingEventScoper;
        [super sendEvent:event];
        break;
      }

      case NSKeyDown:
      case NSKeyUp: {
        tracked_objects::ScopedTracker tracking_profile(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "463272 -[BrowserCrApplication sendEvent:] Key"));
        base::mac::ScopedSendingEvent sendingEventScoper;
        [super sendEvent:event];
        break;
      }

      case NSScrollWheel: {
        tracked_objects::ScopedTracker tracking_profile(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "463272 -[BrowserCrApplication sendEvent:] ScrollWheel"));
        base::mac::ScopedSendingEvent sendingEventScoper;
        [super sendEvent:event];
        break;
      }

      case NSEventTypeGesture:
      case NSEventTypeMagnify:
      case NSEventTypeSwipe:
      case NSEventTypeRotate:
      case NSEventTypeBeginGesture:
      case NSEventTypeEndGesture: {
        tracked_objects::ScopedTracker tracking_profile(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "463272 -[BrowserCrApplication sendEvent:] Gesture"));
        base::mac::ScopedSendingEvent sendingEventScoper;
        [super sendEvent:event];
        break;
      }

      case NSAppKitDefined: {
        tracked_objects::ScopedTracker tracking_profile(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "463272 -[BrowserCrApplication sendEvent:] AppKit"));
        base::mac::ScopedSendingEvent sendingEventScoper;
        [super sendEvent:event];
        break;
      }

      case NSSystemDefined: {
        tracked_objects::ScopedTracker tracking_profile(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "463272 -[BrowserCrApplication sendEvent:] System"));
        base::mac::ScopedSendingEvent sendingEventScoper;
        [super sendEvent:event];
        break;
      }

      default: {
        tracked_objects::ScopedTracker tracking_profile(
            FROM_HERE_WITH_EXPLICIT_FUNCTION(
                "463272 -[BrowserCrApplication sendEvent:] Other"));
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

- (id)_removeWindow:(NSWindow*)window {
  // Note _removeWindow is called from -[NSWindow dealloc], which can happen at
  // unpredictable times due to reference counting. Just update state.
  {
    base::AutoLock lock(previousKeyWindowsLock_);
    [self removePreviousKeyWindow:window];
  }
  return [super _removeWindow:window];
}

- (id)_setKeyWindow:(NSWindow*)window {
  // |window| is nil when the current key window is being closed.
  // A separate call follows with a new value when a new key window is set.
  // Closed windows are not tracked in previousKeyWindows_.
  if (window != nil) {
    base::AutoLock lock(previousKeyWindowsLock_);
    [self removePreviousKeyWindow:window];
    NSWindow* currentKeyWindow = [self keyWindow];
    if (currentKeyWindow != nil && currentKeyWindow != window)
      previousKeyWindows_.push_back(currentKeyWindow);
  }

  return [super _setKeyWindow:window];
}

- (NSWindow*)previousKeyWindow {
  base::AutoLock lock(previousKeyWindowsLock_);
  return previousKeyWindows_.empty() ? nil : previousKeyWindows_.back();
}

- (void)removePreviousKeyWindow:(NSWindow*)window {
  previousKeyWindowsLock_.AssertAcquired();
  std::vector<NSWindow*>::iterator window_iterator =
      std::find(previousKeyWindows_.begin(),
                previousKeyWindows_.end(),
                window);
  if (window_iterator != previousKeyWindows_.end()) {
    previousKeyWindows_.erase(window_iterator);
  }
}

@end
