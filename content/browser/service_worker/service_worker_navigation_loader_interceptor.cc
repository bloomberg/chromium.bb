// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_navigation_loader_interceptor.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_main_resource_handle_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace content {

namespace {

///////////////////////////////////////////////////////////////////////////////
// Core thread helpers

void LoaderCallbackWrapperOnCoreThread(
    ServiceWorkerMainResourceHandleCore* handle_core,
    base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> interceptor_on_ui,
    NavigationLoaderInterceptor::LoaderCallback loader_callback,
    SingleRequestURLLoaderFactory::RequestHandler handler) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  base::Optional<SubresourceLoaderParams> subresource_loader_params;
  if (handle_core->interceptor()) {
    subresource_loader_params =
        handle_core->interceptor()->MaybeCreateSubresourceLoaderParams();
  }

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::LoaderCallbackWrapper,
          interceptor_on_ui, std::move(subresource_loader_params),
          std::move(loader_callback), std::move(handler)));
}

void FallbackCallbackWrapperOnCoreThread(
    base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> interceptor_on_ui,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback,
    bool reset_subresource_loader_params) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::FallbackCallbackWrapper,
          interceptor_on_ui, std::move(fallback_callback),
          reset_subresource_loader_params));
}

void InvokeRequestHandlerOnCoreThread(
    SingleRequestURLLoaderFactory::RequestHandler handler,
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  std::move(handler).Run(resource_request, std::move(receiver),
                         std::move(client_remote));
}

// Does setup on the the core thread and calls back to
// |interceptor_on_ui->LoaderCallbackWrapper()| on the UI thread.
void MaybeCreateLoaderOnCoreThread(
    base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> interceptor_on_ui,
    ServiceWorkerMainResourceHandleCore* handle_core,
    blink::mojom::ResourceType resource_type,
    bool skip_service_worker,
    bool are_ancestors_secure,
    int frame_tree_node_id,
    int process_id,
    DedicatedWorkerId dedicated_worker_id,
    SharedWorkerId shared_worker_id,
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        client_remote,
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    NavigationLoaderInterceptor::LoaderCallback loader_callback,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback,
    bool initialize_container_host_only) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  ServiceWorkerContextCore* context_core =
      handle_core->context_wrapper()->context();
  ResourceContext* resource_context =
      ServiceWorkerContext::IsServiceWorkerOnUIEnabled()
          ? nullptr
          : handle_core->context_wrapper()->resource_context();
  if (!context_core || (!resource_context && !browser_context)) {
    LoaderCallbackWrapperOnCoreThread(handle_core, std::move(interceptor_on_ui),
                                      std::move(loader_callback),
                                      /*handler=*/{});
    return;
  }

  if (!handle_core->container_host()) {
    // This is the initial request before redirects, so make the container host.
    // Its lifetime is tied to the |provider_info| in the
    // ServiceWorkerMainResourceHandle on the UI thread and which will be passed
    // to the renderer when the navigation commits.
    DCHECK(host_receiver);
    DCHECK(client_remote);
    base::WeakPtr<ServiceWorkerContainerHost> container_host;

    if (resource_type == blink::mojom::ResourceType::kMainFrame ||
        resource_type == blink::mojom::ResourceType::kSubFrame) {
      container_host = context_core->CreateContainerHostForWindow(
          std::move(host_receiver), are_ancestors_secure,
          std::move(client_remote), frame_tree_node_id);
    } else {
      DCHECK(resource_type == blink::mojom::ResourceType::kWorker ||
             resource_type == blink::mojom::ResourceType::kSharedWorker);
      auto client_type =
          resource_type == blink::mojom::ResourceType::kWorker
              ? blink::mojom::ServiceWorkerClientType::kDedicatedWorker
              : blink::mojom::ServiceWorkerClientType::kSharedWorker;
      container_host = context_core->CreateContainerHostForWorker(
          std::move(host_receiver), process_id, std::move(client_remote),
          client_type, dedicated_worker_id, shared_worker_id);
    }
    DCHECK(container_host);
    handle_core->set_container_host(container_host);

    // Also make the inner interceptor.
    DCHECK(!handle_core->interceptor());
    handle_core->set_interceptor(
        std::make_unique<ServiceWorkerControlleeRequestHandler>(
            context_core->AsWeakPtr(), container_host, resource_type,
            skip_service_worker,
            handle_core->service_worker_accessed_callback()));
  }

  // If |initialize_container_host_only| is true, we have already determined
  // there is no registered service worker on the UI thread, so just initialize
  // the container host for this request.
  if (initialize_container_host_only) {
    handle_core->interceptor()->InitializeContainerHost(
        tentative_resource_request);
    LoaderCallbackWrapperOnCoreThread(handle_core, interceptor_on_ui,
                                      std::move(loader_callback),
                                      /*handler=*/{});
    return;
  }

  // Start the inner interceptor. We continue in
  // LoaderCallbackWrapperOnCoreThread().
  //
  // It's safe to bind the raw |handle_core| to the callback because it owns the
  // interceptor, which invokes the callback.
  handle_core->interceptor()->MaybeCreateLoader(
      tentative_resource_request, browser_context, resource_context,
      base::BindOnce(&LoaderCallbackWrapperOnCoreThread, handle_core,
                     interceptor_on_ui, std::move(loader_callback)),
      base::BindOnce(&FallbackCallbackWrapperOnCoreThread, interceptor_on_ui,
                     std::move(fallback_callback)));
}

bool SchemeMaySupportRedirectingToHTTPS(const GURL& url) {
#if defined(OS_CHROMEOS)
  return url.SchemeIs(kExternalFileScheme);
#else   // OS_CHROMEOS
  return false;
#endif  // OS_CHROMEOS
}

// Returns true if a ServiceWorkerNavigationLoaderInterceptor should be created
// for a worker with this |url|.
bool ShouldCreateForWorker(const GURL& url) {
  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  return url.SchemeIsHTTPOrHTTPS() || OriginCanAccessServiceWorkers(url);
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace

std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerNavigationLoaderInterceptor::CreateForNavigation(
    const GURL& url,
    base::WeakPtr<ServiceWorkerMainResourceHandle> navigation_handle,
    const NavigationRequestInfo& request_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ShouldCreateForNavigation(url))
    return nullptr;

  return std::unique_ptr<ServiceWorkerNavigationLoaderInterceptor>(
      new ServiceWorkerNavigationLoaderInterceptor(
          std::move(navigation_handle),
          request_info.is_main_frame ? blink::mojom::ResourceType::kMainFrame
                                     : blink::mojom::ResourceType::kSubFrame,
          request_info.begin_params->skip_service_worker,
          request_info.are_ancestors_secure, request_info.frame_tree_node_id,
          ChildProcessHost::kInvalidUniqueID, DedicatedWorkerId(),
          SharedWorkerId()));
}

std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerNavigationLoaderInterceptor::CreateForWorker(
    const network::ResourceRequest& resource_request,
    int process_id,
    DedicatedWorkerId dedicated_worker_id,
    SharedWorkerId shared_worker_id,
    base::WeakPtr<ServiceWorkerMainResourceHandle> navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto resource_type =
      static_cast<blink::mojom::ResourceType>(resource_request.resource_type);
  DCHECK(resource_type == blink::mojom::ResourceType::kWorker ||
         resource_type == blink::mojom::ResourceType::kSharedWorker)
      << resource_request.resource_type;

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!ShouldCreateForWorker(resource_request.url))
    return nullptr;

  return std::unique_ptr<ServiceWorkerNavigationLoaderInterceptor>(
      new ServiceWorkerNavigationLoaderInterceptor(
          std::move(navigation_handle), resource_type,
          resource_request.skip_service_worker, /*are_ancestors_secure=*/false,
          FrameTreeNode::kFrameTreeNodeInvalidId, process_id,
          dedicated_worker_id, shared_worker_id));
}

ServiceWorkerNavigationLoaderInterceptor::
    ~ServiceWorkerNavigationLoaderInterceptor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerNavigationLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    LoaderCallback loader_callback,
    FallbackCallback fallback_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_);

  mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
      host_receiver;
  mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
      client_remote;

  // If this is the first request before redirects, a provider info has not yet
  // been created.
  if (!handle_->has_provider_info()) {
    auto provider_info =
        blink::mojom::ServiceWorkerProviderInfoForClient::New();
    host_receiver =
        provider_info->host_remote.InitWithNewEndpointAndPassReceiver();
    provider_info->client_receiver =
        client_remote.InitWithNewEndpointAndPassReceiver();
    handle_->OnCreatedProviderHost(std::move(provider_info));
  }

  bool initialize_container_host_only = false;
  LoaderCallback original_callback;
  if (!ServiceWorkerContext::IsServiceWorkerOnUIEnabled() &&
      !handle_->context_wrapper()->HasRegistrationForOrigin(
          tentative_resource_request.url.GetOrigin())) {
    // We have no registrations, so it's safe to continue the request now
    // without blocking on the IO thread. Give a dummy callback to the
    // IO thread interceptor, and we'll run the original callback immediately
    // after starting it.
    original_callback = std::move(loader_callback);
    loader_callback =
        base::BindOnce([](scoped_refptr<network::SharedURLLoaderFactory>) {});
    initialize_container_host_only = true;
  }

  // Start the inner interceptor on the core thread. It will call back to
  // LoaderCallbackWrapper() on the UI thread.
  ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(&MaybeCreateLoaderOnCoreThread, GetWeakPtr(),
                     handle_->core(), resource_type_, skip_service_worker_,
                     are_ancestors_secure_, frame_tree_node_id_, process_id_,
                     dedicated_worker_id_, shared_worker_id_,
                     std::move(host_receiver), std::move(client_remote),
                     tentative_resource_request, browser_context,
                     std::move(loader_callback), std::move(fallback_callback),
                     initialize_container_host_only));

  if (original_callback)
    std::move(original_callback).Run({});
}

base::Optional<SubresourceLoaderParams>
ServiceWorkerNavigationLoaderInterceptor::MaybeCreateSubresourceLoaderParams() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::move(subresource_loader_params_);
}

void ServiceWorkerNavigationLoaderInterceptor::LoaderCallbackWrapper(
    base::Optional<SubresourceLoaderParams> subresource_loader_params,
    LoaderCallback loader_callback,
    SingleRequestURLLoaderFactory::RequestHandler handler_on_core_thread) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // For worker main script requests, |handle_| can be destroyed during
  // interception. The initiator of this interceptor (i.e., WorkerScriptLoader)
  // will handle the case.
  // For navigation requests, this case should not happen because it's
  // guaranteed that this interceptor is destroyed before |handle_|.
  if (!handle_) {
    std::move(loader_callback).Run({});
    return;
  }

  subresource_loader_params_ = std::move(subresource_loader_params);

  if (!handler_on_core_thread) {
    std::move(loader_callback).Run({});
    return;
  }

  // The inner core thread interceptor wants to handle the request. However,
  // |handler_on_core_thread| expects to run on the core thread. Give our own
  // wrapper to the loader callback.
  std::move(loader_callback)
      .Run(base::MakeRefCounted<SingleRequestURLLoaderFactory>(base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::RequestHandlerWrapper,
          GetWeakPtr(), std::move(handler_on_core_thread))));
}

void ServiceWorkerNavigationLoaderInterceptor::FallbackCallbackWrapper(
    FallbackCallback fallback_callback,
    bool reset_subresource_loader_params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(fallback_callback).Run(reset_subresource_loader_params);
}

base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor>
ServiceWorkerNavigationLoaderInterceptor::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return weak_factory_.GetWeakPtr();
}

ServiceWorkerNavigationLoaderInterceptor::
    ServiceWorkerNavigationLoaderInterceptor(
        base::WeakPtr<ServiceWorkerMainResourceHandle> handle,
        blink::mojom::ResourceType resource_type,
        bool skip_service_worker,
        bool are_ancestors_secure,
        int frame_tree_node_id,
        int process_id,
        DedicatedWorkerId dedicated_worker_id,
        SharedWorkerId shared_worker_id)
    : handle_(std::move(handle)),
      resource_type_(resource_type),
      skip_service_worker_(skip_service_worker),
      are_ancestors_secure_(are_ancestors_secure),
      frame_tree_node_id_(frame_tree_node_id),
      process_id_(process_id),
      dedicated_worker_id_(dedicated_worker_id),
      shared_worker_id_(shared_worker_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_);
}

// static
bool ServiceWorkerNavigationLoaderInterceptor::ShouldCreateForNavigation(
    const GURL& url) {
  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  return url.SchemeIsHTTPOrHTTPS() || OriginCanAccessServiceWorkers(url) ||
         SchemeMaySupportRedirectingToHTTPS(url);
}

void ServiceWorkerNavigationLoaderInterceptor::RequestHandlerWrapper(
    SingleRequestURLLoaderFactory::RequestHandler handler_on_core_thread,
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerContextWrapper::RunOrPostTaskOnCoreThread(
      FROM_HERE,
      base::BindOnce(InvokeRequestHandlerOnCoreThread,
                     std::move(handler_on_core_thread), resource_request,
                     std::move(receiver), std::move(client)));
}

}  // namespace content
