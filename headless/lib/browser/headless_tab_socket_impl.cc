// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_tab_socket_impl.h"

namespace headless {

HeadlessTabSocketImpl::HeadlessTabSocketImpl() : listener_(nullptr) {}

HeadlessTabSocketImpl::~HeadlessTabSocketImpl() {}

void HeadlessTabSocketImpl::SendMessageToTab(const std::string& message) {
  AwaitNextMessageFromEmbedderCallback callback;

  {
    base::AutoLock lock(lock_);
    if (waiting_for_message_cb_.is_null() || !outgoing_message_queue_.empty()) {
      outgoing_message_queue_.push_back(message);
      return;
    } else {
      callback = std::move(waiting_for_message_cb_);
      DCHECK(waiting_for_message_cb_.is_null());
    }
  }

  std::move(callback).Run(message);
}

void HeadlessTabSocketImpl::SetListener(Listener* listener) {
  std::list<std::string> messages;

  {
    base::AutoLock lock(lock_);
    listener_ = listener;
    if (!listener)
      return;

    std::swap(messages, incoming_message_queue_);
  }

  for (const std::string& message : messages) {
    listener_->OnMessageFromTab(message);
  }
}

void HeadlessTabSocketImpl::SendMessageToEmbedder(const std::string& message) {
  Listener* listener = nullptr;
  {
    base::AutoLock lock(lock_);
    if (listener_) {
      listener = listener_;
    } else {
      incoming_message_queue_.push_back(message);
      return;
    }
  }

  listener->OnMessageFromTab(message);
}

void HeadlessTabSocketImpl::AwaitNextMessageFromEmbedder(
    AwaitNextMessageFromEmbedderCallback callback) {
  std::string message;
  {
    base::AutoLock lock(lock_);
    if (outgoing_message_queue_.empty()) {
      waiting_for_message_cb_ = std::move(callback);
      DCHECK(!waiting_for_message_cb_.is_null());
      return;
    } else {
      message = outgoing_message_queue_.front();
      outgoing_message_queue_.pop_front();
    }
  }
  std::move(callback).Run(message);
}

void HeadlessTabSocketImpl::CreateMojoService(
    mojo::InterfaceRequest<TabSocket> request) {
  mojo_bindings_.AddBinding(this, std::move(request));
}

}  // namespace headless
