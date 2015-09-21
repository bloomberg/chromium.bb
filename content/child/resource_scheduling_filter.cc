// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/resource_scheduling_filter.h"

#include "base/bind.h"
#include "base/location.h"
#include "content/child/resource_dispatcher.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_start.h"

namespace content {

ResourceSchedulingFilter::ResourceSchedulingFilter(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread_task_runner,
    ResourceDispatcher* resource_dispatcher)
    : main_thread_task_runner_(main_thread_task_runner),
      resource_dispatcher_(resource_dispatcher),
      weak_ptr_factory_(this) {
  DCHECK(main_thread_task_runner_.get());
  DCHECK(resource_dispatcher_);
}

ResourceSchedulingFilter::~ResourceSchedulingFilter() {
}

bool ResourceSchedulingFilter::OnMessageReceived(const IPC::Message& message) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ResourceSchedulingFilter::DispatchMessage,
                            weak_ptr_factory_.GetWeakPtr(), message));
  return true;
}

bool ResourceSchedulingFilter::GetSupportedMessageClasses(
    std::vector<uint32>* supported_message_classes) const {
  supported_message_classes->push_back(ResourceMsgStart);
  return true;
}

void ResourceSchedulingFilter::DispatchMessage(const IPC::Message& message) {
  resource_dispatcher_->OnMessageReceived(message);
}

}  // namespace content
