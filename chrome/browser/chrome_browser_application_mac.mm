// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/chrome_browser_application_mac.h"

#import "base/logging.h"
#include "base/mac/crash_logging.h"
#import "base/mac/scoped_nsexception_enabler.h"
#import "base/metrics/histogram.h"
#import "base/memory/scoped_nsobject.h"
#import "base/sys_string_conversions.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#import "chrome/common/mac/objc_method_swizzle.h"
#import "chrome/common/mac/objc_zombie.h"
#include "content/browser/accessibility/browser_accessibility_state.h"
#include "content/browser/renderer_host/render_view_host.h"

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

    nil
  };

  BOOL found = NO;
  for (int i = 0; kAcceptableNSExceptionNames[i]; ++i) {
    if (aName == kAcceptableNSExceptionNames[i]) {
      found = YES;
    }
  }

  if (!found) {
    // Update breakpad with the exception info.
    static NSString* const kNSExceptionKey = @"nsexception";
    NSString* value =
        [NSString stringWithFormat:@"%@ reason %@", aName, aReason];
    base::mac::SetCrashKeyValue(kNSExceptionKey, value);

    // Force crash for selected exceptions to generate crash dumps.
    BOOL fatal = NO;
    if (aName == NSInternalInconsistencyException) {
      NSString* const kNSMenuItemArrayBoundsCheck =
          @"Invalid parameter not satisfying: (index >= 0) && "
          @"(index < [_itemArray count])";
      if ([aReason isEqualToString:kNSMenuItemArrayBoundsCheck]) {
        fatal = YES;
      }

      NSString* const kNoWindowCheck = @"View is not in any window";
      if ([aReason isEqualToString:kNoWindowCheck]) {
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
                 << base::SysNSStringToUTF8(value);
    } else {
      // Make sure that developers see when their code throws
      // exceptions.
      DLOG(ERROR) << "Someone is trying to raise an exception!  "
                  << base::SysNSStringToUTF8(value);
      DCHECK(allow);
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

@implementation BrowserCrApplication

+ (void)initialize {
  // Turn all deallocated Objective-C objects into zombies, keeping
  // the most recent 10,000 of them on the treadmill.
  ObjcEvilDoers::ZombieEnable(true, 10000);
}

- (id)init {
  SwizzleInit();
  if ((self = [super init])) {
    eventHooks_.reset([[NSMutableArray alloc] init]);
  }
  return self;
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
- (void)terminate:(id)sender {
  AppController* appController = static_cast<AppController*>([NSApp delegate]);
  if ([appController tryToTerminateApplication:self]) {
    [[NSNotificationCenter defaultCenter]
        postNotificationName:NSApplicationWillTerminateNotification
                      object:self];
  }

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
  static NSString* const kActionKey = @"sendaction";

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
  NSString* value =
        [NSString stringWithFormat:@"%@ tag %d sending %@ to %p",
                  [sender className], tag, actionString, aTarget];

  base::mac::ScopedCrashKey key(kActionKey, value);

  // Certain third-party code, such as print drivers, can still throw
  // exceptions and Chromium cannot fix them.  This provides a way to
  // work around those on a spot basis.
  bool enableNSExceptions = false;

  // http://crbug.com/80686 , an Epson printer driver.
  if (anAction == @selector(selectPDE:)) {
    enableNSExceptions = true;
  }

  // Minimize the window by keeping this close to the super call.
  scoped_ptr<base::mac::ScopedNSExceptionEnabler> enabler(NULL);
  if (enableNSExceptions)
    enabler.reset(new base::mac::ScopedNSExceptionEnabler());
  return [super sendAction:anAction to:aTarget from:sender];
}

- (void)addEventHook:(id<CrApplicationEventHookProtocol>)handler {
  [eventHooks_ addObject:handler];
}

- (void)removeEventHook:(id<CrApplicationEventHookProtocol>)handler {
  [eventHooks_ removeObject:handler];
}

- (void)sendEvent:(NSEvent*)event {
  base::mac::ScopedSendingEvent sendingEventScoper;
  for (id<CrApplicationEventHookProtocol> handler in eventHooks_.get()) {
    [handler hookForEvent:event];
  }
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
    [self clearIsHandlingSendEvent];

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
    static NSString* const kFirstExceptionKey = @"firstexception";
    static BOOL trackedFirstException = NO;
    static NSString* const kLastExceptionKey = @"lastexception";

    // TODO(shess): It would be useful to post some stacktrace info
    // from the exception.
    // 10.6 has -[NSException callStackSymbols]
    // 10.5 has -[NSException callStackReturnAddresses]
    // 10.5 has backtrace_symbols().
    // I've tried to combine the latter two, but got nothing useful.
    // The addresses are right, though, maybe we could train the crash
    // server to decode them for us.

    NSString* value = [NSString stringWithFormat:@"%@ reason %@",
                                [anException name], [anException reason]];
    if (!trackedFirstException) {
      base::mac::SetCrashKeyValue(kFirstExceptionKey, value);
      trackedFirstException = YES;
    } else {
      base::mac::SetCrashKeyValue(kLastExceptionKey, value);
    }

    reportingException = NO;
  }

  [super reportException:anException];
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  if ([attribute isEqualToString:@"AXEnhancedUserInterface"] &&
      [value intValue] == 1) {
    BrowserAccessibilityState::GetInstance()->OnScreenReaderDetected();
    for (TabContentsIterator it;
         !it.done();
         ++it) {
      if (TabContentsWrapper* contents = *it) {
        if (RenderViewHost* rvh = contents->render_view_host()) {
          rvh->EnableRendererAccessibility();
        }
      }
    }
  }
  return [super accessibilitySetValue:value forAttribute:attribute];
}

@end
