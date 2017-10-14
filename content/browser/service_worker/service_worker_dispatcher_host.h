// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/common/service_worker/service_worker.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "mojo/public/cpp/bindings/strong_associated_binding_set.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace blink {
class MessagePortChannel;
}

namespace url {
class Origin;
}  // namespace url

namespace content {

class ResourceContext;
class ServiceWorkerContextCore;
class ServiceWorkerContextWrapper;
class ServiceWorkerHandle;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerRegistrationHandle;
class ServiceWorkerVersion;
struct ServiceWorkerObjectInfo;
struct ServiceWorkerVersionAttributes;

// ServiceWorkerDispatcherHost is the browser-side endpoint for several IPC
// messages for service workers. There is a 1:1 correspondence between
// renderer processes and ServiceWorkerDispatcherHosts. Currently
// ServiceWorkerDispatcherHost handles both legacy IPC messages (to and from
// its corresponding ServiceWorkerDispatcher on the renderer) and Mojo IPC
// messages (from any ServiceWorkerNetworkProvider on the renderer).
//
// Most messages are "from" a "service worker provider" on the renderer (a
// ServiceWorkerNetworkProvider or a blink::WebServiceWorkerProvider), hence
// they include a provider_id which must match to a ServiceWorkerProviderHost.
//
// ServiceWorkerDispatcherHost is created on the UI thread in
// RenderProcessHostImpl::Init() via CreateMessageFilters(). But initialization
// and destruction occur on the IO thread, as does most (or all?) message
// handling.  It lives as long as the renderer process lives. Therefore much
// tracking of renderer processes in browser-side service worker code is built
// on ServiceWorkerDispatcherHost lifetime.
//
// This class is bound with mojom::ServiceWorkerDispatcherHost. All
// InterfacePtrs on the same render process are bound to the same
// content::ServiceWorkerDispatcherHost. This can be overridden only for
// testing.
class CONTENT_EXPORT ServiceWorkerDispatcherHost
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<mojom::ServiceWorkerDispatcherHost>,
      public mojom::ServiceWorkerDispatcherHost {
 public:
  ServiceWorkerDispatcherHost(
      int render_process_id,
      ResourceContext* resource_context);

  void Init(ServiceWorkerContextWrapper* context_wrapper);

  // BrowserMessageFilter implementation
  void OnFilterAdded(IPC::Channel* channel) override;
  void OnFilterRemoved() override;
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation

  // Send() queues the message until the underlying sender is ready.  This
  // class assumes that Send() can only fail after that when the renderer
  // process has terminated, at which point the whole instance will eventually
  // be destroyed.
  bool Send(IPC::Message* message) override;

  // Following methods are virtual only for testing.
  virtual void RegisterServiceWorkerHandle(
      std::unique_ptr<ServiceWorkerHandle> handle);
  virtual void RegisterServiceWorkerRegistrationHandle(
      ServiceWorkerRegistrationHandle* handle);
  virtual void UnregisterServiceWorkerRegistrationHandle(int handle_id);

  ServiceWorkerHandle* FindServiceWorkerHandle(int provider_id,
                                               int64_t version_id);

  // Gets or creates the registration and version handles appropriate for
  // representing |registration| inside of |provider_host|. Sets |out_info| and
  // |out_attrs| accordingly for these handles.
  void GetRegistrationObjectInfoAndVersionAttributes(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerRegistration* registration,
      blink::mojom::ServiceWorkerRegistrationObjectInfoPtr* out_info,
      ServiceWorkerVersionAttributes* out_attrs);

  // Returns an object info representing |registration|. The object info holds a
  // Mojo connection to the ServiceWorkerRegistrationHandle for the
  // |registration| to ensure the handle stays alive while the object info is
  // alive. A new handle is created if one does not already exist.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
  CreateRegistrationObjectInfo(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerRegistration* registration);

  ResourceContext* resource_context() { return resource_context_; }

  base::WeakPtr<ServiceWorkerDispatcherHost> AsWeakPtr();

 protected:
  ~ServiceWorkerDispatcherHost() override;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<ServiceWorkerDispatcherHost>;
  friend class ServiceWorkerDispatcherHostTest;
  friend class TestingServiceWorkerDispatcherHost;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDispatcherHostTest,
                           ProviderCreatedAndDestroyed);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerDispatcherHostTest,
                           CleanupOnRendererCrash);
  FRIEND_TEST_ALL_PREFIXES(BackgroundSyncManagerTest,
                           RegisterWithoutLiveSWRegistration);

  using StatusCallback = base::Callback<void(ServiceWorkerStatusCode status)>;
  enum class ProviderStatus { OK, NO_CONTEXT, DEAD_HOST, NO_HOST, NO_URL };
  // Debugging for https://crbug.com/750267
  enum class Phase { kInitial, kAddedToContext, kRemovedFromContext };

  // mojom::ServiceWorkerDispatcherHost implementation
  void OnProviderCreated(ServiceWorkerProviderHostInfo info) override;

  // IPC Message handlers
  void OnUnregisterServiceWorker(int thread_id,
                                 int request_id,
                                 int provider_id,
                                 int64_t registration_id);
  void OnEnableNavigationPreload(int thread_id,
                                 int request_id,
                                 int provider_id,
                                 int64_t registration_id,
                                 bool enable);
  void OnGetNavigationPreloadState(int thread_id,
                                   int request_id,
                                   int provider_id,
                                   int64_t registration_id);
  void OnSetNavigationPreloadHeader(int thread_id,
                                    int request_id,
                                    int provider_id,
                                    int64_t registration_id,
                                    const std::string& value);
  void OnCountFeature(int64_t version_id, uint32_t feature);
  void OnIncrementServiceWorkerRefCount(int handle_id);
  void OnDecrementServiceWorkerRefCount(int handle_id);
  void OnPostMessageToWorker(
      int handle_id,
      int provider_id,
      const base::string16& message,
      const url::Origin& source_origin,
      const std::vector<blink::MessagePortChannel>& sent_message_ports);

  void OnTerminateWorker(int handle_id);

  void DispatchExtendableMessageEvent(
      scoped_refptr<ServiceWorkerVersion> worker,
      const base::string16& message,
      const url::Origin& source_origin,
      const std::vector<blink::MessagePortChannel>& sent_message_ports,
      ServiceWorkerProviderHost* sender_provider_host,
      const StatusCallback& callback);
  template <typename SourceInfo>
  void DispatchExtendableMessageEventInternal(
      scoped_refptr<ServiceWorkerVersion> worker,
      const base::string16& message,
      const url::Origin& source_origin,
      const std::vector<blink::MessagePortChannel>& sent_message_ports,
      const base::Optional<base::TimeDelta>& timeout,
      const StatusCallback& callback,
      const SourceInfo& source_info);
  void DispatchExtendableMessageEventAfterStartWorker(
      scoped_refptr<ServiceWorkerVersion> worker,
      const base::string16& message,
      const url::Origin& source_origin,
      const std::vector<blink::MessagePortChannel>& sent_message_ports,
      const ExtendableMessageEventSource& source,
      const base::Optional<base::TimeDelta>& timeout,
      const StatusCallback& callback);
  template <typename SourceInfo>
  void DidFailToDispatchExtendableMessageEvent(
      const std::vector<blink::MessagePortChannel>& sent_message_ports,
      const SourceInfo& source_info,
      const StatusCallback& callback,
      ServiceWorkerStatusCode status);
  void ReleaseSourceInfo(const ServiceWorkerClientInfo& source_info);
  void ReleaseSourceInfo(const ServiceWorkerObjectInfo& source_info);

  ServiceWorkerRegistrationHandle* FindRegistrationHandle(
      int provider_id,
      int64_t registration_id);

  void UnregistrationComplete(int thread_id,
                              int request_id,
                              ServiceWorkerStatusCode status);

  ServiceWorkerContextCore* GetContext();
  // Returns the provider host with id equal to |provider_id|, or nullptr
  // if the provider host could not be found or is not appropriate for
  // initiating a request such as register/unregister/update.
  ServiceWorkerProviderHost* GetProviderHostForRequest(
      ProviderStatus* out_status,
      int provider_id);

  void DidUpdateNavigationPreloadEnabled(int thread_id,
                                         int request_id,
                                         int registration_id,
                                         bool enable,
                                         ServiceWorkerStatusCode status);
  void DidUpdateNavigationPreloadHeader(int thread_id,
                                        int request_id,
                                        int registration_id,
                                        const std::string& value,
                                        ServiceWorkerStatusCode status);

  const int render_process_id_;
  ResourceContext* resource_context_;
  // Only accessed on the IO thread.
  Phase phase_ = Phase::kInitial;
  // Only accessed on the IO thread.
  scoped_refptr<ServiceWorkerContextWrapper> context_wrapper_;

  base::IDMap<std::unique_ptr<ServiceWorkerHandle>> handles_;

  using RegistrationHandleMap =
      base::IDMap<std::unique_ptr<ServiceWorkerRegistrationHandle>>;
  RegistrationHandleMap registration_handles_;

  bool channel_ready_;  // True after BrowserMessageFilter::sender_ != NULL.
  std::vector<std::unique_ptr<IPC::Message>> pending_messages_;

  base::WeakPtrFactory<ServiceWorkerDispatcherHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_DISPATCHER_HOST_H_
