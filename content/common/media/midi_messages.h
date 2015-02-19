// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for access to MIDI hardware.
// Multiply-included message file, hence no include guard.

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/param_traits_macros.h"
#include "media/midi/midi_port_info.h"
#include "media/midi/midi_result.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MidiMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(media::MidiPortState,
                          media::MIDI_PORT_STATE_LAST)

IPC_STRUCT_TRAITS_BEGIN(media::MidiPortInfo)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(manufacturer)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(version)
  IPC_STRUCT_TRAITS_MEMBER(state)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(media::MidiResult, media::MIDI_RESULT_LAST)

// Messages for IPC between MidiMessageFilter and MidiHost.

// Renderer request to browser for access to MIDI services.
IPC_MESSAGE_CONTROL0(MidiHostMsg_StartSession)

IPC_MESSAGE_CONTROL3(MidiHostMsg_SendData,
                     uint32 /* port */,
                     std::vector<uint8> /* data */,
                     double /* timestamp */)

IPC_MESSAGE_CONTROL0(MidiHostMsg_EndSession)

// Messages sent from the browser to the renderer.

IPC_MESSAGE_CONTROL1(MidiMsg_AddInputPort,
                     media::MidiPortInfo /* input port */)

IPC_MESSAGE_CONTROL1(MidiMsg_AddOutputPort,
                     media::MidiPortInfo /* output port */)

IPC_MESSAGE_CONTROL2(MidiMsg_SetInputPortState,
                     uint32 /* port */,
                     media::MidiPortState /* state */)

IPC_MESSAGE_CONTROL2(MidiMsg_SetOutputPortState,
                     uint32 /* port */,
                     media::MidiPortState /* state */)

IPC_MESSAGE_CONTROL1(MidiMsg_SessionStarted,
                     media::MidiResult /* result */)

IPC_MESSAGE_CONTROL3(MidiMsg_DataReceived,
                     uint32 /* port */,
                     std::vector<uint8> /* data */,
                     double /* timestamp */)

IPC_MESSAGE_CONTROL1(MidiMsg_AcknowledgeSentData,
                     uint32 /* bytes sent */)
