// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "media/video/capture/video_capture.h"

#define IPC_MESSAGE_START VideoCaptureMsgStart

IPC_ENUM_TRAITS(media::VideoCapture::State)

IPC_STRUCT_TRAITS_BEGIN(media::VideoCapture::CaptureParams)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(frame_rate)
  IPC_STRUCT_TRAITS_MEMBER(session_id)
IPC_STRUCT_TRAITS_END()

// Notify the renderer process about the state update such as
// Start/Pause/Stop.
IPC_MESSAGE_ROUTED2(VideoCaptureMsg_StateChanged,
                    int /* device id */,
                    media::VideoCapture::State /* new state */)

// Tell the renderer process that a buffer is available from video capture.
IPC_MESSAGE_ROUTED3(VideoCaptureMsg_BufferReady,
                    int /* device id */,
                    TransportDIB::Handle /* DIB */,
                    base::Time /* timestamp */)

// Tell the renderer process the width, height and frame rate the camera use.
IPC_MESSAGE_ROUTED2(VideoCaptureMsg_DeviceInfo,
                    int /* device_id */,
                    media::VideoCapture::CaptureParams)

// Start the video capture specified by (routing_id, device_id).
IPC_MESSAGE_ROUTED2(VideoCaptureHostMsg_Start,
                    int /* device_id */,
                    media::VideoCapture::CaptureParams)

// Pause the video capture specified by (routing_id, device_id).
IPC_MESSAGE_ROUTED1(VideoCaptureHostMsg_Pause,
                    int /* device_id */)

// Close the video capture specified by (routing_id, device_id).
IPC_MESSAGE_ROUTED1(VideoCaptureHostMsg_Stop,
                    int /* device_id */)

// Tell the browser process that the video frame buffer |handle| is ready for
// device (routing_id, device_id) to fill up.
IPC_MESSAGE_ROUTED2(VideoCaptureHostMsg_BufferReady,
                    int /* device_id */,
                    TransportDIB::Handle /* handle */)
