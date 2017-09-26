// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_message_filter.h"

#include <stdint.h>

#include "base/macros.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/common/devtools_messages.h"
#include "content/common/view_messages.h"

namespace content {

SharedWorkerMessageFilter::SharedWorkerMessageFilter(
    int render_process_id,
    const NextRoutingIDCallback& next_routing_id_callback)
    : BrowserMessageFilter(0 /* none */),
      render_process_id_(render_process_id),
      next_routing_id_callback_(next_routing_id_callback) {}

int SharedWorkerMessageFilter::GetNextRoutingID() {
  return next_routing_id_callback_.Run();
}

SharedWorkerMessageFilter::~SharedWorkerMessageFilter() {}

void SharedWorkerMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  SharedWorkerServiceImpl::GetInstance()->AddFilter(this);
}

void SharedWorkerMessageFilter::OnFilterRemoved() {
  SharedWorkerServiceImpl::GetInstance()->RemoveFilter(this);
}

void SharedWorkerMessageFilter::OnChannelClosing() {
  SharedWorkerServiceImpl::GetInstance()->OnProcessClosing(render_process_id_);
}

bool SharedWorkerMessageFilter::OnMessageReceived(const IPC::Message& message) {
  return false;
}

}  // namespace content
