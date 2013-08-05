// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for device motion.
// Multiply-included message file, hence no include guard.

#include "base/memory/shared_memory.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_param_traits.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START DeviceMotionMsgStart

// Messages sent from the renderer to the browser.

// Asks the browser process to start polling, and return a shared memory
// handles that will hold the data from the hardware. See
// device_motion_hardware_buffer.h for a description of how synchronization is
// handled. The number of Starts should match the number of Stops.
IPC_MESSAGE_CONTROL0(DeviceMotionHostMsg_StartPolling)
IPC_MESSAGE_CONTROL1(DeviceMotionMsg_DidStartPolling,
                     base::SharedMemoryHandle /* handle */)

IPC_MESSAGE_CONTROL0(DeviceMotionHostMsg_StopPolling)

// TODO(timvolodine): remove the methods below once the Device Motion
// is implemented.

IPC_STRUCT_BEGIN(DeviceMotionMsg_Updated_Params)
  // These fields have the same meaning as in device_motion::Motion.
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_x)
  IPC_STRUCT_MEMBER(double, acceleration_x)
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_y)
  IPC_STRUCT_MEMBER(double, acceleration_y)
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_z)
  IPC_STRUCT_MEMBER(double, acceleration_z)
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_including_gravity_x)
  IPC_STRUCT_MEMBER(double, acceleration_including_gravity_x)
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_including_gravity_y)
  IPC_STRUCT_MEMBER(double, acceleration_including_gravity_y)
  IPC_STRUCT_MEMBER(bool, can_provide_acceleration_including_gravity_z)
  IPC_STRUCT_MEMBER(double, acceleration_including_gravity_z)
  IPC_STRUCT_MEMBER(bool, can_provide_rotation_rate_alpha)
  IPC_STRUCT_MEMBER(double, rotation_rate_alpha)
  IPC_STRUCT_MEMBER(bool, can_provide_rotation_rate_beta)
  IPC_STRUCT_MEMBER(double, rotation_rate_beta)
  IPC_STRUCT_MEMBER(bool, can_provide_rotation_rate_gamma)
  IPC_STRUCT_MEMBER(double, rotation_rate_gamma)
  IPC_STRUCT_MEMBER(bool, can_provide_interval)
  IPC_STRUCT_MEMBER(double, interval)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

// Notification that the device's motion has changed.
IPC_MESSAGE_ROUTED1(DeviceMotionMsg_Updated,
                    DeviceMotionMsg_Updated_Params)

// Messages sent from the renderer to the browser.

// A RenderView requests to start receiving device motion updates.
IPC_MESSAGE_CONTROL1(DeviceMotionHostMsg_StartUpdating,
                     int /* render_view_id */)

// A RenderView requests to stop receiving device motion updates.
IPC_MESSAGE_CONTROL1(DeviceMotionHostMsg_StopUpdating,
                     int /* render_view_id */)
