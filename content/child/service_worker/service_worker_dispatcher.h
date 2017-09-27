// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
#define CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_

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
#include "content/public/child/worker_thread.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerError.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_state.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace IPC {
class Message;
}

struct ServiceWorkerMsg_MessageToDocument_Params;
struct ServiceWorkerMsg_SetControllerServiceWorker_Params;

namespace content {

class ServiceWorkerHandleReference;
class ServiceWorkerProviderContext;
class ServiceWorkerRegistrationHandleReference;
class ThreadSafeSender;
class WebServiceWorkerImpl;
class WebServiceWorkerRegistrationImpl;
struct ServiceWorkerObjectInfo;
struct ServiceWorkerVersionAttributes;

// This class manages communication with the browser process about
// registration of the service worker, exposed to renderer and worker
// scripts through methods like navigator.registerServiceWorker().
class CONTENT_EXPORT ServiceWorkerDispatcher : public WorkerThread::Observer {
 public:
  typedef blink::WebServiceWorkerRegistration::WebServiceWorkerUpdateCallbacks
      WebServiceWorkerUpdateCallbacks;
  typedef blink::WebServiceWorkerRegistration::
      WebServiceWorkerUnregistrationCallbacks
          WebServiceWorkerUnregistrationCallbacks;
  using WebEnableNavigationPreloadCallbacks =
      blink::WebServiceWorkerRegistration::WebEnableNavigationPreloadCallbacks;
  using WebGetNavigationPreloadStateCallbacks = blink::
      WebServiceWorkerRegistration::WebGetNavigationPreloadStateCallbacks;
  using WebSetNavigationPreloadHeaderCallbacks = blink::
      WebServiceWorkerRegistration::WebSetNavigationPreloadHeaderCallbacks;

  ServiceWorkerDispatcher(
      ThreadSafeSender* thread_safe_sender,
      base::SingleThreadTaskRunner* main_thread_task_runner);
  ~ServiceWorkerDispatcher() override;

  void OnMessageReceived(const IPC::Message& msg);

  // Corresponds to ServiceWorkerRegistration.update().
  void UpdateServiceWorker(
      int provider_id,
      int64_t registration_id,
      std::unique_ptr<WebServiceWorkerUpdateCallbacks> callbacks);
  // Corresponds to ServiceWorkerRegistration.unregister().
  void UnregisterServiceWorker(
      int provider_id,
      int64_t registration_id,
      std::unique_ptr<WebServiceWorkerUnregistrationCallbacks> callbacks);

  // Corresponds to NavigationPreloadManager.enable/disable.
  void EnableNavigationPreload(
      int provider_id,
      int64_t registration_id,
      bool enable,
      std::unique_ptr<WebEnableNavigationPreloadCallbacks> callbacks);
  // Corresponds to NavigationPreloadManager.getState.
  void GetNavigationPreloadState(
      int provider_id,
      int64_t registration_id,
      std::unique_ptr<WebGetNavigationPreloadStateCallbacks> callbacks);
  // Corresponds to NavigationPreloadManager.setHeaderValue.
  void SetNavigationPreloadHeader(
      int provider_id,
      int64_t registration_id,
      const std::string& value,
      std::unique_ptr<WebSetNavigationPreloadHeaderCallbacks> callbacks);

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

  // Returns the existing service worker or a newly created one with the given
  // handle reference. Returns nullptr if the given reference is invalid.
  scoped_refptr<WebServiceWorkerImpl> GetOrCreateServiceWorker(
      std::unique_ptr<ServiceWorkerHandleReference> handle_ref);

  // Returns the existing registration or a newly created one. When a new one is
  // created, increments interprocess references to the registration and its
  // versions via ServiceWorker(Registration)HandleReference.
  scoped_refptr<WebServiceWorkerRegistrationImpl> GetOrCreateRegistration(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
      const ServiceWorkerVersionAttributes& attrs);

  // Returns the existing registration or a newly created one. Always adopts
  // interprocess references to the registration and its versions via
  // ServiceWorker(Registration)HandleReference.
  scoped_refptr<WebServiceWorkerRegistrationImpl> GetOrAdoptRegistration(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info,
      const ServiceWorkerVersionAttributes& attrs);

  static ServiceWorkerDispatcher* GetOrCreateThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender,
      base::SingleThreadTaskRunner* main_thread_task_runner);

  // Unlike GetOrCreateThreadSpecificInstance() this doesn't create a new
  // instance if thread-local instance doesn't exist.
  static ServiceWorkerDispatcher* GetThreadSpecificInstance();

  base::SingleThreadTaskRunner* main_thread_task_runner() {
    return main_thread_task_runner_.get();
  }

 private:
  using UpdateCallbackMap =
      base::IDMap<std::unique_ptr<WebServiceWorkerUpdateCallbacks>>;
  using UnregistrationCallbackMap =
      base::IDMap<std::unique_ptr<WebServiceWorkerUnregistrationCallbacks>>;
  using EnableNavigationPreloadCallbackMap =
      base::IDMap<std::unique_ptr<WebEnableNavigationPreloadCallbacks>>;
  using GetNavigationPreloadStateCallbackMap =
      base::IDMap<std::unique_ptr<WebGetNavigationPreloadStateCallbacks>>;
  using SetNavigationPreloadHeaderCallbackMap =
      base::IDMap<std::unique_ptr<WebSetNavigationPreloadHeaderCallbacks>>;

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

  void OnUpdated(int thread_id, int request_id);
  void OnUnregistered(int thread_id,
                      int request_id,
                      bool is_success);
  void OnDidEnableNavigationPreload(int thread_id, int request_id);
  void OnDidGetNavigationPreloadState(int thread_id,
                                      int request_id,
                                      const NavigationPreloadState& state);
  void OnDidSetNavigationPreloadHeader(int thread_id, int request_id);
  void OnUpdateError(int thread_id,
                     int request_id,
                     blink::mojom::ServiceWorkerErrorType error_type,
                     const base::string16& message);
  void OnUnregistrationError(int thread_id,
                             int request_id,
                             blink::mojom::ServiceWorkerErrorType error_type,
                             const base::string16& message);
  void OnEnableNavigationPreloadError(
      int thread_id,
      int request_id,
      blink::mojom::ServiceWorkerErrorType error_type,
      const std::string& message);
  void OnGetNavigationPreloadStateError(
      int thread_id,
      int request_id,
      blink::mojom::ServiceWorkerErrorType error_type,
      const std::string& message);
  void OnSetNavigationPreloadHeaderError(
      int thread_id,
      int request_id,
      blink::mojom::ServiceWorkerErrorType error_type,
      const std::string& message);
  void OnServiceWorkerStateChanged(int thread_id,
                                   int handle_id,
                                   blink::mojom::ServiceWorkerState state);
  void OnSetVersionAttributes(int thread_id,
                              int registration_handle_id,
                              int changed_mask,
                              const ServiceWorkerVersionAttributes& attributes);
  void OnUpdateFound(int thread_id,
                     int registration_handle_id);
  void OnSetControllerServiceWorker(
      const ServiceWorkerMsg_SetControllerServiceWorker_Params& params);
  void OnPostMessage(const ServiceWorkerMsg_MessageToDocument_Params& params);
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

  // Assumes that the given object information retains an interprocess handle
  // reference passed from the browser process, and adopts it.
  std::unique_ptr<ServiceWorkerRegistrationHandleReference> Adopt(
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr info);
  std::unique_ptr<ServiceWorkerHandleReference> Adopt(
      const ServiceWorkerObjectInfo& info);

  UpdateCallbackMap pending_update_callbacks_;
  UnregistrationCallbackMap pending_unregistration_callbacks_;
  EnableNavigationPreloadCallbackMap enable_navigation_preload_callbacks_;
  GetNavigationPreloadStateCallbackMap get_navigation_preload_state_callbacks_;
  SetNavigationPreloadHeaderCallbackMap
      set_navigation_preload_header_callbacks_;

  ProviderClientMap provider_clients_;
  ProviderContextMap provider_contexts_;

  WorkerObjectMap service_workers_;
  RegistrationObjectMap registrations_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_H_
