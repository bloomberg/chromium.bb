// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/chrome_browser_application_mac.h"

#import "base/auto_reset.h"
#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#import "base/logging.h"
#import "base/mac/scoped_nsexception_enabler.h"
#import "base/mac/scoped_nsobject.h"
#import "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/crash_keys.h"
#import "chrome/common/mac/objc_method_swizzle.h"
#import "chrome/common/mac/objc_zombie.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace {

// Tracking for cases being hit by -crInitWithName:reason:userInfo:.
enum ExceptionEventType {
  EXCEPTION_ACCESSIBILITY = 0,
  EXCEPTION_MENU_ITEM_BOUNDS_CHECK,
  EXCEPTION_VIEW_NOT_IN_WINDOW,
  EXCEPTION_NSURL_INIT_NIL,
  EXCEPTION_NSDATADETECTOR_NIL_STRING,

  // Always keep this at the end.
  EXCEPTION_MAX,
};

void RecordExceptionEvent(ExceptionEventType event_type) {
  UMA_HISTOGRAM_ENUMERATION("OSX.ExceptionHandlerEvents",
                            event_type, EXCEPTION_MAX);
}

}  // namespace

// The implementation of NSExceptions break various assumptions in the
// Chrome code.  This category defines a replacement for
// -initWithName:reason:userInfo: for purposes of forcing a break in
// the debugger when an exception is raised.  -raise sounds more
// obvious to intercept, but it doesn't catch the original throw
// because the objc runtime doesn't use it.
@interface NSException (CrNSExceptionSwizzle)
- (id)crInitWithName:(NSString*)aName
              reason:(NSString*)aReason
            userInfo:(NSDictionary*)someUserInfo;
@end

static IMP gOriginalInitIMP = NULL;

@implementation NSException (CrNSExceptionSwizzle)
- (id)crInitWithName:(NSString*)aName
              reason:(NSString*)aReason
            userInfo:(NSDictionary*)someUserInfo {
  // Method only called when swizzled.
  DCHECK(_cmd == @selector(initWithName:reason:userInfo:));

  // Parts of Cocoa rely on creating and throwing exceptions. These are not
  // worth bugging-out over. It is very important that there be zero chance that
  // any Chromium code is on the stack; these must be created by Apple code and
  // then immediately consumed by Apple code.
  static NSString* const kAcceptableNSExceptionNames[] = {
    // If an object does not support an accessibility attribute, this will
    // get thrown.
    NSAccessibilityException,
  };

  BOOL found = NO;
  for (size_t i = 0; i < arraysize(kAcceptableNSExceptionNames); ++i) {
    if (aName == kAcceptableNSExceptionNames[i]) {
      found = YES;
      RecordExceptionEvent(EXCEPTION_ACCESSIBILITY);
      break;
    }
  }

  if (!found) {
    // Update breakpad with the exception info.
    std::string value = base::StringPrintf("%s reason %s",
        [aName UTF8String], [aReason UTF8String]);
    base::debug::SetCrashKeyValue(crash_keys::mac::kNSException, value);
    base::debug::SetCrashKeyToStackTrace(crash_keys::mac::kNSExceptionTrace,
                                         base::debug::StackTrace());

    // Force crash for selected exceptions to generate crash dumps.
    BOOL fatal = NO;
    if (aName == NSInternalInconsistencyException) {
      NSString* const kNSMenuItemArrayBoundsCheck =
          @"Invalid parameter not satisfying: (index >= 0) && "
          @"(index < [_itemArray count])";
      if ([aReason isEqualToString:kNSMenuItemArrayBoundsCheck]) {
        RecordExceptionEvent(EXCEPTION_MENU_ITEM_BOUNDS_CHECK);
        fatal = YES;
      }

      NSString* const kNoWindowCheck = @"View is not in any window";
      if ([aReason isEqualToString:kNoWindowCheck]) {
        RecordExceptionEvent(EXCEPTION_VIEW_NOT_IN_WINDOW);
        fatal = YES;
      }
    }

    // Mostly "unrecognized selector sent to (instance|class)".  A
    // very small number of things like inappropriate nil being passed.
    if (aName == NSInvalidArgumentException) {
      fatal = YES;

      // TODO(shess): http://crbug.com/85463 throws this exception
      // from ImageKit.  Our code is not on the stack, so it needs to
      // be whitelisted for now.
      NSString* const kNSURLInitNilCheck =
          @"*** -[NSURL initFileURLWithPath:isDirectory:]: "
          @"nil string parameter";
      if ([aReason isEqualToString:kNSURLInitNilCheck]) {
        RecordExceptionEvent(EXCEPTION_NSURL_INIT_NIL);
        fatal = NO;
      }

      // TODO(shess): <http://crbug.com/316759> OSX 10.9 is failing
      // trying to extract structure from a string.
      NSString* const kNSDataDetectorNilCheck =
          @"*** -[NSDataDetector enumerateMatchesInString:"
          @"options:range:usingBlock:]: nil argument";
      if ([aReason isEqualToString:kNSDataDetectorNilCheck]) {
        RecordExceptionEvent(EXCEPTION_NSDATADETECTOR_NIL_STRING);
        fatal = NO;
      }
    }

    // Dear reader: Something you just did provoked an NSException.
    // NSException is implemented in terms of setjmp()/longjmp(),
    // which does poor things when combined with C++ scoping
    // (destructors are skipped).  Chrome should be NSException-free,
    // please check your backtrace and see if you can't file a bug
    // with a repro case.
    const bool allow = base::mac::GetNSExceptionsAllowed();
    if (fatal && !allow) {
      LOG(FATAL) << "Someone is trying to raise an exception!  "
                 << value;
    } else {
      // Make sure that developers see when their code throws
      // exceptions.
      DCHECK(allow) << "Someone is trying to raise an exception!  "
                    << value;
    }
  }

  // Forward to the original version.
  return gOriginalInitIMP(self, _cmd, aName, aReason, someUserInfo);
}
@end

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

namespace {

void SwizzleInit() {
  // Do-nothing wrapper so that we can arrange to only swizzle
  // -[NSException raise] when DCHECK() is turned on (as opposed to
  // replicating the preprocess logic which turns DCHECK() on).
  gOriginalInitIMP = ObjcEvilDoers::SwizzleImplementedInstanceMethods(
      [NSException class],
      @selector(initWithName:reason:userInfo:),
      @selector(crInitWithName:reason:userInfo:));
}

}  // namespace

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
}

- (id)init {
  SwizzleInit();
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

  // Certain third-party code, such as print drivers, can still throw
  // exceptions and Chromium cannot fix them.  This provides a way to
  // work around those on a spot basis.
  bool enableNSExceptions = false;

  // http://crbug.com/80686 , an Epson printer driver.
  if (anAction == @selector(selectPDE:)) {
    enableNSExceptions = true;
  }

  // Minimize the window by keeping this close to the super call.
  scoped_ptr<base::mac::ScopedNSExceptionEnabler> enabler;
  if (enableNSExceptions)
    enabler.reset(new base::mac::ScopedNSExceptionEnabler());
  return [super sendAction:anAction to:aTarget from:sender];
}

- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  base::mac::ScopedSendingEvent sendingEventScoper;
  [super sendEvent:event];
}

// NSExceptions which are caught by the event loop are logged here.
// NSException uses setjmp/longjmp, which can be very bad for C++, so
// we attempt to track and report them.
- (void)reportException:(NSException *)anException {
  // If we throw an exception in this code, we can create an infinite
  // loop.  If we throw out of the if() without resetting
  // |reportException|, we'll stop reporting exceptions for this run.
  static BOOL reportingException = NO;
  DCHECK(!reportingException);
  if (!reportingException) {
    reportingException = YES;
    chrome_browser_application_mac::RecordExceptionWithUma(anException);

    // http://crbug.com/45928 is a bug about needing to double-close
    // windows sometimes.  One theory is that |-isHandlingSendEvent|
    // gets latched to always return |YES|.  Since scopers are used to
    // manipulate that value, that should not be possible.  One way to
    // sidestep scopers is setjmp/longjmp (see above).  The following
    // is to "fix" this while the more fundamental concern is
    // addressed elsewhere.
    [self setHandlingSendEvent:NO];

    // If |ScopedNSExceptionEnabler| is used to allow exceptions, and an
    // uncaught exception is thrown, it will throw past all of the scopers.
    // Reset the flag so that future exceptions are not masked.
    base::mac::SetNSExceptionsAllowed(false);

    // Store some human-readable information in breakpad keys in case
    // there is a crash.  Since breakpad does not provide infinite
    // storage, we track two exceptions.  The first exception thrown
    // is tracked because it may be the one which caused the system to
    // go off the rails.  The last exception thrown is tracked because
    // it may be the one most directly associated with the crash.
    static BOOL trackedFirstException = NO;

    const char* const kExceptionKey =
        trackedFirstException ? crash_keys::mac::kLastNSException
                              : crash_keys::mac::kFirstNSException;
    NSString* value = [NSString stringWithFormat:@"%@ reason %@",
                                [anException name], [anException reason]];
    base::debug::SetCrashKeyValue(kExceptionKey, [value UTF8String]);

    // Encode the callstack from point of throw.
    // TODO(shess): Our swizzle plus the 23-frame limit plus Cocoa
    // overhead may make this less than useful.  If so, perhaps skip
    // some items and/or use two keys.
    const char* const kExceptionBtKey =
        trackedFirstException ? crash_keys::mac::kLastNSExceptionTrace
                              : crash_keys::mac::kFirstNSExceptionTrace;
    NSArray* addressArray = [anException callStackReturnAddresses];
    NSUInteger addressCount = [addressArray count];
    if (addressCount) {
      // SetCrashKeyFromAddresses() only encodes 23, so that's a natural limit.
      const NSUInteger kAddressCountMax = 23;
      void* addresses[kAddressCountMax];
      if (addressCount > kAddressCountMax)
        addressCount = kAddressCountMax;

      for (NSUInteger i = 0; i < addressCount; ++i) {
        addresses[i] = reinterpret_cast<void*>(
            [[addressArray objectAtIndex:i] unsignedIntegerValue]);
      }
      base::debug::SetCrashKeyFromAddresses(
          kExceptionBtKey, addresses, static_cast<size_t>(addressCount));
    } else {
      base::debug::ClearCrashKey(kExceptionBtKey);
    }
    trackedFirstException = YES;

    reportingException = NO;
  }

  [super reportException:anException];
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  if ([attribute isEqualToString:@"AXEnhancedUserInterface"] &&
      [value intValue] == 1) {
    content::BrowserAccessibilityState::GetInstance()->OnScreenReaderDetected();
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
  {
    base::AutoLock lock(previousKeyWindowsLock_);
    [self removePreviousKeyWindow:window];
  }
  id result = [super _removeWindow:window];

  // Ensure app has a key window after a window is removed.
  // OS wants to make a panel browser window key after closing an app window
  // because panels use a higher priority window level, but panel windows may
  // refuse to become key, leaving the app with no key window. The OS does
  // not seem to consider other windows after the first window chosen refuses
  // to become key. Force consideration of other windows here.
  if ([self isActive] && [self keyWindow] == nil) {
    NSWindow* key =
        [self makeWindowsPerform:@selector(canBecomeKeyWindow) inOrder:YES];
    [key makeKeyWindow];
  }

  // Return result from the super class. It appears to be the app that
  // owns the removed window (determined via experimentation).
  return result;
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
