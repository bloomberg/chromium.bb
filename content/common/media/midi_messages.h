// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for access to MIDI hardware.
// Multiply-included message file, hence no include guard.

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "media/midi/midi_port_info.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MidiMsgStart

IPC_STRUCT_TRAITS_BEGIN(media::MidiPortInfo)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(manufacturer)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(version)
IPC_STRUCT_TRAITS_END()

// Messages for IPC between MidiDispatcher and MidiDispatcherHost.

// Renderer request to browser for using system exclusive messages.
IPC_MESSAGE_CONTROL3(MidiHostMsg_RequestSysExPermission,
                     int /* routing id */,
                     int /* client id */,
                     GURL /* origin */)

// Renderer request to browser for canceling a previous permission request.
IPC_MESSAGE_CONTROL3(MidiHostMsg_CancelSysExPermissionRequest,
                     int /* render_view_id */,
                     int /* bridge_id */,
                     GURL /* GURL of the frame */)

// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED2(MidiMsg_SysExPermissionApproved,
                    int /* client id */,
                    bool /* success */)

// Messages for IPC between MidiMessageFilter and MidiHost.

// Renderer request to browser for access to MIDI services.
IPC_MESSAGE_CONTROL1(MidiHostMsg_StartSession,
                     int /* client id */)

IPC_MESSAGE_CONTROL3(MidiHostMsg_SendData,
                     uint32 /* port */,
                     std::vector<uint8> /* data */,
                     double /* timestamp */)

// Messages sent from the browser to the renderer.

IPC_MESSAGE_CONTROL4(MidiMsg_SessionStarted,
                     int /* client id */,
                     bool /* success */,
                     media::MidiPortInfoList /* input ports */,
                     media::MidiPortInfoList /* output ports */)

IPC_MESSAGE_CONTROL3(MidiMsg_DataReceived,
                     uint32 /* port */,
                     std::vector<uint8> /* data */,
                     double /* timestamp */)

IPC_MESSAGE_CONTROL1(MidiMsg_AcknowledgeSentData,
                     uint32 /* bytes sent */)
