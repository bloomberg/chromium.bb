// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_PEER_CONNECTION_TRACKER_MESSAGES_H_
#define CONTENT_COMMON_MEDIA_PEER_CONNECTION_TRACKER_MESSAGES_H_

#include "base/values.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START PeerConnectionTrackerMsgStart

IPC_STRUCT_BEGIN(PeerConnectionInfo)
  // ID of the peer connection. Unique only within the renderer process.
  IPC_STRUCT_MEMBER(int, lid)
  // Serialized version of RTCConfiguration.
  IPC_STRUCT_MEMBER(std::string, rtc_configuration)
  // Serialized version of blink::WebMediaConstraints.
  IPC_STRUCT_MEMBER(std::string, constraints)
  // The URL of the blink::WebLocalFrame within which this peer connection
  // lives. Used for debugging purposes (displayed by WebRTC-Internals).
  IPC_STRUCT_MEMBER(std::string, url)
IPC_STRUCT_END()

// Messages sent from PeerConnectionTracker to PeerConnectionTrackerHost.
IPC_MESSAGE_CONTROL1(PeerConnectionTrackerHost_AddPeerConnection,
                     PeerConnectionInfo /* info */)
IPC_MESSAGE_CONTROL2(PeerConnectionTrackerHost_AddStandardStats,
                     int /* lid */,
                     base::ListValue /* value */)
IPC_MESSAGE_CONTROL2(PeerConnectionTrackerHost_AddLegacyStats,
                     int /* lid */,
                     base::ListValue /* value */)

// Messages sent to PeerConnectionTracker.
IPC_MESSAGE_CONTROL0(PeerConnectionTracker_GetStandardStats)
IPC_MESSAGE_CONTROL0(PeerConnectionTracker_GetLegacyStats)
IPC_MESSAGE_CONTROL0(PeerConnectionTracker_OnSuspend)
IPC_MESSAGE_CONTROL2(PeerConnectionTracker_StartEventLog,
                     int /* peer_connection_local_id */,
                     int /* output_period_ms */)
IPC_MESSAGE_CONTROL1(PeerConnectionTracker_StopEventLog,
                     int /* peer_connection_local_id */)

#endif  // CONTENT_COMMON_MEDIA_PEER_CONNECTION_TRACKER_MESSAGES_H_
