// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/shared_worker_host.h"

#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_message_filter.h"
#include "content/public/browser/browser_thread.h"

namespace content {

SharedWorkerHost::SharedWorkerHost(SharedWorkerInstance* instance)
    : instance_(instance),
      worker_route_id_(MSG_ROUTING_NONE) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

SharedWorkerHost::~SharedWorkerHost() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void SharedWorkerHost::Init(SharedWorkerMessageFilter* filter) {
  DCHECK(worker_route_id_ == MSG_ROUTING_NONE);
  worker_route_id_ = filter->GetNextRoutingID();
  // TODO(horo): implement this.
  NOTIMPLEMENTED();
}

}  // namespace content
