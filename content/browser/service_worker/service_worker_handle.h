// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_version.h"

namespace IPC {
class Sender;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerRegistration;

// Roughly corresponds to one ServiceWorker object in the renderer process
// (WebServiceWorkerImpl).
// Has references to the corresponding ServiceWorkerVersion and
// ServiceWorkerRegistration (therefore they're guaranteed to be alive while
// this handle is around).
class ServiceWorkerHandle : public ServiceWorkerVersion::Listener {
 public:
  // Creates a handle for a live version. The version's corresponding
  // registration must be also alive.
  // This may return NULL if |context|.get() or |version| is NULL.
  // |sender| and |thread_id| will be used to send messages to the
  // corresponding WebServiceWorkerImpl (which should live on |thread_id|
  // in the child process).
  static scoped_ptr<ServiceWorkerHandle> Create(
      base::WeakPtr<ServiceWorkerContextCore> context,
      IPC::Sender* sender,
      int thread_id,
      ServiceWorkerVersion* version);

  ServiceWorkerHandle(base::WeakPtr<ServiceWorkerContextCore> context,
                      IPC::Sender* sender,
                      int thread_id,
                      ServiceWorkerRegistration* registration,
                      ServiceWorkerVersion* version);
  virtual ~ServiceWorkerHandle();

  // ServiceWorkerVersion::Listener overrides.
  virtual void OnVersionStateChanged(ServiceWorkerVersion* version) OVERRIDE;

  ServiceWorkerRegistration* registration() { return registration_.get(); }
  ServiceWorkerVersion* version() { return version_.get(); }

 private:
  base::WeakPtr<ServiceWorkerContextCore> context_;
  IPC::Sender* sender_;  // Not owned, it should always outlive this.
  const int thread_id_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandle);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HANDLE_H_
