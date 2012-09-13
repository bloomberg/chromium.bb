// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_udp_socket.h"

#include <string.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/udp/udp_server_socket.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/private/net_address_private_impl.h"

using content::BrowserThread;
using ppapi::NetAddressPrivateImpl;

PepperUDPSocket::PepperUDPSocket(
    PepperMessageFilter* manager,
    int32 routing_id,
    uint32 plugin_dispatcher_id,
    uint32 socket_id)
    : manager_(manager),
      routing_id_(routing_id),
      plugin_dispatcher_id_(plugin_dispatcher_id),
      socket_id_(socket_id),
      allow_address_reuse_(false),
      allow_broadcast_(false) {
  DCHECK(manager);
}

PepperUDPSocket::~PepperUDPSocket() {
  // Make sure there are no further callbacks from socket_.
  if (socket_.get())
    socket_->Close();
}

void PepperUDPSocket::AllowAddressReuse(bool value) {
  allow_address_reuse_ = value;
}

void PepperUDPSocket::AllowBroadcast(bool value) {
  allow_broadcast_ = value;
}

void PepperUDPSocket::Bind(const PP_NetAddress_Private& addr) {
  socket_.reset(new net::UDPServerSocket(NULL, net::NetLog::Source()));

  net::IPAddressNumber address;
  int port;
  if (!socket_.get() ||
      !NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    SendBindACKError();
    return;
  }

  if (allow_address_reuse_)
    socket_->AllowAddressReuse();
  if (allow_broadcast_)
    socket_->AllowBroadcast();

  int result = socket_->Listen(net::IPEndPoint(address, port));

  if (result == net::OK &&
      socket_->GetLocalAddress(&bound_address_) != net::OK) {
    SendBindACKError();
    return;
  }

  OnBindCompleted(result);
}

void PepperUDPSocket::RecvFrom(int32_t num_bytes) {
  if (recvfrom_buffer_.get()) {
    SendRecvFromACKError();
    return;
  }

  recvfrom_buffer_ = new net::IOBuffer(num_bytes);
  int result = socket_->RecvFrom(
      recvfrom_buffer_, num_bytes, &recvfrom_address_,
      base::Bind(&PepperUDPSocket::OnRecvFromCompleted,
                 base::Unretained(this)));

  if (result != net::ERR_IO_PENDING)
    OnRecvFromCompleted(result);
}

void PepperUDPSocket::SendTo(const std::string& data,
                             const PP_NetAddress_Private& addr) {
  if (sendto_buffer_.get() || data.empty()) {
    SendSendToACKError();
    return;
  }

  net::IPAddressNumber address;
  int port;
  if (!NetAddressPrivateImpl::NetAddressToIPEndPoint(addr, &address, &port)) {
    SendSendToACKError();
    return;
  }

  int data_size = data.size();

  sendto_buffer_ = new net::IOBuffer(data_size);
  memcpy(sendto_buffer_->data(), data.data(), data_size);
  int result = socket_->SendTo(
      sendto_buffer_, data_size, net::IPEndPoint(address, port),
      base::Bind(&PepperUDPSocket::OnSendToCompleted, base::Unretained(this)));

  if (result != net::ERR_IO_PENDING)
    OnSendToCompleted(result);
}

void PepperUDPSocket::SendRecvFromACKError() {
  PP_NetAddress_Private addr = NetAddressPrivateImpl::kInvalidNetAddress;
  manager_->Send(new PpapiMsg_PPBUDPSocket_RecvFromACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, false, std::string(),
      addr));
}

void PepperUDPSocket::SendSendToACKError() {
  manager_->Send(new PpapiMsg_PPBUDPSocket_SendToACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, false, 0));
}

void PepperUDPSocket::SendBindACKError() {
  PP_NetAddress_Private addr = NetAddressPrivateImpl::kInvalidNetAddress;
  manager_->Send(new PpapiMsg_PPBUDPSocket_BindACK(
      routing_id_, plugin_dispatcher_id_, socket_id_, false, addr));
}

void PepperUDPSocket::OnBindCompleted(int result) {
  PP_NetAddress_Private addr = NetAddressPrivateImpl::kInvalidNetAddress;
  if (result < 0 ||
      !NetAddressPrivateImpl::IPEndPointToNetAddress(bound_address_.address(),
                                                     bound_address_.port(),
                                                     &addr)) {
    SendBindACKError();
  } else {
    manager_->Send(new PpapiMsg_PPBUDPSocket_BindACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true, addr));
  }
}

void PepperUDPSocket::OnRecvFromCompleted(int result) {
  DCHECK(recvfrom_buffer_.get());

  // Convert IPEndPoint we get back from RecvFrom to a PP_NetAddress_Private,
  // to send back.
  PP_NetAddress_Private addr = NetAddressPrivateImpl::kInvalidNetAddress;
  if (result < 0 ||
      !NetAddressPrivateImpl::IPEndPointToNetAddress(
          recvfrom_address_.address(),
          recvfrom_address_.port(),
          &addr)) {
    SendRecvFromACKError();
  } else {
    manager_->Send(new PpapiMsg_PPBUDPSocket_RecvFromACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true,
        std::string(recvfrom_buffer_->data(), result), addr));
  }

  recvfrom_buffer_ = NULL;
}

void PepperUDPSocket::OnSendToCompleted(int result) {
  DCHECK(sendto_buffer_.get());

  if (result < 0) {
    SendSendToACKError();
  } else {
    manager_->Send(new PpapiMsg_PPBUDPSocket_SendToACK(
        routing_id_, plugin_dispatcher_id_, socket_id_, true, result));
  }

  sendto_buffer_ = NULL;
}
