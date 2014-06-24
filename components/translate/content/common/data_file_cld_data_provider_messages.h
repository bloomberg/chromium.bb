// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"

// Note, for clean abstraction there is only one "CldDataProviderMessageStart"
// that is declared in ipc_message_start.h. All implementations share it,
// which avoids the need for each implementation to make its own entry.
// This would otherwise be very messy, as there is no mechanism to customize
// or extend the IPCMessageStart enum.
#define IPC_MESSAGE_START CldDataProviderMsgStart

// Informs the browser process that Compact Language Detector (CLD) data is
// needed in the form of a data file.
// This message is sent by a DataFileRendererCldDataProvider to a
// DataFileBrowserCldDataProvider.
IPC_MESSAGE_ROUTED0(ChromeViewHostMsg_NeedCldDataFile)

// Informs the renderer process that Compact Language Detector (CLD) data is
// available and provides an IPC::PlatformFileForTransit obtained from
// IPC::GetFileHandleForProcess(...)
// This message is sent by a DataFileBrowserCldDataProvider to a
// DataFileRendererCldDataProvider.
IPC_MESSAGE_ROUTED3(ChromeViewMsg_CldDataFileAvailable,
                    IPC::PlatformFileForTransit /* ipc_file_handle */,
                    uint64 /* data_offset */,
                    uint64 /* data_length */)
