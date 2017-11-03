// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_WORKER_FETCH_CONTEXT_IMPL_H_

#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/service_worker_modes.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "content/public/renderer/child_url_loader_factory_getter.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/WebKit/common/blob/blob_registry.mojom.h"
#include "third_party/WebKit/public/platform/WebApplicationCacheHost.h"
#include "third_party/WebKit/public/platform/WebWorkerFetchContext.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_object.mojom.h"
#include "url/gurl.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

class ResourceDispatcher;
class ThreadSafeSender;

// This class is used while fetching resource requests on workers (dedicated
// worker and shared worker) when off-main-thread-fetch is enabled. This class
// is created on the main thread and passed to the worker thread.
// This class is not used for service workers. For service workers,
// ServiceWorkerFetchContextImpl class is used instead.
class WorkerFetchContextImpl : public blink::WebWorkerFetchContext,
                               public mojom::ServiceWorkerWorkerClient {
 public:
  WorkerFetchContextImpl(
      mojom::ServiceWorkerWorkerClientRequest service_worker_client_request,
      mojom::ServiceWorkerContainerHostPtrInfo
          service_worker_container_host_info,
      ChildURLLoaderFactoryGetter::Info url_loader_factory_getter_info);
  ~WorkerFetchContextImpl() override;

  // blink::WebWorkerFetchContext implementation:
  void InitializeOnWorkerThread(
      scoped_refptr<base::SingleThreadTaskRunner>) override;
  std::unique_ptr<blink::WebURLLoaderFactory> CreateURLLoaderFactory() override;
  void WillSendRequest(blink::WebURLRequest&) override;
  bool IsControlledByServiceWorker() const override;
  void SetIsOnSubframe(bool) override;
  bool IsOnSubframe() const override;
  blink::WebURL SiteForCookies() const override;
  void DidRunContentWithCertificateErrors(const blink::WebURL& url) override;
  void DidDisplayContentWithCertificateErrors(
      const blink::WebURL& url) override;
  void DidRunInsecureContent(const blink::WebSecurityOrigin&,
                             const blink::WebURL& insecure_url) override;
  void SetApplicationCacheHostID(int id) override;
  int ApplicationCacheHostID() const override;
  void SetSubresourceFilterBuilder(
      std::unique_ptr<blink::WebDocumentSubresourceFilter::Builder>) override;
  std::unique_ptr<blink::WebDocumentSubresourceFilter> TakeSubresourceFilter()
      override;

  // mojom::ServiceWorkerWorkerClient implementation:
  void SetControllerServiceWorker(int64_t controller_version_id) override;

  // Sets the fetch context status copied from the frame; the parent frame for a
  // dedicated worker, the main frame of the shadow page for a shared worker.
  void set_service_worker_provider_id(int id);
  void set_is_controlled_by_service_worker(bool flag);
  void set_parent_frame_id(int id);
  void set_site_for_cookies(const blink::WebURL& site_for_cookies);
  // Sets whether the worker context is a secure context.
  // https://w3c.github.io/webappsec-secure-contexts/
  void set_is_secure_context(bool flag);
  void set_origin_url(const GURL& origin_url);

 private:
  class URLLoaderFactoryImpl;

  bool Send(IPC::Message* message);

  // S13nServiceWorker:
  // Resets the service worker url loader factory of a URLLoaderFactoryImpl
  // which was passed to Blink. The url loader factory is connected to the
  // controller service worker. Sets nullptr if the worker context is not
  // controlled by a service worker.
  void ResetServiceWorkerURLLoaderFactory();

  mojo::Binding<mojom::ServiceWorkerWorkerClient> binding_;

  // Bound to |this| on the worker thread.
  mojom::ServiceWorkerWorkerClientRequest service_worker_client_request_;
  // Consumed on the worker thread to create |service_worker_container_host_|.
  mojom::ServiceWorkerContainerHostPtrInfo service_worker_container_host_info_;
  // Consumed on the worker thread to create |url_loader_factory_getter_|.
  ChildURLLoaderFactoryGetter::Info url_loader_factory_getter_info_;
  // Consumed on the worker thread to create |blob_registry_|.
  blink::mojom::BlobRegistryPtrInfo blob_registry_ptr_info_;

  int service_worker_provider_id_ = kInvalidServiceWorkerProviderId;
  bool is_controlled_by_service_worker_ = false;
  // S13nServiceWorker:
  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  mojom::ServiceWorkerContainerHostPtr service_worker_container_host_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  scoped_refptr<ChildURLLoaderFactoryGetter> url_loader_factory_getter_;

  // S13nServiceWorker:
  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  scoped_refptr<base::RefCountedData<blink::mojom::BlobRegistryPtr>>
      blob_registry_;

  // Updated when mojom::ServiceWorkerWorkerClient::SetControllerServiceWorker()
  // is called from the browser process via mojo IPC.
  int controller_version_id_ = blink::mojom::kInvalidServiceWorkerVersionId;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  std::unique_ptr<blink::WebDocumentSubresourceFilter::Builder>
      subresource_filter_builder_;
  bool is_on_sub_frame_ = false;
  int parent_frame_id_ = MSG_ROUTING_NONE;
  GURL site_for_cookies_;
  bool is_secure_context_ = false;
  GURL origin_url_;
  int appcache_host_id_ = blink::WebApplicationCacheHost::kAppCacheNoHostId;

  // A weak ptr to blink::WebURLLoaderFactory which was created and passed to
  // Blink by CreateURLLoaderFactory().
  base::WeakPtr<URLLoaderFactoryImpl> url_loader_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_WORKER_FETCH_CONTEXT_IMPL_H_
