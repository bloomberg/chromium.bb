// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/socket_dispatcher.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "content/common/p2p_messages.h"
#include "content/renderer/p2p/host_address_request.h"
#include "content/renderer/p2p/socket_client.h"
#include "content/renderer/render_view_impl.h"
#include "webkit/glue/network_list_observer.h"

namespace content {

class P2PSocketDispatcher::AsyncMessageSender
    : public base::RefCountedThreadSafe<AsyncMessageSender> {
 public:
  explicit AsyncMessageSender(IPC::Sender* message_sender)
      : message_loop_(base::MessageLoopProxy::current()),
        message_sender_(message_sender) {
  }

  void Detach() {
    DCHECK(message_loop_->BelongsToCurrentThread());
    message_sender_ = NULL;
  }

  void Send(IPC::Message* msg) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&AsyncMessageSender::DoSend, this, msg));
  }

 private:
  friend class base::RefCountedThreadSafe<AsyncMessageSender>;
  ~AsyncMessageSender() {}

  void DoSend(IPC::Message* msg) {
    DCHECK(message_loop_->BelongsToCurrentThread());
    if (message_sender_)
      message_sender_->Send(msg);
  }

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  IPC::Sender* message_sender_;

  DISALLOW_COPY_AND_ASSIGN(AsyncMessageSender);
};

P2PSocketDispatcher::P2PSocketDispatcher(RenderViewImpl* render_view)
    : content::RenderViewObserver(render_view),
      message_loop_(base::MessageLoopProxy::current()),
      network_notifications_started_(false),
      network_list_observers_(
          new ObserverListThreadSafe<webkit_glue::NetworkListObserver>()),
      async_message_sender_(new AsyncMessageSender(this)) {
}

P2PSocketDispatcher::~P2PSocketDispatcher() {
  FOR_EACH_OBSERVER(P2PSocketDispatcherDestructionObserver,
                    destruction_observer_, OnSocketDispatcherDestroyed());
  network_list_observers_->AssertEmpty();
  if (network_notifications_started_)
    Send(new P2PHostMsg_StopNetworkNotifications(routing_id()));
  for (IDMap<P2PSocketClient>::iterator i(&clients_); !i.IsAtEnd();
       i.Advance()) {
    i.GetCurrentValue()->Detach();
  }
  async_message_sender_->Detach();
}

void P2PSocketDispatcher::AddNetworkListObserver(
    webkit_glue::NetworkListObserver* network_list_observer) {
  network_list_observers_->AddObserver(network_list_observer);
  network_notifications_started_ = true;
  async_message_sender_->Send(
      new P2PHostMsg_StartNetworkNotifications(routing_id()));
}

void P2PSocketDispatcher::RemoveNetworkListObserver(
    webkit_glue::NetworkListObserver* network_list_observer) {
  network_list_observers_->RemoveObserver(network_list_observer);
}

void P2PSocketDispatcher::AddDestructionObserver(
    P2PSocketDispatcherDestructionObserver* observer) {
  destruction_observer_.AddObserver(observer);
}

void P2PSocketDispatcher::RemoveDestructionObserver(
    P2PSocketDispatcherDestructionObserver* observer) {
  destruction_observer_.RemoveObserver(observer);
}

bool P2PSocketDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(P2PSocketDispatcher, message)
    IPC_MESSAGE_HANDLER(P2PMsg_NetworkListChanged, OnNetworkListChanged)
    IPC_MESSAGE_HANDLER(P2PMsg_GetHostAddressResult, OnGetHostAddressResult)
    IPC_MESSAGE_HANDLER(P2PMsg_OnSocketCreated, OnSocketCreated)
    IPC_MESSAGE_HANDLER(P2PMsg_OnIncomingTcpConnection, OnIncomingTcpConnection)
    IPC_MESSAGE_HANDLER(P2PMsg_OnError, OnError)
    IPC_MESSAGE_HANDLER(P2PMsg_OnDataReceived, OnDataReceived)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

base::MessageLoopProxy* P2PSocketDispatcher::message_loop() {
  return message_loop_;
}

int P2PSocketDispatcher::RegisterClient(P2PSocketClient* client) {
  return clients_.Add(client);
}

void P2PSocketDispatcher::UnregisterClient(int id) {
  clients_.Remove(id);
}

void P2PSocketDispatcher::SendP2PMessage(IPC::Message* msg) {
  msg->set_routing_id(routing_id());
  Send(msg);
}

int P2PSocketDispatcher::RegisterHostAddressRequest(
    P2PHostAddressRequest* request) {
  return host_address_requests_.Add(request);
}

void P2PSocketDispatcher::UnregisterHostAddressRequest(int id) {
  host_address_requests_.Remove(id);
}

void P2PSocketDispatcher::OnNetworkListChanged(
    const net::NetworkInterfaceList& networks) {
  network_list_observers_->Notify(
      &webkit_glue::NetworkListObserver::OnNetworkListChanged, networks);
}

void P2PSocketDispatcher::OnGetHostAddressResult(
    int32 request_id,
    const net::IPAddressNumber& address) {
  P2PHostAddressRequest* request = host_address_requests_.Lookup(request_id);
  if (!request) {
    VLOG(1) << "Received P2P message for socket that doesn't exist.";
    return;
  }

  request->OnResponse(address);
}

void P2PSocketDispatcher::OnSocketCreated(
    int socket_id, const net::IPEndPoint& address) {
  P2PSocketClient* client = GetClient(socket_id);
  if (client) {
    client->OnSocketCreated(address);
  }
}

void P2PSocketDispatcher::OnIncomingTcpConnection(
    int socket_id, const net::IPEndPoint& address) {
  P2PSocketClient* client = GetClient(socket_id);
  if (client) {
    client->OnIncomingTcpConnection(address);
  }
}

void P2PSocketDispatcher::OnError(int socket_id) {
  P2PSocketClient* client = GetClient(socket_id);
  if (client) {
    client->OnError();
  }
}

void P2PSocketDispatcher::OnDataReceived(
    int socket_id, const net::IPEndPoint& address,
    const std::vector<char>& data) {
  P2PSocketClient* client = GetClient(socket_id);
  if (client) {
    client->OnDataReceived(address, data);
  }
}

P2PSocketClient* P2PSocketDispatcher::GetClient(int socket_id) {
  P2PSocketClient* client = clients_.Lookup(socket_id);
  if (client == NULL) {
    // This may happen if the socket was closed, but the browser side
    // hasn't processed the close message by the time it sends the
    // message to the renderer.
    VLOG(1) << "Received P2P message for socket that doesn't exist.";
    return NULL;
  }

  return client;
}

}  // namespace content
