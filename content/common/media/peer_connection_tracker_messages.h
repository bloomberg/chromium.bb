// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START PeerConnectionTrackerMsgStart

IPC_STRUCT_BEGIN(PeerConnectionInfo)
  IPC_STRUCT_MEMBER(int, lid)
  IPC_STRUCT_MEMBER(std::string, servers)
  IPC_STRUCT_MEMBER(std::string, constraints)
  IPC_STRUCT_MEMBER(std::string, url)
IPC_STRUCT_END()

// Messages sent from PeerConnectionTracker to PeerConnectionTrackerHost.
IPC_MESSAGE_CONTROL1(PeerConnectionTrackerHost_AddPeerConnection,
                     PeerConnectionInfo /* info */)
IPC_MESSAGE_CONTROL1(PeerConnectionTrackerHost_RemovePeerConnection,
                     int /* lid */)
