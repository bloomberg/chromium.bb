// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_IPC_DISPATCHER_H_
#define CHROME_RENDERER_MEDIA_CAST_IPC_DISPATCHER_H_

#include "base/callback.h"
#include "base/id_map.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/cast/cast_sender.h"
#include "media/cast/transport/cast_transport_sender.h"

class CastTransportSenderIPC;

// This dispatcher listens to incoming IPC messages and sends
// the call to the correct CastTransportSenderIPC instance.
class CastIPCDispatcher : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit CastIPCDispatcher(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  static CastIPCDispatcher* Get();
  void Send(IPC::Message* message);
  int32 AddSender(CastTransportSenderIPC* sender);
  void RemoveSender(int32 channel_id);

  // IPC::ChannelProxy::MessageFilter implementation
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

 protected:
  virtual ~CastIPCDispatcher();

 private:
  void OnReceivedPacket(int32 channel_id, const media::cast::Packet& packet);
  void OnNotifyStatusChange(
      int32 channel_id,
      media::cast::transport::CastTransportStatus status);
  void OnRtpStatistics(
      int32 channel_id,
      bool audio,
      const media::cast::transport::RtcpSenderInfo& sender_info,
      base::TimeTicks time_sent,
      uint32 rtp_timestamp);

  static CastIPCDispatcher* global_instance_;

  // IPC channel for Send(); must only be accesed on |io_message_loop_|.
  IPC::Channel* channel_;

  // Message loop on which IPC calls are driven.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // A map of stream ids to delegates; must only be accessed on
  // |io_message_loop_|.
  IDMap<CastTransportSenderIPC> id_map_;
  DISALLOW_COPY_AND_ASSIGN(CastIPCDispatcher);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_IPC_DISPATCHER_H_
