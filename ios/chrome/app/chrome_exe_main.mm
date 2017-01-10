// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/at_exit.h"
#include "base/debug/crash_logging.h"
#include "components/crash/core/common/crash_keys.h"
#include "ios/chrome/app/startup/ios_chrome_main.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/crash_report/crash_keys.h"
#include "ios/chrome/common/channel_info.h"

namespace {

NSString* const kUIApplicationDelegateInfoKey = @"UIApplicationDelegate";

void StartCrashController() {
  std::string channel_string = GetChannelString();

  RegisterChromeIOSCrashKeys();
  base::debug::SetCrashKeyValue(crash_keys::kChannel, channel_string);
  breakpad_helper::Start(channel_string);
}

}  // namespace

int main(int argc, char* argv[]) {
  IOSChromeMain::InitStartTime();
  NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];

  // Set NSUserDefaults keys to force pseudo-RTL if needed.
  if ([standardDefaults boolForKey:@"EnablePseudoRTL"]) {
    NSDictionary* pseudoDict = @{ @"YES" : @"AppleTextDirection" };
    [standardDefaults registerDefaults:pseudoDict];
  }

  // Create this here since it's needed to start the crash handler.
  base::AtExitManager at_exit;

  // The Crash Controller is started here even if the user opted out since we
  // don't have yet preferences. Later on it is stopped if the user opted out.
  // In any case reports are not sent if the user opted out.
  StartCrashController();

  // Always ignore SIGPIPE.  We check the return value of write().
  CHECK_NE(SIG_ERR, signal(SIGPIPE, SIG_IGN));

  // Purging the pool to prevent autorelease objects created by the previous
  // calls to live forever.
  [pool release];
  pool = [[NSAutoreleasePool alloc] init];

  // Part of code that requires us to specify which UIApplication delegate class
  // to use by adding "UIApplicationDelegate" key to Info.plist file.
  NSString* delegateClassName = [[NSBundle mainBundle]
      objectForInfoDictionaryKey:kUIApplicationDelegateInfoKey];
  CHECK(delegateClassName);

  int retVal = UIApplicationMain(argc, argv, nil, delegateClassName);
  [pool release];
  return retVal;
}
