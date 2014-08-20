// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_REFERENCE_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_REFERENCE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/service_worker/service_worker_types.h"
#include "url/gurl.h"

namespace content {

class ThreadSafeSender;

class ServiceWorkerRegistrationHandleReference {
 public:
  // Creates a new ServiceWorkerRegistrationHandleReference and increments
  // ref-count.
  static scoped_ptr<ServiceWorkerRegistrationHandleReference> Create(
      const ServiceWorkerRegistrationObjectInfo& info,
      ThreadSafeSender* sender);

  // Creates a new ServiceWorkerRegistrationHandleReference by adopting a
  // ref-count.
  static scoped_ptr<ServiceWorkerRegistrationHandleReference> Adopt(
      const ServiceWorkerRegistrationObjectInfo& info,
      ThreadSafeSender* sender);

  ~ServiceWorkerRegistrationHandleReference();

  int handle_id() const { return info_.handle_id; }
  GURL scope() const { return info_.scope; }

 private:
  ServiceWorkerRegistrationHandleReference(
      const ServiceWorkerRegistrationObjectInfo& info,
      ThreadSafeSender* sender,
      bool increment_ref_in_ctor);

  ServiceWorkerRegistrationObjectInfo info_;
  scoped_refptr<ThreadSafeSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationHandleReference);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_REFERENCE_H_
