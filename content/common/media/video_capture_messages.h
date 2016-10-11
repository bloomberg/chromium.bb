// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/video_capture_types.h"
#include "media/base/video_frame.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START VideoCaptureMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoFrame::StorageType,
                          media::VideoFrame::STORAGE_LAST)

IPC_STRUCT_BEGIN(VideoCaptureMsg_BufferReady_Params)
  IPC_STRUCT_MEMBER(int, device_id)
  IPC_STRUCT_MEMBER(int, buffer_id)
  IPC_STRUCT_MEMBER(base::TimeDelta, timestamp)
  IPC_STRUCT_MEMBER(base::DictionaryValue, metadata)
  IPC_STRUCT_MEMBER(media::VideoPixelFormat, pixel_format)
  IPC_STRUCT_MEMBER(media::VideoFrame::StorageType, storage_type)
  IPC_STRUCT_MEMBER(gfx::Size, coded_size)
  IPC_STRUCT_MEMBER(gfx::Rect, visible_rect)
IPC_STRUCT_END()

// TODO(nick): device_id in these messages is basically just a route_id. We
// should shift to IPC_MESSAGE_ROUTED and use MessageRouter in the filter impls.

// Tell the renderer process that a new buffer is allocated for video capture.
IPC_MESSAGE_CONTROL4(VideoCaptureMsg_NewBuffer,
                     int /* device id */,
                     base::SharedMemoryHandle /* handle */,
                     int /* length */,
                     int /* buffer_id */)

// Tell the renderer process that it should release a buffer previously
// allocated by VideoCaptureMsg_NewBuffer.
IPC_MESSAGE_CONTROL2(VideoCaptureMsg_FreeBuffer,
                     int /* device id */,
                     int /* buffer_id */)

// Tell the renderer process that a Buffer is available from video capture, and
// send the associated VideoFrame constituent parts as IPC parameters.
IPC_MESSAGE_CONTROL1(VideoCaptureMsg_BufferReady,
                     VideoCaptureMsg_BufferReady_Params)
