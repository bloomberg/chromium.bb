// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import <UIKit/UIKit.h>

#include "base/at_exit.h"
#include "base/debug/crash_logging.h"
#include "components/crash/core/common/crash_keys.h"
#include "ios/chrome/app/startup/ios_chrome_main.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/crash_report/crash_keys.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/clean/chrome/app/app_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

void StartCrashController() {
  std::string channel_string = GetChannelString();

  RegisterChromeIOSCrashKeys();
  base::debug::SetCrashKeyValue(crash_keys::kChannel, channel_string);
  breakpad_helper::Start(channel_string);
}

}  // namespace

int main(int argc, char* argv[]) {
  // Create the AtExitManager for the application before the crash controller
  // is started. This needs to be stack allocated and live for the lifetime of
  // the app.
  base::AtExitManager at_exit;

  IOSChromeMain::InitStartTime();

  // Pre-launch actions are in their own autorelease pool so they get cleaned
  // up before the main app starts.
  @autoreleasepool {
    NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];

    // Set NSUserDefaults keys to force pseudo-RTL if needed.
    if ([standardDefaults boolForKey:@"EnablePseudoRTL"]) {
      NSDictionary* pseudoDict = @{ @"YES" : @"AppleTextDirection" };
      [standardDefaults registerDefaults:pseudoDict];
    }

    // The crash controller is started here, even though the user may have opted
    // out; it needs to start as soon as possible to catch crashes during app
    // launch. Since user preferences aren't loaded yet, it isn't known if the
    // user has opted out. Once the preferences are loaded, the crash
    // controller is stopped for opted-out users, and any crash reports that
    // are collected before then are not sent.
    StartCrashController();

    // Always ignore SIGPIPE.  Chrome should always check the return value of
    // write() instead of relying on this signal.
    CHECK_NE(SIG_ERR, signal(SIGPIPE, SIG_IGN));
  }

  @autoreleasepool {
    return UIApplicationMain(argc, argv, nil,
                             NSStringFromClass([AppDelegate class]));
  }
}
