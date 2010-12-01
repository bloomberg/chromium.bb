// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/about_ipc_bridge.h"
#include "chrome/browser/ui/cocoa/about_ipc_controller.h"

#if defined(IPC_MESSAGE_LOG_ENABLED)

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

#endif
