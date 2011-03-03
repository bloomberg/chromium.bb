// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/p2p/socket_client.h"

#include "base/message_loop_proxy.h"
#include "chrome/renderer/p2p/socket_dispatcher.h"
#include "content/common/p2p_messages.h"

P2PSocketClient::P2PSocketClient(P2PSocketDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      ipc_message_loop_(dispatcher->message_loop()),
      delegate_message_loop_(NULL),
      socket_id_(0), delegate_(NULL),
      state_(STATE_UNINITIALIZED) {
}

P2PSocketClient::~P2PSocketClient() {
  DCHECK(state_ == STATE_CLOSED || state_ == STATE_UNINITIALIZED ||
         state_ == STATE_ERROR);
}

void P2PSocketClient::Init(
    P2PSocketType type, const P2PSocketAddress& address,
    P2PSocketClient::Delegate* delegate,
    scoped_refptr<base::MessageLoopProxy> delegate_loop) {
  if (!ipc_message_loop_->BelongsToCurrentThread()) {
    ipc_message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &P2PSocketClient::Init,
                                     type, address, delegate, delegate_loop));
    return;
  }

  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  state_ = STATE_OPENING;
  delegate_ = delegate;
  delegate_message_loop_ = delegate_loop;
  socket_id_ = dispatcher_->RegisterClient(this);
  dispatcher_->SendP2PMessage(
      new P2PHostMsg_CreateSocket(0, type, socket_id_, address));
}

void P2PSocketClient::Send(const P2PSocketAddress& address,
                           const std::vector<char>& data) {
  if (!ipc_message_loop_->BelongsToCurrentThread()) {
    ipc_message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &P2PSocketClient::Send, address,
                                     data));
    return;
  }

  // Can send data only when the socket is open.
  DCHECK_EQ(state_, STATE_OPEN);
  dispatcher_->SendP2PMessage(
      new P2PHostMsg_Send(0, socket_id_, address, data));
}

void P2PSocketClient::Close() {
  DCHECK(delegate_message_loop_->BelongsToCurrentThread());

  delegate_ = NULL;

  ipc_message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &P2PSocketClient::DoClose));
}

void P2PSocketClient::DoClose() {
  if (dispatcher_) {
    if (state_ == STATE_OPEN || state_ == STATE_OPENING) {
      dispatcher_->SendP2PMessage(new P2PHostMsg_DestroySocket(0, socket_id_));
    }
    dispatcher_->UnregisterClient(socket_id_);
  }

  state_ = STATE_CLOSED;
}

void P2PSocketClient::OnSocketCreated(const P2PSocketAddress& address) {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_OPENING);
  state_ = STATE_OPEN;

  delegate_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &P2PSocketClient::DeliverOnSocketCreated, address));
}

void P2PSocketClient::DeliverOnSocketCreated(const P2PSocketAddress& address) {
  if (delegate_)
    delegate_->OnOpen(address);
}

void P2PSocketClient::OnError() {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());
  state_ = STATE_ERROR;

  delegate_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &P2PSocketClient::DeliverOnError));
}

void P2PSocketClient::DeliverOnError() {
  if (delegate_)
    delegate_->OnError();
}

void P2PSocketClient::OnDataReceived(const P2PSocketAddress& address,
                                     const std::vector<char>& data) {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(STATE_OPEN, state_);
  delegate_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &P2PSocketClient::DeliverOnDataReceived, address, data));
}

void P2PSocketClient::DeliverOnDataReceived(const P2PSocketAddress& address,
                                            const std::vector<char>& data) {
  if (delegate_)
    delegate_->OnDataReceived(address, data);
}

void P2PSocketClient::Detach() {
  DCHECK(ipc_message_loop_->BelongsToCurrentThread());
  dispatcher_ = NULL;
  OnError();
}
