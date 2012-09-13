// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_tcp_server_socket.h"

#include <cstddef>

#include "base/logging.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"

using content::BrowserThread;
using ppapi::NetAddressPrivateImpl;

PepperTCPServerSocket::PepperTCPServerSocket(
    PepperMessageFilter* manager,
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    PP_Resource socket_resource,
    uint32 socket_id)
    : manager_(manager),
      routing_id_(routing_id),
      plugin_dispatcher_id_(plugin_dispatcher_id),
      socket_resource_(socket_resource),
      socket_id_(socket_id),
      state_(BEFORE_LISTENING) {
  DCHECK(manager);
}

PepperTCPServerSocket::~PepperTCPServerSocket() {
}

void PepperTCPServerSocket::Listen(const PP_NetAddress_Private& addr,
                                   int32 backlog) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::IPAddressNumber address;
  int port;
  if (state_ != BEFORE_LISTENING ||
      !NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    CancelListenRequest();
    return;
  }

  state_ = LISTEN_IN_PROGRESS;

  socket_.reset(new net::TCPServerSocket(NULL, net::NetLog::Source()));
  int result = socket_->Listen(net::IPEndPoint(address, port), backlog);
  if (result != net::ERR_IO_PENDING)
    OnListenCompleted(result);
}

void PepperTCPServerSocket::Accept(int32 tcp_client_socket_routing_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (state_ != LISTENING) {
    SendAcceptACKError();
    return;
  }

  state_ = ACCEPT_IN_PROGRESS;

  int result = socket_->Accept(
      &socket_buffer_,
      base::Bind(&PepperTCPServerSocket::OnAcceptCompleted,
                 base::Unretained(this),
                 tcp_client_socket_routing_id));
  if (result != net::ERR_IO_PENDING)
    OnAcceptCompleted(tcp_client_socket_routing_id, result);
}

void PepperTCPServerSocket::CancelListenRequest() {
  manager_->Send(new PpapiMsg_PPBTCPServerSocket_ListenACK(
      routing_id_,
      plugin_dispatcher_id_,
      socket_resource_,
      0,
      PP_ERROR_FAILED));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&PepperMessageFilter::RemoveTCPServerSocket, manager_,
                 socket_id_));
}

void PepperTCPServerSocket::SendAcceptACKError() {
  manager_->Send(new PpapiMsg_PPBTCPServerSocket_AcceptACK(
      routing_id_,
      plugin_dispatcher_id_,
      socket_id_,
      0,
      NetAddressPrivateImpl::kInvalidNetAddress,
      NetAddressPrivateImpl::kInvalidNetAddress));
}

void PepperTCPServerSocket::OnListenCompleted(int result) {
  DCHECK(state_ == LISTEN_IN_PROGRESS && socket_.get());

  if (result != net::OK) {
    CancelListenRequest();
  } else {
    manager_->Send(new PpapiMsg_PPBTCPServerSocket_ListenACK(
        routing_id_,
        plugin_dispatcher_id_,
        socket_resource_,
        socket_id_,
        PP_OK));
    state_ = LISTENING;
  }
}

void PepperTCPServerSocket::OnAcceptCompleted(
    int32 tcp_client_socket_routing_id,
    int result) {
  DCHECK(state_ == ACCEPT_IN_PROGRESS && socket_buffer_.get());

  if (result != net::OK) {
    SendAcceptACKError();
  } else {
    scoped_ptr<net::StreamSocket> socket(socket_buffer_.release());

    net::IPEndPoint ip_end_point_local;
    net::IPEndPoint ip_end_point_remote;
    PP_NetAddress_Private local_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;
    PP_NetAddress_Private remote_addr =
        NetAddressPrivateImpl::kInvalidNetAddress;

    if (socket->GetLocalAddress(&ip_end_point_local) != net::OK ||
        !NetAddressPrivateImpl::IPEndPointToNetAddress(
            ip_end_point_local.address(),
            ip_end_point_local.port(),
            &local_addr) ||
        socket->GetPeerAddress(&ip_end_point_remote) != net::OK ||
        !NetAddressPrivateImpl::IPEndPointToNetAddress(
            ip_end_point_remote.address(),
            ip_end_point_remote.port(),
            &remote_addr)) {
      SendAcceptACKError();
    } else {
      uint32 accepted_socket_id =
          manager_->AddAcceptedTCPSocket(tcp_client_socket_routing_id,
                                         plugin_dispatcher_id_,
                                         socket.release());
      if (accepted_socket_id != 0) {
        manager_->Send(new PpapiMsg_PPBTCPServerSocket_AcceptACK(
            routing_id_,
            plugin_dispatcher_id_,
            socket_id_,
            accepted_socket_id,
            local_addr,
            remote_addr));
      } else {
        SendAcceptACKError();
      }
    }
  }

  state_ = LISTENING;
}
