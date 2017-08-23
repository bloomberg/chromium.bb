// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/service_worker_event_dispatcher_holder.h"

namespace content {

ServiceWorkerEventDispatcherHolder::ServiceWorkerEventDispatcherHolder(
    mojom::ServiceWorkerEventDispatcherPtrInfo ptr_info) {
  event_dispatcher_.Bind(std::move(ptr_info));
}

ServiceWorkerEventDispatcherHolder::ServiceWorkerEventDispatcherHolder(
    mojom::ServiceWorkerEventDispatcher* raw_ptr)
    : event_dispatcher_raw_ptr_for_testing_(raw_ptr) {}

ServiceWorkerEventDispatcherHolder::~ServiceWorkerEventDispatcherHolder() =
    default;

}  // namespace content
