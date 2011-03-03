// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/p2p/socket_client.h"

#include "base/message_loop_proxy.h"
#include "chrome/renderer/p2p/socket_dispatcher.h"
#include "content/common/p2p_messages.h"

P2PSocketClient::P2PSocketClient(P2PSocketDispatcher* dispatcher)
    : dispatcher_(dispatcher),
      message_loop_(dispatcher->message_loop()),
      socket_id_(0), delegate_(NULL),
      state_(STATE_UNINITIALIZED) {
}

P2PSocketClient::~P2PSocketClient() {
  DCHECK(state_ == STATE_CLOSED || state_ == STATE_UNINITIALIZED ||
         state_ == STATE_ERROR);
}

void P2PSocketClient::Init(P2PSocketType type, P2PSocketAddress address,
                           P2PSocketClient::Delegate* delegate) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &P2PSocketClient::Init,
                                     type, address, delegate));
    return;
  }

  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  socket_id_ = dispatcher_->RegisterClient(this);
  dispatcher_->SendP2PMessage(
      new P2PHostMsg_CreateSocket(0, type, socket_id_, address));
  state_ = STATE_OPENING;
}

void P2PSocketClient::Send(P2PSocketAddress address,
                           const std::vector<char>& data) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &P2PSocketClient::Send, address,
                                     data));
    return;
  }

  // Can send data only when the socket is open.
  DCHECK_EQ(state_, STATE_OPEN);
  dispatcher_->SendP2PMessage(
      new P2PHostMsg_Send(0, socket_id_, address, data));
}

void P2PSocketClient::Close(Task* closed_task) {
  if (!message_loop_->BelongsToCurrentThread()) {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &P2PSocketClient::Close,
                                     closed_task));
    return;
  }

  if (dispatcher_) {
    if (state_ == STATE_OPEN || state_ == STATE_OPENING) {
      dispatcher_->SendP2PMessage(new P2PHostMsg_DestroySocket(0, socket_id_));
    }
    dispatcher_->UnregisterClient(socket_id_);
  }

  state_ = STATE_CLOSED;

  closed_task->Run();
  delete closed_task;
}

void P2PSocketClient::OnSocketCreated(P2PSocketAddress address) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_OPENING);
  state_ = STATE_OPEN;
  delegate_->OnOpen(address);
}

void P2PSocketClient::OnError() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  state_ = STATE_ERROR;
  delegate_->OnError();
}

void P2PSocketClient::OnDataReceived(P2PSocketAddress address,
                                     const std::vector<char>& data) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK_EQ(STATE_OPEN, state_);
  delegate_->OnDataReceived(address, data);
}

void P2PSocketClient::Detach() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  delegate_ = NULL;
  state_ = STATE_ERROR;
  delegate_->OnError();
}
