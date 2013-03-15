// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/scoped_nsobject.h"
#include "base/message_loop.h"
#include "base/message_pump_default.h"
#include "base/test/test_suite.h"

// Springboard will kill any iOS app that fails to check in after launch within
// a given time. Starting a UIApplication before invoking TestSuite::Run
// prevents this from happening.

// InitIOSRunHook saves the TestSuite and argc/argv, then invoking
// RunTestsFromIOSApp calls UIApplicationMain(), providing an application
// delegate class: ChromeUnitTestDelegate. The delegate implements
// application:didFinishLaunchingWithOptions: to invoke the TestSuite's Run
// method.

// Since the executable isn't likely to be a real iOS UI, the delegate puts up a
// window displaying the app name. If a bunch of apps using MainHook are being
// run in a row, this provides an indication of which one is currently running.

static base::TestSuite* g_test_suite = NULL;
static int g_argc;
static char** g_argv;

@interface UIApplication (Testing)
- (void) _terminateWithStatus:(int)status;
@end

@interface ChromeUnitTestDelegate : NSObject {
 @private
  scoped_nsobject<UIWindow> window_;
}
- (void)runTests;
@end

@implementation ChromeUnitTestDelegate

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {

  CGRect bounds = [[UIScreen mainScreen] bounds];

  // Yes, this is leaked, it's just to make what's running visible.
  window_.reset([[UIWindow alloc] initWithFrame:bounds]);
  [window_ makeKeyAndVisible];

  // Add a label with the app name.
  UILabel* label = [[[UILabel alloc] initWithFrame:bounds] autorelease];
  label.text = [[NSProcessInfo processInfo] processName];
  label.textAlignment = UITextAlignmentCenter;
  [window_ addSubview:label];

  // Queue up the test run.
  [self performSelector:@selector(runTests)
             withObject:nil
             afterDelay:0.1];
  return YES;
}

- (void)runTests {
  int exitStatus = g_test_suite->Run();

  // If a test app is too fast, it will exit before Instruments has has a
  // a chance to initialize and no test results will be seen.
  // TODO(ios): crbug.com/137010 Figure out how much time is actually needed,
  // and sleep only to make sure that much time has elapsed since launch.
  [NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow:2.0]];
  window_.reset();

  // Use the hidden selector to try and cleanly take down the app (otherwise
  // things can think the app crashed even on a zero exit status).
  UIApplication* application = [UIApplication sharedApplication];
  [application _terminateWithStatus:exitStatus];

  exit(exitStatus);
}

@end

namespace {

base::MessagePump* CreateMessagePumpForUIForTests() {
  // A default MessagePump will do quite nicely in tests.
  return new base::MessagePumpDefault();
}

}  // namespace

namespace base {

void InitIOSTestMessageLoop() {
  MessageLoop::InitMessagePumpForUIFactory(&CreateMessagePumpForUIForTests);
}

void InitIOSRunHook(TestSuite* suite, int argc, char* argv[]) {
  g_test_suite = suite;
  g_argc = argc;
  g_argv = argv;
}

void RunTestsFromIOSApp() {
  // When TestSuite::Run is invoked it calls RunTestsFromIOSApp(). On the first
  // invocation, this method fires up an iOS app via UIApplicationMain. Since
  // UIApplicationMain does not return until the app exits, control does not
  // return to the initial TestSuite::Run invocation, so the app invokes
  // TestSuite::Run a second time and since |ran_hook| is true at this point,
  // this method is a no-op and control returns to TestSuite:Run so that test
  // are executed. Once the app exits, RunTestsFromIOSApp calls exit() so that
  // control is not returned to the initial invocation of TestSuite::Run.
  static bool ran_hook = false;
  if (!ran_hook) {
    ran_hook = true;
    mac::ScopedNSAutoreleasePool pool;
    int exit_status = UIApplicationMain(g_argc, g_argv, nil,
                                        @"ChromeUnitTestDelegate");
    exit(exit_status);
  }
}

}  // namespace base
