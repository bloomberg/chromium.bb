// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains IPC messages for controlling memory pressure handling
// in child processes for the purposes of enforcing consistent conditions
// across memory measurements.

// Multiply-included message header, no traditional include guard.

#include "ipc/ipc_message_macros.h"
#include "content/common/content_export.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START MemoryMsgStart

// Sent to all child processes to enable/disable suppressing memory
// notifications.
IPC_MESSAGE_CONTROL1(MemoryMsg_SetPressureNotificationsSuppressed,
                     bool /* suppressed */)
