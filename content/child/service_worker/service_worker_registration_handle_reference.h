// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_REFERENCE_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_REFERENCE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "url/gurl.h"

namespace content {

class ThreadSafeSender;
struct ServiceWorkerObjectInfo;

class ServiceWorkerRegistrationHandleReference {
 public:
  // Creates a new ServiceWorkerRegistrationHandleReference and increments
  // ref-count.
  static scoped_ptr<ServiceWorkerRegistrationHandleReference> Create(
      int registration_handle_id,
      const ServiceWorkerObjectInfo& info,
      ThreadSafeSender* sender);

  // Creates a new ServiceWorkerRegistrationHandleReference by adopting a
  // ref-count.
  static scoped_ptr<ServiceWorkerRegistrationHandleReference> Adopt(
      int registration_handle_id,
      const ServiceWorkerObjectInfo& info,
      ThreadSafeSender* sender);

  ~ServiceWorkerRegistrationHandleReference();

  int handle_id() const { return handle_id_; }
  GURL scope() const { return scope_; }

 private:
  ServiceWorkerRegistrationHandleReference(int registration_handle_id,
                                           const ServiceWorkerObjectInfo& info,
                                           ThreadSafeSender* sender,
                                           bool increment_ref_in_ctor);

  const int handle_id_;
  const GURL scope_;
  scoped_refptr<ThreadSafeSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegistrationHandleReference);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_REGISTRATION_HANDLE_REFERENCE_H_
