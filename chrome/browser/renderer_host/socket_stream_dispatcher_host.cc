// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/socket_stream_dispatcher_host.h"

#include "base/logging.h"
#include "chrome/browser/renderer_host/socket_stream_host.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/net/socket_stream.h"
#include "ipc/ipc_message.h"

SocketStreamDispatcherHost::SocketStreamDispatcherHost()
    : sender_(NULL) {
}

SocketStreamDispatcherHost::~SocketStreamDispatcherHost() {
  // TODO(ukai): Implement IDMap::RemoveAll().
  for (IDMap<SocketStreamHost>::const_iterator iter(&hosts_);
       !iter.IsAtEnd();
       iter.Advance()) {
    int socket_id = iter.GetCurrentKey();
    const SocketStreamHost* socket_stream_host = iter.GetCurrentValue();
    delete socket_stream_host;
    hosts_.Remove(socket_id);
  }
}

void SocketStreamDispatcherHost::Initialize(
    IPC::Message::Sender* sender, int process_id) {
  DLOG(INFO) << "Initialize: SocketStreamDispatcherHost process_id="
             << process_id_;
  DCHECK(sender);
  sender_ = sender;
  process_id_ = process_id;
}

bool SocketStreamDispatcherHost::OnMessageReceived(const IPC::Message& msg,
                                                   bool* msg_ok) {
  DCHECK(sender_);
  *msg_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(SocketStreamDispatcherHost, msg, *msg_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SocketStream_Connect, OnConnect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SocketStream_SendData, OnSendData)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SocketStream_Close, OnCloseReq)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

// SocketStream::Delegate methods implementations.
void SocketStreamDispatcherHost::OnConnected(net::SocketStream* socket,
                                             int max_pending_send_allowed) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DLOG(INFO) << "SocketStreamDispatcherHost::OnConnected socket_id="
             << socket_id
             << " max_pending_send_allowed=" << max_pending_send_allowed;
  if (socket_id == chrome_common_net::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnConnected";
    return;
  }
  if (!sender_->Send(new ViewMsg_SocketStream_Connected(
          socket_id, max_pending_send_allowed))) {
    LOG(ERROR) << "ViewMsg_SocketStream_Connected failed.";
    DeleteSocketStreamHost(socket_id);
  }
}

void SocketStreamDispatcherHost::OnSentData(net::SocketStream* socket,
                                            int amount_sent) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DLOG(INFO) << "SocketStreamDispatcherHost::OnSentData socket_id="
             << socket_id
             << " amount_sent=" << amount_sent;
  if (socket_id == chrome_common_net::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnReceivedData";
    return;
  }
  if (!sender_->Send(
          new ViewMsg_SocketStream_SentData(socket_id, amount_sent))) {
    LOG(ERROR) << "ViewMsg_SocketStream_SentData failed.";
    DeleteSocketStreamHost(socket_id);
  }
}

void SocketStreamDispatcherHost::OnReceivedData(
    net::SocketStream* socket, const char* data, int len) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DLOG(INFO) << "SocketStreamDispatcherHost::OnReceiveData socket_id="
             << socket_id;
  if (socket_id == chrome_common_net::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnReceivedData";
    return;
  }
  if (!sender_->Send(new ViewMsg_SocketStream_ReceivedData(
          socket_id, std::vector<char>(data, data + len)))) {
    LOG(ERROR) << "ViewMsg_SocketStream_ReceivedData failed.";
    DeleteSocketStreamHost(socket_id);
  }
}

void SocketStreamDispatcherHost::OnClose(net::SocketStream* socket) {
  int socket_id = SocketStreamHost::SocketIdFromSocketStream(socket);
  DLOG(INFO) << "SocketStreamDispatcherHost::OnClosed socket_id="
             << socket_id;
  if (socket_id == chrome_common_net::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnClose";
    return;
  }
  DeleteSocketStreamHost(socket_id);
}

// Message handlers called by OnMessageReceived.
void SocketStreamDispatcherHost::OnConnect(const GURL& url, int socket_id) {
  DLOG(INFO) << "SocketStreamDispatcherHost::OnConnect url=" << url
             << " socket_id=" << socket_id;
  DCHECK_NE(chrome_common_net::kNoSocketId, socket_id);
  if (hosts_.Lookup(socket_id)) {
    LOG(ERROR) << "socket_id=" << socket_id << " already registered.";
    return;
  }
  SocketStreamHost* socket_stream_host = new SocketStreamHost(this, socket_id);
  hosts_.AddWithID(socket_stream_host, socket_id);
  socket_stream_host->Connect(url);
  DLOG(INFO) << "SocketStreamDispatcherHost::OnConnect -> " << socket_id;
}

void SocketStreamDispatcherHost::OnSendData(
    int socket_id, const std::vector<char>& data) {
  DLOG(INFO) << "SocketStreamDispatcherHost::OnSendData socket_id="
             << socket_id;
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  if (!socket_stream_host) {
    LOG(ERROR) << "socket_id=" << socket_id << " already closed.";
    return;
  }
  if (!socket_stream_host->SendData(data)) {
    // Cannot accept more data to send.
    socket_stream_host->Close();
  }
}

void SocketStreamDispatcherHost::OnCloseReq(int socket_id) {
  DLOG(INFO) << "SocketStreamDispatcherHost::OnCloseReq socket_id="
             << socket_id;
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  if (!socket_stream_host)
    return;
  socket_stream_host->Close();
}

void SocketStreamDispatcherHost::DeleteSocketStreamHost(int socket_id) {
  SocketStreamHost* socket_stream_host = hosts_.Lookup(socket_id);
  DCHECK(socket_stream_host);
  delete socket_stream_host;
  hosts_.Remove(socket_id);
  if (!sender_->Send(new ViewMsg_SocketStream_Closed(socket_id))) {
    LOG(ERROR) << "ViewMsg_SocketStream_Closed failed.";
  }
}
