// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/serial_extension_host_queue.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "extensions/browser/deferred_start_render_host.h"

namespace extensions {

SerialExtensionHostQueue::SerialExtensionHostQueue()
    : pending_create_(false), ptr_factory_(this) {
}

SerialExtensionHostQueue::~SerialExtensionHostQueue() {
}

void SerialExtensionHostQueue::Add(DeferredStartRenderHost* host) {
  queue_.push_back(host);
  PostTask();
}

void SerialExtensionHostQueue::Remove(DeferredStartRenderHost* host) {
  std::list<DeferredStartRenderHost*>::iterator it =
      std::find(queue_.begin(), queue_.end(), host);
  if (it != queue_.end())
    queue_.erase(it);
}

void SerialExtensionHostQueue::PostTask() {
  if (!pending_create_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&SerialExtensionHostQueue::ProcessOneHost,
                              ptr_factory_.GetWeakPtr()));
    pending_create_ = true;
  }
}

void SerialExtensionHostQueue::ProcessOneHost() {
  pending_create_ = false;
  if (queue_.empty())
    return;  // can happen on shutdown

  queue_.front()->CreateRenderViewNow();
  queue_.pop_front();

  if (!queue_.empty())
    PostTask();
}

}  // namespace extensions
