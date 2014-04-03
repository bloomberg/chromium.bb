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

class MessagePortMessageFilter;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;

class CONTENT_EXPORT ServiceWorkerDispatcherHost : public BrowserMessageFilter {
 public:
  ServiceWorkerDispatcherHost(
      int render_process_id,
      MessagePortMessageFilter* message_port_message_filter);

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
  void OnAddScriptClient(int thread_id, int provider_id);
  void OnRemoveScriptClient(int thread_id, int provider_id);
  void OnSetHostedVersionId(int provider_id, int64 version_id);
  void OnWorkerStarted(int thread_id,
                       int embedded_worker_id);
  void OnWorkerStopped(int embedded_worker_id);
  void OnSendMessageToBrowser(int embedded_worker_id,
                              int request_id,
                              const IPC::Message& message);
  void OnPostMessage(int64 registration_id,
                     const base::string16& message,
                     const std::vector<int>& sent_message_port_ids);

  static void PostMessageFoundRegistration(
      const base::string16& message,
      const std::vector<int>& sent_message_port_ids,
      const std::vector<int>& new_routing_ids,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& result);

  // Callbacks from ServiceWorkerContextCore
  void RegistrationComplete(int32 thread_id,
                            int32 request_id,
                            ServiceWorkerStatusCode status,
                            int64 registration_id);

  void UnregistrationComplete(int32 thread_id,
                              int32 request_id,
                              ServiceWorkerStatusCode status);

  void SendRegistrationError(int32 thread_id,
                             int32 request_id,
                             ServiceWorkerStatusCode status);

  int render_process_id_;
  MessagePortMessageFilter* const message_port_message_filter_;

  base::WeakPtr<ServiceWorkerContextCore> context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
