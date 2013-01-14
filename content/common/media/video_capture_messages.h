// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_memory.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "media/video/capture/video_capture_types.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START VideoCaptureMsgStart

IPC_ENUM_TRAITS(content::VideoCaptureState)

// Notify the renderer process about the state update such as
// Start/Pause/Stop.
IPC_MESSAGE_CONTROL2(VideoCaptureMsg_StateChanged,
                     int /* device id */,
                     content::VideoCaptureState /* new state */)

// Tell the renderer process that a new buffer is allocated for video capture.
IPC_MESSAGE_CONTROL4(VideoCaptureMsg_NewBuffer,
                     int /* device id */,
                     base::SharedMemoryHandle /* handle */,
                     int /* length */,
                     int /* buffer_id */)

// Tell the renderer process that a buffer is available from video capture.
IPC_MESSAGE_CONTROL3(VideoCaptureMsg_BufferReady,
                     int /* device id */,
                     int /* buffer_id */,
                     base::Time /* timestamp */)

// Tell the renderer process the width, height and frame rate the camera use.
IPC_MESSAGE_CONTROL2(VideoCaptureMsg_DeviceInfo,
                     int /* device_id */,
                     media::VideoCaptureParams)

// Start the video capture specified by |device_id|.
IPC_MESSAGE_CONTROL2(VideoCaptureHostMsg_Start,
                     int /* device_id */,
                     media::VideoCaptureParams)

// Pause the video capture specified by |device_id|.
IPC_MESSAGE_CONTROL1(VideoCaptureHostMsg_Pause,
                     int /* device_id */)

// Close the video capture specified by |device_id|.
IPC_MESSAGE_CONTROL1(VideoCaptureHostMsg_Stop,
                     int /* device_id */)

// Tell the browser process that the video frame buffer |handle| is ready for
// device |device_id| to fill up.
IPC_MESSAGE_CONTROL2(VideoCaptureHostMsg_BufferReady,
                     int /* device_id */,
                     int /* buffer_id */)
