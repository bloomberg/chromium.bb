// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_WEB_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_LOADER_WEB_WORKER_FETCH_CONTEXT_IMPL_H_

#include <memory>
#include <string>

#include "base/synchronization/waitable_event.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/renderer_preferences.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/platform/web_application_cache_host.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "url/gurl.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

class ResourceDispatcher;
class ThreadSafeSender;
class URLLoaderThrottleProvider;
class WebSocketHandshakeThrottleProvider;

// This class is used for fetching resource requests from workers (dedicated
// worker and shared worker). This class is created on the main thread and
// passed to the worker thread. This class is not used for service workers. For
// service workers, ServiceWorkerFetchContextImpl class is used instead.
class CONTENT_EXPORT WebWorkerFetchContextImpl
    : public blink::WebWorkerFetchContext,
      public mojom::ServiceWorkerWorkerClient {
 public:
  // |service_worker_client_request| is bound to |this| to receive
  // OnControllerChanged() notifications.
  // |service_worker_worker_client_registry_info| is a host pointer to register
  // a new ServiceWorkerWorkerClient, which is needed when creating a nested
  // worker. |loader_factory_info| is used for regular loading by the worker.
  //
  // S13nServiceWorker:
  // If the worker is controlled by a service worker, this class makes another
  // loader factory which sends requests to the service worker, and passes
  // |fallback_factory_info| to that factory to use for network fallback.
  //
  // |loader_factory_info| and |fallback_factory_info| are different because
  // |loader_factory_info| can possibly include a default factory like AppCache,
  // while |fallback_factory_info| should not have such a default factory and
  // instead go directly to network for http(s) requests.
  // |fallback_factory_info| might not be simply the direct network factory,
  // because it might additionally support non-NetworkService schemes (e.g.,
  // chrome-extension://).
  WebWorkerFetchContextImpl(
      RendererPreferences renderer_preferences,
      mojom::ServiceWorkerWorkerClientRequest service_worker_client_request,
      mojom::ServiceWorkerWorkerClientRegistryPtrInfo
          service_worker_worker_client_registry_info,
      mojom::ServiceWorkerContainerHostPtrInfo
          service_worker_container_host_info,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> loader_factory_info,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          fallback_factory_info,
      std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
      std::unique_ptr<WebSocketHandshakeThrottleProvider>
          websocket_handshake_throttle_provider,
      ThreadSafeSender* thread_safe_sender,
      std::unique_ptr<service_manager::Connector> service_manager_connection);
  ~WebWorkerFetchContextImpl() override;

  // blink::WebWorkerFetchContext implementation:
  std::unique_ptr<blink::WebWorkerFetchContext> CloneForNestedWorker() override;
  void SetTerminateSyncLoadEvent(base::WaitableEvent*) override;
  void InitializeOnWorkerThread() override;
  std::unique_ptr<blink::WebURLLoaderFactory> CreateURLLoaderFactory() override;
  std::unique_ptr<blink::WebURLLoaderFactory> WrapURLLoaderFactory(
      mojo::ScopedMessagePipeHandle url_loader_factory_handle) override;
  void WillSendRequest(blink::WebURLRequest&) override;
  blink::mojom::ControllerServiceWorkerMode IsControlledByServiceWorker()
      const override;
  void SetIsOnSubframe(bool) override;
  bool IsOnSubframe() const override;
  blink::WebURL SiteForCookies() const override;
  void DidRunContentWithCertificateErrors() override;
  void DidDisplayContentWithCertificateErrors() override;
  void DidRunInsecureContent(const blink::WebSecurityOrigin&,
                             const blink::WebURL& insecure_url) override;
  void SetApplicationCacheHostID(int id) override;
  int ApplicationCacheHostID() const override;
  void SetSubresourceFilterBuilder(
      std::unique_ptr<blink::WebDocumentSubresourceFilter::Builder>) override;
  std::unique_ptr<blink::WebDocumentSubresourceFilter> TakeSubresourceFilter()
      override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle() override;

  // mojom::ServiceWorkerWorkerClient implementation:
  void OnControllerChanged(blink::mojom::ControllerServiceWorkerMode) override;

  // Sets the fetch context status copied from a frame. For dedicated workers,
  // it's copied from the ancestor frame (directly for non-nested workers, or
  // indirectly via its parent worker for nested workers). For shared workers,
  // it's copied from the shadow page.
  void set_service_worker_provider_id(int id);
  void set_is_controlled_by_service_worker(
      blink::mojom::ControllerServiceWorkerMode mode);
  void set_ancestor_frame_id(int id);
  void set_site_for_cookies(const blink::WebURL& site_for_cookies);
  // Sets whether the worker context is a secure context.
  // https://w3c.github.io/webappsec-secure-contexts/
  void set_is_secure_context(bool flag);
  void set_origin_url(const GURL& origin_url);
  void set_client_id(const std::string& client_id);

  using RewriteURLFunction = blink::WebURL (*)(const std::string&, bool);
  static void InstallRewriteURLFunction(RewriteURLFunction rewrite_url);

 private:
  class Factory;

  bool Send(IPC::Message* message);

  // S13nServiceWorker:
  // Resets the service worker url loader factory of a URLLoaderFactoryImpl
  // which was passed to Blink. The url loader factory is connected to the
  // controller service worker. Sets nullptr if the worker context is not
  // controlled by a service worker.
  void ResetServiceWorkerURLLoaderFactory();

  mojo::Binding<mojom::ServiceWorkerWorkerClient> binding_;
  mojom::ServiceWorkerWorkerClientRegistryPtr
      service_worker_worker_client_registry_;

  // Bound to |this| on the worker thread.
  mojom::ServiceWorkerWorkerClientRequest service_worker_client_request_;
  // Consumed on the worker thread to create
  // |service_worker_worker_client_registry_|.
  mojom::ServiceWorkerWorkerClientRegistryPtrInfo
      service_worker_worker_client_registry_info_;
  // Consumed on the worker thread to create |service_worker_container_host_|.
  mojom::ServiceWorkerContainerHostPtrInfo service_worker_container_host_info_;
  // Consumed on the worker thread to create |loader_factory_|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> loader_factory_info_;
  // Consumed on the worker thread to create |fallback_factory_|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> fallback_factory_info_;

  int service_worker_provider_id_ = kInvalidServiceWorkerProviderId;
  blink::mojom::ControllerServiceWorkerMode is_controlled_by_service_worker_ =
      blink::mojom::ControllerServiceWorkerMode::kNoController;

  // S13nServiceWorker:
  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  mojom::ServiceWorkerContainerHostPtr service_worker_container_host_;

  // S13nServiceWorker:
  // The Client#id value of the shared worker or dedicated worker (since
  // dedicated workers are not yet service worker clients, it is the parent
  // document's id in that case). Passed to ControllerServiceWorkerConnector.
  std::string client_id_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  // |loader_factory_| is used for regular loading by the worker. In
  // S13nServiceWorker, if the worker is controlled by a service worker, it
  // creates a ServiceWorkerSubresourceLoaderFactory instead.
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  // S13nServiceWorker: If the worker is controlled by a service worker, it
  // passes this factory to ServiceWorkerSubresourceLoaderFactory to use for
  // network fallback.
  scoped_refptr<network::SharedURLLoaderFactory> fallback_factory_;

  // S13nServiceWorker:
  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  scoped_refptr<base::RefCountedData<blink::mojom::BlobRegistryPtr>>
      blob_registry_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  std::unique_ptr<blink::WebDocumentSubresourceFilter::Builder>
      subresource_filter_builder_;
  // For dedicated workers, this is the ancestor frame (the parent frame for
  // non-nested workers, the closest ancestor for nested workers). For shared
  // workers, this is the shadow page.
  bool is_on_sub_frame_ = false;
  int ancestor_frame_id_ = MSG_ROUTING_NONE;
  GURL site_for_cookies_;
  bool is_secure_context_ = false;
  GURL origin_url_;
  int appcache_host_id_ = blink::WebApplicationCacheHost::kAppCacheNoHostId;

  // TODO(crbug.com/862854): Propagate preference changes from the browser
  // process.
  RendererPreferences renderer_preferences_;

  // This is owned by ThreadedMessagingProxyBase on the main thread.
  base::WaitableEvent* terminate_sync_load_event_ = nullptr;

  // The blink::WebURLLoaderFactory which was created and passed to
  // Blink by CreateURLLoaderFactory().
  base::WeakPtr<Factory> web_loader_factory_;

  std::unique_ptr<URLLoaderThrottleProvider> throttle_provider_;
  std::unique_ptr<WebSocketHandshakeThrottleProvider>
      websocket_handshake_throttle_provider_;

  std::unique_ptr<service_manager::Connector> service_manager_connection_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_WEB_WORKER_FETCH_CONTEXT_IMPL_H_
