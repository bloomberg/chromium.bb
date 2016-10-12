// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include "base/message_loop/message_loop.h"
#include "chrome/app/chrome_command_ids.h"
#import "ui/base/test/windowed_nsnotification_observer.h"

namespace ui_test_utils {

void HideNativeWindow(gfx::NativeWindow window) {
  [window orderOut:nil];
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow window) {
  // Make sure an unbundled program can get the input focus.
  ProcessSerialNumber psn = { 0, kCurrentProcess };
  TransformProcessType(&psn,kProcessTransformToForegroundApplication);
  [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];

  base::scoped_nsobject<WindowedNSNotificationObserver> async_waiter;
  if (![window isKeyWindow]) {
    // Only wait when expecting a change to actually occur.
    async_waiter.reset([[WindowedNSNotificationObserver alloc]
        initForNotification:NSWindowDidBecomeKeyNotification
                     object:window]);
  }
  [window makeKeyAndOrderFront:nil];

  // Wait until |window| becomes key window, then make sure the shortcuts for
  // "Close Window" and "Close Tab" are updated.
  // This is because normal AppKit menu updating does not get invoked when
  // events are sent via ui_test_utils::SendKeyPressSync.
  BOOL notification_observed = [async_waiter wait];
  base::RunLoop().RunUntilIdle();  // There may be other events queued. Flush.
  NSMenu* file_menu = [[[NSApp mainMenu] itemWithTag:IDC_FILE_MENU] submenu];
  [[file_menu delegate] menuNeedsUpdate:file_menu];

  return !async_waiter || notification_observed;
}

}  // namespace ui_test_utils
