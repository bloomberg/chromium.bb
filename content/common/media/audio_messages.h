// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for the audio.
// Multiply-included message file, hence no include guard.

#include "base/basictypes.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "content/common/media/audio_stream_state.h"
#include "ipc/ipc_message_macros.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_parameters.h"

#define IPC_MESSAGE_START AudioMsgStart

IPC_ENUM_TRAITS(AudioStreamState)
IPC_ENUM_TRAITS(AudioParameters::Format)
IPC_ENUM_TRAITS(ChannelLayout)

IPC_STRUCT_TRAITS_BEGIN(AudioBuffersState)
  IPC_STRUCT_TRAITS_MEMBER(pending_bytes)
  IPC_STRUCT_TRAITS_MEMBER(hardware_delay_bytes)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(AudioParameters)
  IPC_STRUCT_TRAITS_MEMBER(format)
  IPC_STRUCT_TRAITS_MEMBER(channel_layout)
  IPC_STRUCT_TRAITS_MEMBER(sample_rate)
  IPC_STRUCT_TRAITS_MEMBER(bits_per_sample)
  IPC_STRUCT_TRAITS_MEMBER(samples_per_packet)
  IPC_STRUCT_TRAITS_MEMBER(channels)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer.

// Sent by AudioRendererHost to renderer to request an audio packet.
IPC_MESSAGE_CONTROL2(AudioMsg_RequestPacket,
                     int /* stream id */,
                     AudioBuffersState)

// Tell the renderer process that the audio stream has been created, renderer
// process would be given a ShareMemoryHandle that it should write to from
// then on.
IPC_MESSAGE_CONTROL3(AudioMsg_NotifyStreamCreated,
                     int /* stream id */,
                     base::SharedMemoryHandle /* handle */,
                     uint32 /* length */)

// Tell the renderer process that a low latency audio stream has been created,
// renderer process would be given a SyncSocket that it should write to from
// then on.
#if defined(OS_WIN)
IPC_MESSAGE_CONTROL4(AudioMsg_NotifyLowLatencyStreamCreated,
                     int /* stream id */,
                     base::SharedMemoryHandle /* handle */,
                     base::SyncSocket::Handle /* socket handle */,
                     uint32 /* length */)
#else
IPC_MESSAGE_CONTROL4(AudioMsg_NotifyLowLatencyStreamCreated,
                     int /* stream id */,
                     base::SharedMemoryHandle /* handle */,
                     base::FileDescriptor /* socket handle */,
                     uint32 /* length */)
#endif

// Tell the renderer process that a low latency audio input stream has been
// created, renderer process would be given a SyncSocket that it should read
// from from then on.
#if defined(OS_WIN)
IPC_MESSAGE_CONTROL4(AudioInputMsg_NotifyLowLatencyStreamCreated,
                     int /* stream id */,
                     base::SharedMemoryHandle /* handle */,
                     base::SyncSocket::Handle /* socket handle */,
                     uint32 /* length */)
#else
IPC_MESSAGE_CONTROL4(AudioInputMsg_NotifyLowLatencyStreamCreated,
                     int /* stream id */,
                     base::SharedMemoryHandle /* handle */,
                     base::FileDescriptor /* socket handle */,
                     uint32 /* length */)
#endif

// Notification message sent from AudioRendererHost to renderer for state
// update after the renderer has requested a Create/Start/Close.
IPC_MESSAGE_CONTROL2(AudioMsg_NotifyStreamStateChanged,
                     int /* stream id */,
                     AudioStreamState /* new state */)

// Notification message sent from browser to renderer for state update.
IPC_MESSAGE_CONTROL2(AudioInputMsg_NotifyStreamStateChanged,
                     int /* stream id */,
                     AudioStreamState /* new state */)

IPC_MESSAGE_CONTROL2(AudioMsg_NotifyStreamVolume,
                     int /* stream id */,
                     double /* volume */)

IPC_MESSAGE_CONTROL2(AudioInputMsg_NotifyStreamVolume,
                     int /* stream id */,
                     double /* volume */)

IPC_MESSAGE_CONTROL2(AudioInputMsg_NotifyDeviceStarted,
                     int /* stream id */,
                     std::string /* device_id */)

// Messages sent from the renderer to the browser.

// Request that got sent to browser for creating an audio output stream
IPC_MESSAGE_CONTROL3(AudioHostMsg_CreateStream,
                     int /* stream_id */,
                     AudioParameters /* params */,
                     bool /* low-latency */)

// Request that got sent to browser for creating an audio input stream
IPC_MESSAGE_CONTROL4(AudioInputHostMsg_CreateStream,
                     int /* stream_id */,
                     AudioParameters /* params */,
                     bool /* low-latency */,
                     std::string /* device_id */)

// Tell the browser the audio buffer prepared for stream (stream_id) is
// filled and is ready to be consumed.
IPC_MESSAGE_CONTROL2(AudioHostMsg_NotifyPacketReady,
                     int /* stream_id */,
                     uint32 /* packet size */)

// Start buffering and play the audio stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioHostMsg_PlayStream,
                     int /* stream_id */)

// Start recording the audio input stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioInputHostMsg_RecordStream,
                     int /* stream_id */)

// Pause the audio stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioHostMsg_PauseStream,
                     int /* stream_id */)

// Discard all buffered audio data for the specified audio stream.
IPC_MESSAGE_CONTROL1(AudioHostMsg_FlushStream,
                     int /* stream_id */)

// Close an audio stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioHostMsg_CloseStream,
                     int /* stream_id */)

// Close an audio input stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioInputHostMsg_CloseStream,
                     int /* stream_id */)

// Get audio volume of the stream specified by stream_id.
IPC_MESSAGE_CONTROL1(AudioHostMsg_GetVolume,
                     int /* stream_id */)

// Get audio volume of the input stream specified by
// (render_view_id, stream_id).
IPC_MESSAGE_CONTROL1(AudioInputHostMsg_GetVolume,
                     int /* stream_id */)

// Set audio volume of the stream specified by stream_id.
// TODO(hclam): change this to vector if we have channel numbers other than 2.
IPC_MESSAGE_CONTROL2(AudioHostMsg_SetVolume,
                     int /* stream_id */,
                     double /* volume */)

// Set audio volume of the input stream specified by stream_id.
IPC_MESSAGE_CONTROL2(AudioInputHostMsg_SetVolume,
                     int /* stream_id */,
                     double /* volume */)

// Start the device referenced by the session_id for the input stream specified
// by stream_id.
IPC_MESSAGE_CONTROL2(AudioInputHostMsg_StartDevice,
                     int /* stream_id */,
                     int /* session_id */)
