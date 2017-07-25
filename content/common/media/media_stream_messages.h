// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the media streaming.
// Multiply-included message file, hence no include guard.

#include <string>

#include "content/common/content_export.h"
#include "content/common/media/media_stream_options.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "url/origin.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaStreamMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(content::MediaStreamType,
                          content::NUM_MEDIA_TYPES - 1)

IPC_ENUM_TRAITS_MAX_VALUE(media::VideoFacingMode,
                          media::NUM_MEDIA_VIDEO_FACING_MODES - 1)

IPC_ENUM_TRAITS_MAX_VALUE(content::MediaStreamRequestResult,
                          content::NUM_MEDIA_REQUEST_RESULTS - 1)

IPC_STRUCT_TRAITS_BEGIN(content::TrackControls)
  IPC_STRUCT_TRAITS_MEMBER(requested)
  IPC_STRUCT_TRAITS_MEMBER(stream_source)
  IPC_STRUCT_TRAITS_MEMBER(device_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::StreamControls)
  IPC_STRUCT_TRAITS_MEMBER(audio)
  IPC_STRUCT_TRAITS_MEMBER(video)
  IPC_STRUCT_TRAITS_MEMBER(hotword_enabled)
  IPC_STRUCT_TRAITS_MEMBER(disable_local_echo)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::StreamDeviceInfo)
  IPC_STRUCT_TRAITS_MEMBER(device.type)
  IPC_STRUCT_TRAITS_MEMBER(device.name)
  IPC_STRUCT_TRAITS_MEMBER(device.id)
  IPC_STRUCT_TRAITS_MEMBER(device.video_facing)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output_device_id)
  IPC_STRUCT_TRAITS_MEMBER(device.input.sample_rate)
  IPC_STRUCT_TRAITS_MEMBER(device.input.channel_layout)
  IPC_STRUCT_TRAITS_MEMBER(device.input.frames_per_buffer)
  IPC_STRUCT_TRAITS_MEMBER(device.input.effects)
  IPC_STRUCT_TRAITS_MEMBER(device.input.mic_positions)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output.sample_rate)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output.channel_layout)
  IPC_STRUCT_TRAITS_MEMBER(device.matched_output.frames_per_buffer)
  IPC_STRUCT_TRAITS_MEMBER(device.camera_calibration)
  IPC_STRUCT_TRAITS_MEMBER(session_id)
IPC_STRUCT_TRAITS_END()

// Message sent from the browser to the renderer

// The browser has generated a stream successfully.
IPC_MESSAGE_ROUTED4(MediaStreamMsg_StreamGenerated,
                    int /* request id */,
                    std::string /* label */,
                    content::StreamDeviceInfoArray /* audio_device_list */,
                    content::StreamDeviceInfoArray /* video_device_list */)

// The browser has failed to generate a stream.
IPC_MESSAGE_ROUTED2(MediaStreamMsg_StreamGenerationFailed,
                    int /* request id */,
                    content::MediaStreamRequestResult /* result */)

// The browser reports that a media device has been stopped. Stopping was
// triggered from the browser process.
IPC_MESSAGE_ROUTED2(MediaStreamMsg_DeviceStopped,
                    std::string /* label */,
                    content::StreamDeviceInfo /* the device */)

// TODO(wjia): should DeviceOpen* messages be merged with
// StreamGenerat* ones?
// The browser has opened a device successfully.
IPC_MESSAGE_ROUTED3(MediaStreamMsg_DeviceOpened,
                    int /* request id */,
                    std::string /* label */,
                    content::StreamDeviceInfo /* the device */)

// The browser has failed to open a device.
IPC_MESSAGE_ROUTED1(MediaStreamMsg_DeviceOpenFailed,
                    int /* request id */)

// Messages sent from the renderer to the browser.

// Request a new media stream.
IPC_MESSAGE_CONTROL5(MediaStreamHostMsg_GenerateStream,
                     int /* render frame id */,
                     int /* request id */,
                     content::StreamControls /* controls */,
                     url::Origin /* security origin */,
                     bool /* user_gesture */)
