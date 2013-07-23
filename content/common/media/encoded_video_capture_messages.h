// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"
#include "ipc/ipc_message_macros.h"
#include "media/video/capture/video_capture_types.h"
#include "media/video/video_encode_types.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START EncodedVideoCaptureMsgStart

#if !defined(OS_ANDROID)
IPC_ENUM_TRAITS(media::VideoCodec)
#endif  // !defined(OS_ANDROID)

IPC_STRUCT_TRAITS_BEGIN(media::VideoEncodingConfig)
  IPC_STRUCT_TRAITS_MEMBER(codec_type)
  IPC_STRUCT_TRAITS_MEMBER(codec_name)
  IPC_STRUCT_TRAITS_MEMBER(max_resolution)
  IPC_STRUCT_TRAITS_MEMBER(max_frames_per_second)
  IPC_STRUCT_TRAITS_MEMBER(max_bitrate)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::RuntimeVideoEncodingParameters)
  IPC_STRUCT_TRAITS_MEMBER(target_bitrate)
  IPC_STRUCT_TRAITS_MEMBER(max_bitrate)
  IPC_STRUCT_TRAITS_MEMBER(frames_per_second)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::VideoEncodingParameters)
  IPC_STRUCT_TRAITS_MEMBER(codec_name)
  IPC_STRUCT_TRAITS_MEMBER(resolution)
  IPC_STRUCT_TRAITS_MEMBER(runtime_params)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::BufferEncodingMetadata)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(key_frame)
IPC_STRUCT_TRAITS_END()

//------------------------------------------------------------------------------
// Renderer Source Messages
// These are messages from the renderer to the browser process.

// Queries the encoding capabilities for the device. A successful request
// results in EncoderVideoSourceMessage_CapabilitiesAvailable message.
IPC_MESSAGE_CONTROL2(EncodedVideoCaptureHostMsg_GetCapabilities,
                     int /* device_id */,
                     media::VideoCaptureSessionId /* session_id */)

// Message from renderer to browser process to create a bitstream with specific
// parameters. A successful request results in beginning of streaming and
// EncoderVideoCaptureMsg_BitstreamCreated message to renderer. A failed request
// triggers EncodedVideoCaptureMsg_BitstreamDestroyed message. |session_id| is
// the capture session id returned by the MediaStreamManager. The renderer is
// responsible for generating unique |device_id| within its context that will be
// used to identify bitstreams in IPC.
IPC_MESSAGE_CONTROL3(EncodedVideoCaptureHostMsg_OpenBitstream,
                     int /* device_id */,
                     media::VideoCaptureSessionId /* session_id */,
                     media::VideoEncodingParameters /* params */)

// Stops streaming a bitstream. When browser has finalized the bitstream it will
// trigger EncodedVideoCaptureMsg_BitstreamClosed message back to renderer.
// Renderer must be prepared to receive EncodedVideoCaptureMsg_BitstreamReady
// messages until it receives EncodedVideoCaptureMsg_BitstreamClosed message.
IPC_MESSAGE_CONTROL1(EncodedVideoCaptureHostMsg_CloseBitstream,
                    int /* device_id */)

// Sets a stream's bitstream configuration. Will always result in
// EncodedVideoCaptureMsg_BitstreamConfigChanged message containing
// currently active parameters, regardless of whether this call succeeded or
// not.
IPC_MESSAGE_CONTROL2(EncodedVideoCaptureHostMsg_TryConfigureBitstream,
                     int /* device_id */,
                     media::RuntimeVideoEncodingParameters /* params */)

// Requests a key frame in the encoded bitstream. Upon receiving this request,
// browser will try to encode an upcoming captured frame as a key frame. This
// allows the receiver to quickly recover from data loss. The request is served
// on a best-effort basis and there is no explicit acknowledgement.
IPC_MESSAGE_CONTROL1(EncodedVideoCaptureHostMsg_RequestKeyFrame,
                     int /* device_id */)

// Notifies that the data within a buffer has been processed and it can be
// reused to encode upcoming bitstream.
IPC_MESSAGE_CONTROL2(EncodedVideoCaptureHostMsg_BitstreamBufferConsumed,
                     int /* device_id */,
                     int /* buffer_id */)

//------------------------------------------------------------------------------
// Renderer Messages
// These are messages from the browser to the renderer process.

// Reports the encoding capabilities of the device.
IPC_MESSAGE_CONTROL2(EncodedVideoCaptureMsg_CapabilitiesAvailable,
                     int /* device_id */,
                     media::VideoEncodingCapabilities /* capabilities */)

// Acknowledges a request to open an encoded video bitstream. When this message
// occurs, bitstream can be considered to be streaming, and renderer should be
// ready to start accepting EncodedVideoCaptureMsg_BitstreamReady messages and
// buffers contained within them. Shared memory buffers used to deliver the
// bitstream are assigned with buffer IDs as specified by the buffers parameter.
// All buffers have the same size as indicated by |buffer_size|.
IPC_MESSAGE_CONTROL4(EncodedVideoCaptureMsg_BitstreamOpened,
                     int /* device_id */,
                     media::VideoEncodingParameters /* params */,
                     std::vector<base::SharedMemoryHandle> /* buffers */,
                     uint32 /* buffer_size */)

// Acknowledges a request to close an encoded video bitstream.
IPC_MESSAGE_CONTROL1(EncodedVideoCaptureMsg_BitstreamClosed,
                     int /* device_id */)

// Informs the clients of the current encoding parameters, regardless of whether
// the previous request to change them has been successful or not. It is usually
// called in response to EncodedVideoCaptureHostMsg_TryConfigureBitstream
// at runtime, but can occur also as a result of config change initiated by
// encoder or other clients in the system, e.g. if there are multiple clients
// and bitstream config change is requested from one client, all clients should
// be prepared to handle the configuration change.
IPC_MESSAGE_CONTROL2(EncodedVideoCaptureMsg_BitstreamConfigChanged,
                     int /* device_id */,
                     media::RuntimeVideoEncodingParameters /* current_params */)

// Indicates that a bitstream buffer is available for the stream. The value of
// |size| indicates the amount of valid bitstream data (in bytes).
IPC_MESSAGE_CONTROL4(EncodedVideoCaptureMsg_BitstreamReady,
                     int /* device_id */,
                     int /* buffer_id */,
                     uint32 /* size */,
                     media::BufferEncodingMetadata /* metadata */)
