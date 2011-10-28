// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, no include guard.

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "base/shared_memory.h"

#define IPC_MESSAGE_START GamepadMsgStart

// Messages sent from the renderer to the browser.

// Asks the browser process to start polling, and return a shared memory
// handles that will hold the (triple-buffered) data from the hardware.
// The number of Starts should match the number of Stops (below).
IPC_SYNC_MESSAGE_CONTROL0_1(GamepadHostMsg_StartPolling,
                            base::SharedMemoryHandle /* handle */)

IPC_SYNC_MESSAGE_CONTROL0_0(GamepadHostMsg_StopPolling)


// Messages sent from browser to renderer.

IPC_MESSAGE_CONTROL1(GamepadMsg_Updated,
                     int /* read_buffer_index */)
