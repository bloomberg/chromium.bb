// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/host_address_request.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/common/p2p_messages.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "jingle/glue/utils.h"

namespace content {

P2PAsyncAddressResolver::P2PAsyncAddressResolver(
    P2PSocketDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      ipc_message_loop_(dispatcher->message_loop()),
      delegate_message_loop_(base::MessageLoopProxy::current()),
      state_(STATE_CREATED),
      request_id_(0),
      registered_(false) {
  AddRef();  // Balanced in Destroy().
}

P2PAsyncAddressResolver::~P2PAsyncAddressResolver() {
  DCHECK(state_ == STATE_CREATED || state_ == STATE_FINISHED);
  DCHECK(!registered_);
}

void P2PAsyncAddressResolver::Start(const talk_base::SocketAddress& host_name) {
  DCHECK(delegate_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(STATE_CREATED, state_);

  state_ = STATE_SENT;
  addr_ = host_name;
  ipc_message_loop_->PostTask(FROM_HERE, base::Bind(
      &P2PAsyncAddressResolver::DoSendRequest, this, host_name));
}

bool P2PAsyncAddressResolver::GetResolvedAddress(
  int family, talk_base::SocketAddress* addr) const {
  DCHECK(delegate_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(STATE_FINISHED, state_);

  if (addresses_.empty())
    return false;

  *addr = addr_;
  for (size_t i = 0; i < addresses_.size(); ++i) {
    if (family == addresses_[i].family()) {
      addr->SetIP(addresses_[i]);
      return true;
    }
  }
  return false;
}

int P2PAsyncAddressResolver::GetError() const {
  return addresses_.empty() ? -1 : 0;
}

void P2PAsyncAddressResolver::Destroy(bool wait) {
  DCHECK(delegate_message_loop_->BelongsToCurrentThread());

  if (state_ != STATE_FINISHED) {
    state_ = STATE_FINISHED;
    ipc_message_loop_->PostTask(FROM_HERE, base::Bind(
        &P2PAsyncAddressResolver::DoUnregister, this));
  }
  Release();
}

void P2PAsyncAddressResolver::DoSendRequest(
    const talk_base::SocketAddress& host_name) {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());

  request_id_ = dispatcher_->RegisterHostAddressRequest(this);
  registered_ = true;
  dispatcher_->SendP2PMessage(
      new P2PHostMsg_GetHostAddress(host_name.hostname(), request_id_));
}

void P2PAsyncAddressResolver::DoUnregister() {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());
  if (registered_) {
    dispatcher_->UnregisterHostAddressRequest(request_id_);
    registered_ = false;
  }
}

void P2PAsyncAddressResolver::OnResponse(const net::IPAddressList& addresses) {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());
  DCHECK(registered_);

  dispatcher_->UnregisterHostAddressRequest(request_id_);
  registered_ = false;

  delegate_message_loop_->PostTask(FROM_HERE, base::Bind(
      &P2PAsyncAddressResolver::DeliverResponse, this, addresses));
}

void P2PAsyncAddressResolver::DeliverResponse(
    const net::IPAddressList& addresses) {
  DCHECK(delegate_message_loop_->BelongsToCurrentThread());
  if (state_ == STATE_SENT) {
    for (size_t i = 0; i < addresses.size(); ++i) {
      talk_base::SocketAddress socket_address;
      if (!jingle_glue::IPEndPointToSocketAddress(
              net::IPEndPoint(addresses[i], 0), &socket_address)) {
        NOTREACHED();
      }
      addresses_.push_back(socket_address.ipaddr());
    }
    state_ = STATE_FINISHED;
    SignalDone(this);
  }
}

}  // namespace content
