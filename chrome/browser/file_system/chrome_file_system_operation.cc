// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/chrome_file_system_operation.h"

#include "chrome/browser/chrome_thread.h"

ChromeFileSystemOperation::ChromeFileSystemOperation(
    int request_id, fileapi::FileSystemOperationClient* client)
    : fileapi::FileSystemOperation(client,
          ChromeThread::GetMessageLoopProxyForThread(ChromeThread::FILE)),
      request_id_(request_id) { }

