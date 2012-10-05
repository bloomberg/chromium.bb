// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the penultimate pieces of the Mac shutdown puzzle. For
// an in-depth overview of the Mac shutdown path, see the comment above
// -[BrowserCrApplication terminate:].

#include "chrome/browser/lifetime/application_lifetime.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/browser_shutdown.h"
#import "chrome/browser/chrome_browser_application_mac.h"

namespace browser {

namespace {

// A closure that is used to finalize application shutdown.
void PerformTerminate() {
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSApplicationWillTerminateNotification
                    object:NSApp];
}

}  // namespace

// At this point, the user is trying to quit (or the system is forcing the
// application to quit) and all browsers have been successfully closed. The
// final step in shutdown is to post the NSApplicationWillTerminateNotification
// to end the -[NSApplication run] event loop. That is performed via the
// MessageLoop, since this can be called via the destruction path of the last
// Browser object.
void HandleAppExitingForPlatform() {
  static bool kill_me_now = false;
  CHECK(!kill_me_now);
  kill_me_now = true;

  MessageLoop::current()->PostNonNestableTask(FROM_HERE,
      base::Bind(&PerformTerminate));
}

}  // namespace browser
