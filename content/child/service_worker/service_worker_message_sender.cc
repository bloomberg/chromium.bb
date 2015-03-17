// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_message_sender.h"

#include "content/child/thread_safe_sender.h"

namespace content {

ServiceWorkerMessageSender::ServiceWorkerMessageSender(
    ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender) {
}

bool ServiceWorkerMessageSender::Send(IPC::Message* message) {
  return thread_safe_sender_->Send(message);
}

ServiceWorkerMessageSender::~ServiceWorkerMessageSender() {
}

}  // namespace content
