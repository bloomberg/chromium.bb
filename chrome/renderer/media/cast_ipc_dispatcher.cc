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
    : channel_(NULL),
      io_message_loop_(io_message_loop) {
  DCHECK(io_message_loop_);
  DCHECK(!global_instance_);
}

CastIPCDispatcher::~CastIPCDispatcher() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  // Unfortunately, you do not always get a OnFilterRemoved call.
  global_instance_ = NULL;
}

CastIPCDispatcher* CastIPCDispatcher::Get() {
  return global_instance_;
}

void CastIPCDispatcher::Send(IPC::Message* message) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  if (channel_) {
    channel_->Send(message);
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
    IPC_MESSAGE_HANDLER(CastMsg_ReceivedPacket, OnReceivedPacket)
    IPC_MESSAGE_HANDLER(CastMsg_NotifyStatusChange, OnNotifyStatusChange)
    IPC_MESSAGE_HANDLER(CastMsg_RtpStatistics, OnRtpStatistics)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP();
  return handled;
}

void CastIPCDispatcher::OnFilterAdded(IPC::Channel* channel) {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  DCHECK(!global_instance_);
  global_instance_ = this;
  channel_ = channel;
}

void CastIPCDispatcher::OnFilterRemoved() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(this, global_instance_);
  global_instance_ = NULL;
  channel_ = NULL;
}

void CastIPCDispatcher::OnChannelClosing() {
  DCHECK(io_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(this, global_instance_);
  channel_ = NULL;
}

void CastIPCDispatcher::OnReceivedPacket(
    int32 channel_id,
    const media::cast::transport::Packet& packet) {
  CastTransportSenderIPC* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->OnReceivedPacket(packet);
  } else {
    LOG(ERROR) << "CastIPCDispatcher::OnReceivedPacket "
               << "on non-existing channel.";
  }
}

void CastIPCDispatcher::OnNotifyStatusChange(
    int32 channel_id,
    media::cast::transport::CastTransportStatus status) {
  CastTransportSenderIPC* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->OnNotifyStatusChange(status);
  } else {
    LOG(ERROR)
        << "CastIPCDispatcher::OnNotifystatusChange on non-existing channel.";
  }
}

void CastIPCDispatcher::OnRtpStatistics(
    int32 channel_id,
    bool audio,
    const media::cast::transport::RtcpSenderInfo& sender_info,
    base::TimeTicks time_sent,
    uint32 rtp_timestamp) {
  CastTransportSenderIPC* sender = id_map_.Lookup(channel_id);
  if (sender) {
    sender->OnRtpStatistics(audio, sender_info, time_sent, rtp_timestamp);
  } else {
    LOG(ERROR)
        << "CastIPCDispatcher::OnNotifystatusChange on non-existing channel.";
  }
}
