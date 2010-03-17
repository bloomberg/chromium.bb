// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/socket_stream_dispatcher_host.h"

#include "base/logging.h"
#include "chrome/browser/renderer_host/socket_stream_host.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/net/socket_stream.h"
#include "ipc/ipc_message.h"
#include "net/websockets/websocket_job.h"
#include "net/websockets/websocket_throttle.h"

SocketStreamDispatcherHost::SocketStreamDispatcherHost() : receiver_(NULL) {
  net::WebSocketJob::EnsureInit();
}

SocketStreamDispatcherHost::~SocketStreamDispatcherHost() {
  // TODO(ukai): Implement IDMap::RemoveAll().
  for (IDMap< IDMap<SocketStreamHost> >::const_iterator iter(&hostmap_);
       !iter.IsAtEnd();
       iter.Advance()) {
    int host_id = iter.GetCurrentKey();
    CancelRequestsForProcess(host_id);
  }
}

bool SocketStreamDispatcherHost::OnMessageReceived(
    const IPC::Message& msg,
    ResourceDispatcherHost::Receiver* receiver,
    bool* msg_ok) {
  if (!IsSocketStreamDispatcherHostMessage(msg))
    return false;

  *msg_ok = true;
  bool handled = true;
  receiver_ = receiver;
  IPC_BEGIN_MESSAGE_MAP_EX(SocketStreamDispatcherHost, msg, *msg_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SocketStream_Connect, OnConnect)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SocketStream_SendData, OnSendData)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SocketStream_Close, OnCloseReq)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  receiver_ = NULL;
  return handled;
}

void SocketStreamDispatcherHost::CancelRequestsForProcess(int host_id) {
  IDMap<SocketStreamHost>* hosts = hostmap_.Lookup(host_id);
  if (hosts == NULL)
    return;
  for (IDMap<SocketStreamHost>::const_iterator hosts_iter(hosts);
       !hosts_iter.IsAtEnd();
       hosts_iter.Advance()) {
    const SocketStreamHost* socket_stream_host = hosts_iter.GetCurrentValue();
    delete socket_stream_host;
    int socket_id = hosts_iter.GetCurrentKey();
    hosts->Remove(socket_id);
  }
  hostmap_.Remove(host_id);
  delete hosts;
}

// SocketStream::Delegate methods implementations.
void SocketStreamDispatcherHost::OnConnected(net::SocketStream* socket,
                                             int max_pending_send_allowed) {
  SocketStreamHost* socket_stream_host =
      SocketStreamHost::GetSocketStreamHost(socket);
  DCHECK(socket_stream_host);
  int socket_id = socket_stream_host->socket_id();
  DLOG(INFO) << "SocketStreamDispatcherHost::OnConnected socket_id="
             << socket_id
             << " max_pending_send_allowed=" << max_pending_send_allowed;
  if (socket_id == chrome_common_net::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnConnected";
    return;
  }
  if (!socket_stream_host->Connected(max_pending_send_allowed)) {
    LOG(ERROR) << "ViewMsg_SocketStream_Connected failed.";
    DeleteSocketStreamHost(socket_stream_host->receiver()->id(), socket_id);
  }
}

void SocketStreamDispatcherHost::OnSentData(net::SocketStream* socket,
                                            int amount_sent) {
  SocketStreamHost* socket_stream_host =
      SocketStreamHost::GetSocketStreamHost(socket);
  DCHECK(socket_stream_host);
  int socket_id = socket_stream_host->socket_id();
  DLOG(INFO) << "SocketStreamDispatcherHost::OnSentData socket_id="
             << socket_id
             << " amount_sent=" << amount_sent;
  if (socket_id == chrome_common_net::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnReceivedData";
    return;
  }
  if (!socket_stream_host->SentData(amount_sent)) {
    LOG(ERROR) << "ViewMsg_SocketStream_SentData failed.";
    DeleteSocketStreamHost(socket_stream_host->receiver()->id(), socket_id);
  }
}

void SocketStreamDispatcherHost::OnReceivedData(
    net::SocketStream* socket, const char* data, int len) {
  SocketStreamHost* socket_stream_host =
      SocketStreamHost::GetSocketStreamHost(socket);
  DCHECK(socket_stream_host);
  int socket_id = socket_stream_host->socket_id();
  DLOG(INFO) << "SocketStreamDispatcherHost::OnReceiveData socket_id="
             << socket_id;
  if (socket_id == chrome_common_net::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnReceivedData";
    return;
  }
  if (!socket_stream_host->ReceivedData(data, len)) {
    LOG(ERROR) << "ViewMsg_SocketStream_ReceivedData failed.";
    DeleteSocketStreamHost(socket_stream_host->receiver()->id(), socket_id);
  }
}

void SocketStreamDispatcherHost::OnClose(net::SocketStream* socket) {
  SocketStreamHost* socket_stream_host =
      SocketStreamHost::GetSocketStreamHost(socket);
  DCHECK(socket_stream_host);
  int socket_id = socket_stream_host->socket_id();
  DLOG(INFO) << "SocketStreamDispatcherHost::OnClosed socket_id="
             << socket_id;
  if (socket_id == chrome_common_net::kNoSocketId) {
    LOG(ERROR) << "NoSocketId in OnClose";
    return;
  }
  DeleteSocketStreamHost(socket_stream_host->receiver()->id(), socket_id);
}

// Message handlers called by OnMessageReceived.
void SocketStreamDispatcherHost::OnConnect(const GURL& url, int socket_id) {
  DLOG(INFO) << "SocketStreamDispatcherHost::OnConnect url=" << url
             << " socket_id=" << socket_id;
  DCHECK_NE(chrome_common_net::kNoSocketId, socket_id);
  DCHECK(receiver_);
  if (LookupHostMap(receiver_->id(), socket_id)) {
    LOG(ERROR) << "host_id=" << receiver_->id()
               << " socket_id=" << socket_id << " already registered.";
    return;
  }
  SocketStreamHost* socket_stream_host =
      new SocketStreamHost(this, receiver_, socket_id);
  AddHostMap(receiver_->id(), socket_id, socket_stream_host);
  socket_stream_host->Connect(url);
  DLOG(INFO) << "SocketStreamDispatcherHost::OnConnect -> " << socket_id;
}

void SocketStreamDispatcherHost::OnSendData(
    int socket_id, const std::vector<char>& data) {
  DLOG(INFO) << "SocketStreamDispatcherHost::OnSendData socket_id="
             << socket_id;
  DCHECK(receiver_);
  SocketStreamHost* socket_stream_host =
      LookupHostMap(receiver_->id(), socket_id);
  if (!socket_stream_host) {
    LOG(ERROR) << "host_id=" << receiver_->id()
               << " socket_id=" << socket_id << " already closed.";
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
  DCHECK(receiver_);
  SocketStreamHost* socket_stream_host =
      LookupHostMap(receiver_->id(), socket_id);
  if (!socket_stream_host)
    return;
  socket_stream_host->Close();
}

void SocketStreamDispatcherHost::DeleteSocketStreamHost(
    int host_id, int socket_id) {
  SocketStreamHost* socket_stream_host = LookupHostMap(host_id, socket_id);
  DCHECK(socket_stream_host);
  delete socket_stream_host;
  IDMap<SocketStreamHost>* hosts = hostmap_.Lookup(host_id);
  DCHECK(hosts);
  hosts->Remove(socket_id);
  if (hosts->IsEmpty()) {
    hostmap_.Remove(host_id);
    delete hosts;
  }
}

void SocketStreamDispatcherHost::AddHostMap(
    int host_id, int socket_id, SocketStreamHost* socket_stream_host) {
  IDMap<SocketStreamHost>* hosts = hostmap_.Lookup(host_id);
  if (!hosts) {
    hosts = new IDMap<SocketStreamHost>;
    hostmap_.AddWithID(hosts, host_id);
  }
  hosts->AddWithID(socket_stream_host, socket_id);
}

SocketStreamHost* SocketStreamDispatcherHost::LookupHostMap(
    int host_id, int socket_id) {
  IDMap<SocketStreamHost>* hosts = hostmap_.Lookup(host_id);
  if (!hosts)
    return NULL;
  return hosts->Lookup(socket_id);
}

/* static */
bool SocketStreamDispatcherHost::IsSocketStreamDispatcherHostMessage(
    const IPC::Message& message) {
  switch (message.type()) {
    case ViewHostMsg_SocketStream_Connect::ID:
    case ViewHostMsg_SocketStream_SendData::ID:
    case ViewHostMsg_SocketStream_Close::ID:
      return true;

    default:
      break;
  }
  return false;
}
