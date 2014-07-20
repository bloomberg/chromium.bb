// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_ipc_dispatcher.h"

#include "chrome/common/cast_messages.h"
#include "chrome/renderer/media/cast_transport_sender_ipc.h"
#include "ipc/ipc_message_macros.h"

CastIPCDispatcher* CastIPCDispatcher::global_instance_ = NULL;

CastIPCDispatcher::CastIPCDispatcher(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop)
    : sender_(NULL),
      io_message_loop_(io_message_loop) {
  DCHECK(io_message_loop_);
  DCHECK(!global_instance_);
}

CastIPCDispatcher::~CastIPCDispatcher() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  DCHECK(!global_instance_);
}

CastIPCDispatcher* CastIPCDispatcher::Get() {
  return global_instance_;
}

void CastIPCDispatcher::Send(IPC::Message* message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (sender_) {
    sender_->Send(message);
  } else {
    delete message;
  }
}

int32 CastIPCDispatcher::AddSender(CastTransportSenderIPC* sender) {
  return id_map_.Add(sender);
}

void CastIPCDispatcher::RemoveSender(int32 channel_id) {
  return id_map_.Remove(channel_id);
}

bool CastIPCDispatcher::OnMessageReceived(const IPC::Message& message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CastIPCDispatcher, message)
    IPC_MESSAGE_HANDLER(CastMsg_NotifyStatusChange, OnNotifyStatusChange)
    IPC_MESSAGE_HANDLER(CastMsg_RawEvents, OnRawEvents)
    IPC_MESSAGE_HANDLER(CastMsg_Rtt, OnRtt)
    IPC_MESSAGE_HANDLER(CastMsg_RtcpCastMessage, OnRtcpCastMessage)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP();
  return handled;
}

void CastIPCDispatcher::OnFilterAdded(IPC::Sender* sender) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  DCHECK(!global_instance_);
  global_instance_ = this;
  sender_ = sender;
}

void CastIPCDispatcher::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(this, global_instance_);
  global_instance_ = NULL;
  sender_ = NULL;
}

void CastIPCDispatcher::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(this, global_instance_);
}

void CastIPCDispatcher::OnNotifyStatusChange(
    int32 channel_id,
    media::cast::CastTransportStatus status) {
  CastTransportSenderIPC* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->OnNotifyStatusChange(status);
  } else {
    DVLOG(1)
        << "CastIPCDispatcher::OnNotifystatusChange on non-existing channel.";
  }
}

void CastIPCDispatcher::OnRawEvents(
    int32 channel_id,
    const std::vector<media::cast::PacketEvent>& packet_events,
    const std::vector<media::cast::FrameEvent>& frame_events) {
  CastTransportSenderIPC* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->OnRawEvents(packet_events, frame_events);
  } else {
    DVLOG(1) << "CastIPCDispatcher::OnRawEvents on non-existing channel.";
  }
}

void CastIPCDispatcher::OnRtt(int32 channel_id,
                              uint32 ssrc,
                              const media::cast::RtcpRttReport& rtt_report) {
  CastTransportSenderIPC* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->OnRtt(ssrc, rtt_report);
  } else {
    DVLOG(1) << "CastIPCDispatcher::OnRtt on non-existing channel.";
  }
}

void CastIPCDispatcher::OnRtcpCastMessage(
    int32 channel_id,
    uint32 ssrc,
    const media::cast::RtcpCastMessage& cast_message) {
  CastTransportSenderIPC* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->OnRtcpCastMessage(ssrc, cast_message);
  } else {
    DVLOG(1) << "CastIPCDispatcher::OnRtt on non-existing channel.";
  }
}
