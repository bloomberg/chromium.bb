// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the media streaming.
// Multiply-included message file, hence no include guard.

#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/ipc/media_param_traits.h"
#include "media/capture/ipc/capture_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(content::MediaStreamType,
                          content::NUM_MEDIA_TYPES - 1)

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoFacingMode,
                          media::NUM_MEDIA_VIDEO_FACING_MODES - 1)

IPC_STRUCT_TRAITS_BEGIN(content::StreamDeviceInfo)
  IPC_STRUCT_TRAITS_MEMBER(device.type)
  IPC_STRUCT_TRAITS_MEMBER(device.id)
  IPC_STRUCT_TRAITS_MEMBER(device.video_facing)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output_device_id)
  IPC_STRUCT_TRAITS_MEMBER(device.name)
  IPC_STRUCT_TRAITS_MEMBER(device.input)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output)
  IPC_STRUCT_TRAITS_MEMBER(device.camera_calibration)
  IPC_STRUCT_TRAITS_MEMBER(session_id)
IPC_STRUCT_TRAITS_END()
