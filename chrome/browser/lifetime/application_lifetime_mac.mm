// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the penultimate pieces of the Mac shutdown puzzle. For
// an in-depth overview of the Mac shutdown path, see the comment above
// -[BrowserCrApplication terminate:].

#include "chrome/browser/lifetime/application_lifetime.h"

#include "base/logging.h"
#include "chrome/browser/browser_shutdown.h"
#import "chrome/browser/chrome_browser_application_mac.h"

namespace chrome {

// At this point, the user is trying to quit (or the system is forcing the
// application to quit) and all browsers have been successfully closed. The
// final step in shutdown is to post the NSApplicationWillTerminateNotification
// to end the -[NSApplication run] event loop.
void HandleAppExitingForPlatform() {
  static bool kill_me_now = false;
  CHECK(!kill_me_now);
  kill_me_now = true;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSApplicationWillTerminateNotification
                    object:NSApp];
}

}  // namespace chrome
