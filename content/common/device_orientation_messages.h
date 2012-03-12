// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for device orientation.
// Multiply-included message file, hence no include guard.

#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START DeviceOrientationMsgStart

IPC_STRUCT_BEGIN(DeviceOrientationMsg_Updated_Params)
  // These fields have the same meaning as in device_orientation::Orientation.
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
