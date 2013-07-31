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
#define IPC_MESSAGE_START MIDIMsgStart

IPC_STRUCT_TRAITS_BEGIN(media::MIDIPortInfo)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(manufacturer)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(version)
IPC_STRUCT_TRAITS_END()

// Messages for IPC between MIDIDispatcher and MIDIDispatcherHost.

// Renderer request to browser for using system exclusive messages.
IPC_MESSAGE_CONTROL3(MIDIHostMsg_RequestSysExPermission,
                     int /* routing id */,
                     int /* client id */,
                     GURL /* origin */)

// Messages sent from the browser to the renderer.

IPC_MESSAGE_ROUTED2(MIDIMsg_SysExPermissionApproved,
                    int /* client id */,
                    bool /* success */)

// Messages for IPC between MIDIMessageFilter and MIDIHost.

// Renderer request to browser for access to MIDI services.
IPC_MESSAGE_CONTROL1(MIDIHostMsg_StartSession,
                     int /* client id */)

IPC_MESSAGE_CONTROL3(MIDIHostMsg_SendData,
                     int /* port */,
                     std::vector<uint8> /* data */,
                     double /* timestamp */)

// Messages sent from the browser to the renderer.

IPC_MESSAGE_CONTROL4(MIDIMsg_SessionStarted,
                     int /* client id */,
                     bool /* success */,
                     media::MIDIPortInfoList /* input ports */,
                     media::MIDIPortInfoList /* output ports */)

IPC_MESSAGE_CONTROL3(MIDIMsg_DataReceived,
                     int /* port */,
                     std::vector<uint8> /* data */,
                     double /* timestamp */)

IPC_MESSAGE_CONTROL1(MIDIMsg_AcknowledgeSentData,
                     size_t /* bytes sent */)
