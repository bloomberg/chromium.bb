// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_EVENT_DISPATCHER_HOLDER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_EVENT_DISPATCHER_HOLDER_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"

namespace content {

// Allows mojo::ServiceWorkerEventDispatcherPtr to be shared by
// multiple owners.
class CONTENT_EXPORT ServiceWorkerEventDispatcherHolder
    : public base::RefCounted<ServiceWorkerEventDispatcherHolder> {
 public:
  explicit ServiceWorkerEventDispatcherHolder(
      mojom::ServiceWorkerEventDispatcherPtrInfo ptr_info);

  mojom::ServiceWorkerEventDispatcher* get() {
    if (event_dispatcher_raw_ptr_for_testing_)
      return event_dispatcher_raw_ptr_for_testing_;
    return event_dispatcher_.get();
  }

 private:
  friend class ServiceWorkerSubresourceLoaderTest;
  friend class base::RefCounted<ServiceWorkerEventDispatcherHolder>;
  ~ServiceWorkerEventDispatcherHolder();

  static scoped_refptr<ServiceWorkerEventDispatcherHolder> CreateForTesting(
      mojom::ServiceWorkerEventDispatcher* raw_ptr) {
    return make_scoped_refptr(new ServiceWorkerEventDispatcherHolder(raw_ptr));
  }
  explicit ServiceWorkerEventDispatcherHolder(
      mojom::ServiceWorkerEventDispatcher* raw_ptr);

  mojom::ServiceWorkerEventDispatcher* event_dispatcher_raw_ptr_for_testing_;
  mojom::ServiceWorkerEventDispatcherPtr event_dispatcher_;
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_EVENT_DISPATCHER_HOLDER_H_
