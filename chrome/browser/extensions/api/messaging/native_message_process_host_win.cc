// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"

#include <windows.h>

#include "base/logging.h"
#include "base/message_pump_win.h"
#include "base/platform_file.h"
#include "base/process_util.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

void NativeMessageProcessHost::OnIOCompleted(
    MessageLoopForIO::IOContext* context,
    DWORD bytes_transfered,
    DWORD error) {
  NOTREACHED();
}

void NativeMessageProcessHost::InitIO() {
  NOTREACHED();
}

bool NativeMessageProcessHost::WriteData(FileHandle file,
                                         const char* data,
                                         size_t bytes_to_write) {
  NOTREACHED();
  return false;
}

bool NativeMessageProcessHost::ReadData(FileHandle file,
                                        char* data,
                                        size_t bytes_to_read) {
  NOTREACHED();
  return false;
}

}  // namespace extensions
