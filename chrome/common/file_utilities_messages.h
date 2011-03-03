// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, so no include guard.

#include "base/time.h"
#include "chrome/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START FileUtilitiesMsgStart

// File utilities messages sent from the renderer to the browser.

// Get file size in bytes. Set result to -1 if failed to get the file size.
IPC_SYNC_MESSAGE_CONTROL1_1(FileUtilitiesMsg_GetFileSize,
                            FilePath /* path */,
                            int64 /* result */)

// Get file modification time in seconds. Set result to 0 if failed to get the
// file modification time.
IPC_SYNC_MESSAGE_CONTROL1_1(FileUtilitiesMsg_GetFileModificationTime,
                            FilePath /* path */,
                            base::Time /* result */)

// Open the file.
IPC_SYNC_MESSAGE_CONTROL2_1(FileUtilitiesMsg_OpenFile,
                            FilePath /* path */,
                            int /* mode */,
                            IPC::PlatformFileForTransit /* result */)

