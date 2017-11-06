// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/renderer/worker_thread.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_object.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_state.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {
class Message;
}

namespace content {

class ServiceWorkerHandleReference;
class ServiceWorkerProviderContext;
class ThreadSafeSender;
class WebServiceWorkerImpl;
class WebServiceWorkerRegistrationImpl;

// This class manages communication with the browser process about
// registration of the service worker, exposed to renderer and worker
// scripts through methods like navigator.registerServiceWorker().
class CONTENT_EXPORT ServiceWorkerDispatcher : public WorkerThread::Observer {
 public:
  ServiceWorkerDispatcher(
      ThreadSafeSender* thread_safe_sender,
      base::SingleThreadTaskRunner* main_thread_task_runner);
  ~ServiceWorkerDispatcher() override;

  void OnMessageReceived(const IPC::Message& msg);

  // Called when a new provider context for a document is created. Usually
  // this happens when a new document is being loaded, and is called much
  // earlier than AddScriptClient.
  // (This is attached only to the document thread's ServiceWorkerDispatcher)
  void AddProviderContext(ServiceWorkerProviderContext* provider_context);
  void RemoveProviderContext(ServiceWorkerProviderContext* provider_context);

  // Called when navigator.serviceWorker is instantiated or detached
  // for a document whose provider can be identified by |provider_id|.
  void AddProviderClient(int provider_id,
                         blink::WebServiceWorkerProviderClient* client);
  void RemoveProviderClient(int provider_id);

  blink::WebServiceWorkerProviderClient* GetProviderClient(int provider_id);

  // Returns the existing service worker or a newly created one with the given
  // handle reference. Returns nullptr if the given reference is invalid.
  scoped_refptr<WebServiceWorkerImpl> GetOrCreateServiceWorker(
      std::unique_ptr<ServiceWorkerHandleReference> handle_ref);

  // Returns the existing registration or a newly created one for a service
  // worker execution context. When a new one is created, increments
  // interprocess references to its versions via ServiceWorkerHandleReference.
  scoped_refptr<WebServiceWorkerRegistrationImpl>
  GetOrCreateRegistrationForServiceWorkerGlobalScope(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);

  // Returns the existing registration or a newly created one for service worker
  // client contexts (document, shared worker). Always adopts
  // interprocess references to its versions via
  // ServiceWorkerHandleReference.
  scoped_refptr<WebServiceWorkerRegistrationImpl>
  GetOrCreateRegistrationForServiceWorkerClient(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info);

  static ServiceWorkerDispatcher* GetOrCreateThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender,
      base::SingleThreadTaskRunner* main_thread_task_runner);

  // Unlike GetOrCreateThreadSpecificInstance() this doesn't create a new
  // instance if thread-local instance doesn't exist.
  static ServiceWorkerDispatcher* GetThreadSpecificInstance();

  // Assumes that the given object information retains an interprocess handle
  // reference passed from the browser process, and adopts it.
  std::unique_ptr<ServiceWorkerHandleReference> Adopt(
      blink::mojom::ServiceWorkerObjectInfoPtr info);

  base::SingleThreadTaskRunner* main_thread_task_runner() {
    return main_thread_task_runner_.get();
  }

 private:
  using ProviderClientMap =
      std::map<int, blink::WebServiceWorkerProviderClient*>;
  using ProviderContextMap = std::map<int, ServiceWorkerProviderContext*>;
  using WorkerToProviderMap = std::map<int, ServiceWorkerProviderContext*>;
  using WorkerObjectMap = std::map<int, WebServiceWorkerImpl*>;
  using RegistrationObjectMap =
      std::map<int, WebServiceWorkerRegistrationImpl*>;

  friend class ServiceWorkerDispatcherTest;
  friend class WebServiceWorkerImpl;
  friend class WebServiceWorkerRegistrationImpl;

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  void OnServiceWorkerStateChanged(int thread_id,
                                   int handle_id,
                                   blink::mojom::ServiceWorkerState state);
  void OnCountFeature(int thread_id, int provider_id, uint32_t feature);

  // Keeps map from handle_id to ServiceWorker object.
  void AddServiceWorker(int handle_id, WebServiceWorkerImpl* worker);
  void RemoveServiceWorker(int handle_id);

  // Keeps map from registration_handle_id to ServiceWorkerRegistration object.
  void AddServiceWorkerRegistration(
      int registration_handle_id,
      WebServiceWorkerRegistrationImpl* registration);
  void RemoveServiceWorkerRegistration(
      int registration_handle_id);

  ProviderClientMap provider_clients_;
  ProviderContextMap provider_contexts_;

  WorkerObjectMap service_workers_;
  RegistrationObjectMap registrations_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
