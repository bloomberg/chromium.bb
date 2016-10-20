// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START VideoCaptureMsgStart

// TODO(mcasas): Remove this message when the Video Capture IPC communication is
// completely migrated to Mojo, https://crbug.com/651897.
IPC_MESSAGE_CONTROL0(VideoCaptureMsg_Noop)
