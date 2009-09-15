// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/chrome_application_mac.h"

@implementation CrApplication

// -terminate: is the entry point for orderly "quit" operations in Cocoa.
// This includes the application menu's quit menu item and keyboard
// equivalent, the application's dock icon menu's quit menu item, "quit" (not
// "force quit") in the Activity Monitor, and quits triggered by user logout
// and system restart and shutdown.
//
// The default NSApplication -terminate: implementation will end the process
// by calling exit(), and thus never leave the main run loop.  This is
// unsuitable for Chrome's purposes.  Chrome depends on leaving the main
// run loop to perform a proper orderly shutdown.  This design is ingrained
// in the application and the assumptions that its code makes, and is
// entirely reasonable and works well on other platforms, but it's not
// compatible with the standard Cocoa quit sequence.  Quits originated from
// within the application can be redirected to not use -terminate:, but
// quits from elsewhere cannot be.
//
// To allow the Cocoa-based Chrome to support the standard Cocoa -terminate:
// interface, and allow all quits to cause Chrome to shut down properly
// regardless of their origin, -terminate: is overriden.  The custom
// -terminate: does not end the application with exit().  Instead, it simply
// returns after posting the normal NSApplicationWillTerminateNotification
// notification.  The application is responsible for exiting on its own in
// whatever way it deems appropriate.  In Chrome's case, the main run loop will
// end and the applicaton will exit by returning from main().
//
// This implementation of -terminate: is scaled back and is not as
// fully-featured as the implementation in NSApplication, nor is it a direct
// drop-in replacement -terminate: in most applications.  It is
// purpose-specific to Chrome.
- (void)terminate:(id)sender {
  NSApplicationTerminateReply shouldTerminate = NSTerminateNow;
  SEL selector = @selector(applicationShouldTerminate:);
  if ([[self delegate] respondsToSelector:selector])
    shouldTerminate = [[self delegate] applicationShouldTerminate:self];

  // If shouldTerminate is NSTerminateLater, the application is expected to
  // call -replyToApplicationShouldTerminate: when it knows whether or not it
  // should terminate.  If the argument is YES,
  // -replyToApplicationShouldTerminate: will call -terminate:.  This will
  // result in another call to the delegate's -applicationShouldTerminate:,
  // which would be expected to return NSTerminateNow at that point.
  if (shouldTerminate != NSTerminateNow)
    return;

  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSApplicationWillTerminateNotification
                    object:self];

  // Return, don't exit.  The application is responsible for exiting on its
  // own.
}

@end
