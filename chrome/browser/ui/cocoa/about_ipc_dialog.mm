// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/cocoa/about_ipc_controller.h"
#include "chrome/browser/ui/cocoa/about_ipc_dialog.h"

#if defined(IPC_MESSAGE_LOG_ENABLED)

namespace chrome {

void ShowAboutIPCDialog() {
  // The controller gets deallocated when then window is closed,
  // so it is safe to "fire and forget".
  AboutIPCController* controller = [AboutIPCController sharedController];
  [[controller window] makeKeyAndOrderFront:controller];
}

}  // namespace chrome

void AboutIPCBridge::Log(const IPC::LogData& data) {
  CocoaLogData* cocoa_data = [[CocoaLogData alloc] initWithLogData:data];
  if ([NSThread isMainThread]) {
    [controller_ log:cocoa_data];
  } else {
    [controller_ performSelectorOnMainThread:@selector(log:)
                                  withObject:cocoa_data
                               waitUntilDone:NO];
  }
}

#endif  // IPC_MESSAGE_LOG_ENABLED
