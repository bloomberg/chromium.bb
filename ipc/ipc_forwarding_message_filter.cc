// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_forwarding_message_filter.h"

#include "base/bind.h"
#include "base/location.h"

namespace IPC {

ForwardingMessageFilter::ForwardingMessageFilter(
    const uint32* message_ids_to_filter,
    size_t num_message_ids_to_filter,
    base::TaskRunner* target_task_runner,
    const Handler& handler)
    : target_task_runner_(target_task_runner),
      handler_(handler) {
  DCHECK(target_task_runner_);
  DCHECK(!handler_.is_null());
  for (size_t i = 0; i < num_message_ids_to_filter; i++)
    message_ids_to_filter_.insert(message_ids_to_filter[i]);
}

void ForwardingMessageFilter::AddRoute(int routing_id) {
  base::AutoLock locked(routes_lock_);
  routes_.insert(routing_id);
}

void ForwardingMessageFilter::RemoveRoute(int routing_id) {
  base::AutoLock locked(routes_lock_);
  routes_.erase(routing_id);
}

bool ForwardingMessageFilter::OnMessageReceived(const Message& message) {
  if (message_ids_to_filter_.find(message.type()) ==
      message_ids_to_filter_.end())
    return false;

  {
    base::AutoLock locked(routes_lock_);
    if (routes_.find(message.routing_id()) == routes_.end())
      return false;
  }

  target_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ForwardingMessageFilter::ForwardToHandler, this, message));
  return true;
}

ForwardingMessageFilter::~ForwardingMessageFilter() {
}

void ForwardingMessageFilter::ForwardToHandler(const Message& message) {
  DCHECK(target_task_runner_->RunsTasksOnCurrentThread());
  handler_.Run(message);
}

}  // namespace IPC
