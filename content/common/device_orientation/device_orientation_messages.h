// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for device orientation.
// Multiply-included message file, hence no include guard.

#include "base/memory/shared_memory.h"
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START DeviceOrientationMsgStart

// Messages sent from the renderer to the browser.

// Asks the browser process to start polling, and return a shared memory
// handle that will hold the data from the hardware. See
// device_orientation_hardware_buffer.h for a description of how synchronization
// is handled. The number of Starts should match the number of Stops.
IPC_MESSAGE_CONTROL0(DeviceOrientationHostMsg_StartPolling)
IPC_MESSAGE_CONTROL1(DeviceOrientationMsg_DidStartPolling,
                     base::SharedMemoryHandle /* handle */)

IPC_MESSAGE_CONTROL0(DeviceOrientationHostMsg_StopPolling)

// TODO(timvolodine): remove the methods below once the shared memory
// Device Orientation is implemented.

IPC_STRUCT_BEGIN(DeviceOrientationMsg_Updated_Params)
  // These fields have the same meaning as in content::Orientation.
  IPC_STRUCT_MEMBER(bool, can_provide_alpha)
  IPC_STRUCT_MEMBER(double, alpha)
  IPC_STRUCT_MEMBER(bool, can_provide_beta)
  IPC_STRUCT_MEMBER(double, beta)
  IPC_STRUCT_MEMBER(bool, can_provide_gamma)
  IPC_STRUCT_MEMBER(double, gamma)
  IPC_STRUCT_MEMBER(bool, can_provide_absolute)
  IPC_STRUCT_MEMBER(bool, absolute)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Notification that the device's orientation has changed.
IPC_MESSAGE_ROUTED1(DeviceOrientationMsg_Updated,
                    DeviceOrientationMsg_Updated_Params)

// Messages sent from the renderer to the browser.

// A RenderView requests to start receiving device orientation updates.
IPC_MESSAGE_CONTROL1(DeviceOrientationHostMsg_StartUpdating,
                     int /* render_view_id */)

// A RenderView requests to stop receiving device orientation updates.
IPC_MESSAGE_CONTROL1(DeviceOrientationHostMsg_StopUpdating,
                     int /* render_view_id */)
