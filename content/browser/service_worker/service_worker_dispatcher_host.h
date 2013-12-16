// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/public/browser/browser_message_filter.h"

class GURL;

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class ServiceWorkerProviderHost;

class CONTENT_EXPORT ServiceWorkerDispatcherHost : public BrowserMessageFilter {
 public:
  explicit ServiceWorkerDispatcherHost(int render_process_id);

  void Init(ServiceWorkerContextWrapper* context_wrapper);

  // BrowserIOMessageFilter implementation
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 protected:
  virtual ~ServiceWorkerDispatcherHost();

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<ServiceWorkerDispatcherHost>;
  friend class TestingServiceWorkerDispatcherHost;

  // IPC Message handlers
  void OnRegisterServiceWorker(int32 thread_id,
                               int32 request_id,
                               const GURL& pattern,
                               const GURL& script_url);
  void OnUnregisterServiceWorker(int32 thread_id,
                                 int32 request_id,
                                 const GURL& pattern);
  void OnProviderCreated(int provider_id);
  void OnProviderDestroyed(int provider_id);

  // Callbacks from ServiceWorkerContextCore
  void RegistrationComplete(int32 thread_id,
                            int32 request_id,
                            ServiceWorkerRegistrationStatus status,
                            int64 registration_id);

  void UnregistrationComplete(int32 thread_id,
                              int32 request_id,
                              ServiceWorkerRegistrationStatus status);

  void SendRegistrationError(int32 thread_id,
                             int32 request_id,
                             ServiceWorkerRegistrationStatus status);
  int render_process_id_;
  base::WeakPtr<ServiceWorkerContextCore> context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
