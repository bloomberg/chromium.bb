// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/worker_fetch_context_impl.h"

#include "base/feature_list.h"
#include "content/child/child_thread_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/frame_messages.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/common/wrapper_shared_url_loader_factory.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/url_loader_throttle_provider.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/resource_dispatcher.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/service_worker/controller_service_worker_connector.h"
#include "content/renderer/service_worker/service_worker_subresource_loader.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"

namespace content {

class WorkerFetchContextImpl::URLLoaderFactoryImpl
    : public blink::WebURLLoaderFactory {
 public:
  URLLoaderFactoryImpl(
      base::WeakPtr<ResourceDispatcher> resource_dispatcher,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory)
      : resource_dispatcher_(std::move(resource_dispatcher)),
        loader_factory_(std::move(loader_factory)),
        weak_ptr_factory_(this) {}
  ~URLLoaderFactoryImpl() override = default;

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const blink::WebURLRequest& request,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override {
    DCHECK(task_runner);
    DCHECK(resource_dispatcher_);
    if (auto loader = CreateServiceWorkerURLLoader(request, task_runner))
      return loader;
    return std::make_unique<WebURLLoaderImpl>(
        resource_dispatcher_.get(), std::move(task_runner), loader_factory_);
  }

  void SetServiceWorkerURLLoaderFactory(
      network::mojom::URLLoaderFactoryPtr service_worker_url_loader_factory) {
    service_worker_url_loader_factory_ =
        base::MakeRefCounted<WrapperSharedURLLoaderFactory>(
            std::move(service_worker_url_loader_factory));
  }

  base::WeakPtr<URLLoaderFactoryImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::unique_ptr<blink::WebURLLoader> CreateServiceWorkerURLLoader(
      const blink::WebURLRequest& request,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    // TODO(horo): Unify this code path with
    // ServiceWorkerNetworkProvider::CreateURLLoader that is used for document
    // cases.

    // We need URLLoaderFactory populated in order to create our own URLLoader
    // for subresource loading via a service worker.
    if (!service_worker_url_loader_factory_)
      return nullptr;

    // If it's not for HTTP or HTTPS no need to intercept the request.
    if (!GURL(request.Url()).SchemeIsHTTPOrHTTPS())
      return nullptr;

    // If GetSkipServiceWorker() returns true, no need to intercept the request.
    if (request.GetSkipServiceWorker())
      return nullptr;

    // Create our own URLLoader to route the request to the controller service
    // worker.
    return std::make_unique<WebURLLoaderImpl>(
        resource_dispatcher_.get(), std::move(task_runner),
        service_worker_url_loader_factory_);
  }

  base::WeakPtr<ResourceDispatcher> resource_dispatcher_;
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      service_worker_url_loader_factory_;
  base::WeakPtrFactory<URLLoaderFactoryImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryImpl);
};

WorkerFetchContextImpl::WorkerFetchContextImpl(
    mojom::ServiceWorkerWorkerClientRequest service_worker_client_request,
    mojom::ServiceWorkerContainerHostPtrInfo service_worker_container_host_info,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        direct_network_factory_info,
    std::unique_ptr<URLLoaderThrottleProvider> throttle_provider)
    : binding_(this),
      service_worker_client_request_(std::move(service_worker_client_request)),
      service_worker_container_host_info_(
          std::move(service_worker_container_host_info)),
      url_loader_factory_info_(std::move(url_loader_factory_info)),
      direct_network_loader_factory_info_(
          std::move(direct_network_factory_info)),
      thread_safe_sender_(ChildThreadImpl::current()->thread_safe_sender()),
      throttle_provider_(std::move(throttle_provider)) {
  if (ServiceWorkerUtils::IsServicificationEnabled()) {
    ChildThreadImpl::current()->GetConnector()->BindInterface(
        mojom::kBrowserServiceName,
        mojo::MakeRequest(&blob_registry_ptr_info_));
  }
}

WorkerFetchContextImpl::~WorkerFetchContextImpl() {}

void WorkerFetchContextImpl::SetTerminateSyncLoadEvent(
    base::WaitableEvent* terminate_sync_load_event) {
  DCHECK(!terminate_sync_load_event_);
  terminate_sync_load_event_ = terminate_sync_load_event;
}

void WorkerFetchContextImpl::InitializeOnWorkerThread() {
  DCHECK(!resource_dispatcher_);
  DCHECK(!binding_.is_bound());
  resource_dispatcher_ = std::make_unique<ResourceDispatcher>();
  resource_dispatcher_->set_terminate_sync_load_event(
      terminate_sync_load_event_);

  shared_url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::move(url_loader_factory_info_));
  direct_network_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::move(direct_network_loader_factory_info_));
  if (service_worker_client_request_.is_pending())
    binding_.Bind(std::move(service_worker_client_request_));

  if (ServiceWorkerUtils::IsServicificationEnabled()) {
    service_worker_container_host_.Bind(
        std::move(service_worker_container_host_info_));

    blink::mojom::BlobRegistryPtr blob_registry_ptr;
    blob_registry_ptr.Bind(std::move(blob_registry_ptr_info_));
    blob_registry_ = base::MakeRefCounted<
        base::RefCountedData<blink::mojom::BlobRegistryPtr>>(
        std::move(blob_registry_ptr));
  }
}

std::unique_ptr<blink::WebURLLoaderFactory>
WorkerFetchContextImpl::CreateURLLoaderFactory() {
  DCHECK(shared_url_loader_factory_);
  DCHECK(!url_loader_factory_);
  auto factory = std::make_unique<URLLoaderFactoryImpl>(
      resource_dispatcher_->GetWeakPtr(), shared_url_loader_factory_);
  url_loader_factory_ = factory->GetWeakPtr();

  if (ServiceWorkerUtils::IsServicificationEnabled())
    ResetServiceWorkerURLLoaderFactory();

  return factory;
}

std::unique_ptr<blink::WebURLLoaderFactory>
WorkerFetchContextImpl::WrapURLLoaderFactory(
    mojo::ScopedMessagePipeHandle url_loader_factory_handle) {
  return std::make_unique<content::WebURLLoaderFactoryImpl>(
      resource_dispatcher_->GetWeakPtr(),
      base::MakeRefCounted<WrapperSharedURLLoaderFactory>(
          network::mojom::URLLoaderFactoryPtrInfo(
              std::move(url_loader_factory_handle),
              network::mojom::URLLoaderFactory::Version_)));
}

void WorkerFetchContextImpl::WillSendRequest(blink::WebURLRequest& request) {
  auto extra_data = std::make_unique<RequestExtraData>();
  extra_data->set_service_worker_provider_id(service_worker_provider_id_);
  extra_data->set_render_frame_id(parent_frame_id_);
  extra_data->set_initiated_in_secure_context(is_secure_context_);
  if (throttle_provider_) {
    extra_data->set_url_loader_throttles(throttle_provider_->CreateThrottles(
        parent_frame_id_, request, WebURLRequestToResourceType(request)));
  }
  request.SetExtraData(std::move(extra_data));
  request.SetAppCacheHostID(appcache_host_id_);

  if (!IsControlledByServiceWorker()) {
    // TODO(falken): Is still this needed? It used to set kForeign for foreign
    // fetch.
    request.SetSkipServiceWorker(true);
  }
}

bool WorkerFetchContextImpl::IsControlledByServiceWorker() const {
  return is_controlled_by_service_worker_ ||
         (controller_version_id_ !=
          blink::mojom::kInvalidServiceWorkerVersionId);
}

void WorkerFetchContextImpl::SetIsOnSubframe(bool is_on_sub_frame) {
  is_on_sub_frame_ = is_on_sub_frame;
}

bool WorkerFetchContextImpl::IsOnSubframe() const {
  return is_on_sub_frame_;
}

blink::WebURL WorkerFetchContextImpl::SiteForCookies() const {
  return site_for_cookies_;
}

void WorkerFetchContextImpl::DidRunContentWithCertificateErrors() {
  Send(new FrameHostMsg_DidRunContentWithCertificateErrors(parent_frame_id_));
}

void WorkerFetchContextImpl::DidDisplayContentWithCertificateErrors() {
  Send(new FrameHostMsg_DidDisplayContentWithCertificateErrors(
      parent_frame_id_));
}

void WorkerFetchContextImpl::DidRunInsecureContent(
    const blink::WebSecurityOrigin& origin,
    const blink::WebURL& url) {
  Send(new FrameHostMsg_DidRunInsecureContent(
      parent_frame_id_, GURL(origin.ToString().Utf8()), url));
}

void WorkerFetchContextImpl::SetSubresourceFilterBuilder(
    std::unique_ptr<blink::WebDocumentSubresourceFilter::Builder>
        subresource_filter_builder) {
  subresource_filter_builder_ = std::move(subresource_filter_builder);
}

std::unique_ptr<blink::WebDocumentSubresourceFilter>
WorkerFetchContextImpl::TakeSubresourceFilter() {
  if (!subresource_filter_builder_)
    return nullptr;
  return std::move(subresource_filter_builder_)->Build();
}

void WorkerFetchContextImpl::set_service_worker_provider_id(int id) {
  service_worker_provider_id_ = id;
}

void WorkerFetchContextImpl::set_is_controlled_by_service_worker(bool flag) {
  is_controlled_by_service_worker_ = flag;
}

void WorkerFetchContextImpl::set_parent_frame_id(int id) {
  parent_frame_id_ = id;
}

void WorkerFetchContextImpl::set_site_for_cookies(
    const blink::WebURL& site_for_cookies) {
  site_for_cookies_ = site_for_cookies;
}

void WorkerFetchContextImpl::set_is_secure_context(bool flag) {
  is_secure_context_ = flag;
}

void WorkerFetchContextImpl::set_origin_url(const GURL& origin_url) {
  origin_url_ = origin_url;
}

void WorkerFetchContextImpl::SetApplicationCacheHostID(int id) {
  appcache_host_id_ = id;
}

int WorkerFetchContextImpl::ApplicationCacheHostID() const {
  return appcache_host_id_;
}

void WorkerFetchContextImpl::SetControllerServiceWorker(
    int64_t controller_version_id) {
  controller_version_id_ = controller_version_id;
  if (ServiceWorkerUtils::IsServicificationEnabled())
    ResetServiceWorkerURLLoaderFactory();
}

bool WorkerFetchContextImpl::Send(IPC::Message* message) {
  return thread_safe_sender_->Send(message);
}

void WorkerFetchContextImpl::ResetServiceWorkerURLLoaderFactory() {
  DCHECK(ServiceWorkerUtils::IsServicificationEnabled());
  if (!url_loader_factory_)
    return;
  if (!IsControlledByServiceWorker()) {
    url_loader_factory_->SetServiceWorkerURLLoaderFactory(nullptr);
    return;
  }
  network::mojom::URLLoaderFactoryPtr service_worker_url_loader_factory;
  ServiceWorkerSubresourceLoaderFactory::Create(
      base::MakeRefCounted<ControllerServiceWorkerConnector>(
          service_worker_container_host_.get()),
      direct_network_loader_factory_,
      mojo::MakeRequest(&service_worker_url_loader_factory));
  url_loader_factory_->SetServiceWorkerURLLoaderFactory(
      std::move(service_worker_url_loader_factory));
}

}  // namespace content
