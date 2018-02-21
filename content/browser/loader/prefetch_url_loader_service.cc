// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/prefetch_url_loader_service.h"

#include "base/feature_list.h"
#include "content/browser/loader/prefetch_url_loader.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/weak_wrapper_shared_url_loader_factory.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/shared_url_loader_factory.h"
#include "content/public/common/url_loader_throttle.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

namespace {

// Per-frame URLLoaderFactory. Used only when Network Service is enabled.
class PrefetchURLLoaderFactory : public network::mojom::URLLoaderFactory,
                                 public blink::mojom::PrefetchURLLoaderService {
 public:
  PrefetchURLLoaderFactory(
      content::PrefetchURLLoaderService* service,
      scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter,
      int frame_tree_node_id)
      : service_(std::move(service)),
        loader_factory_getter_(std::move(loader_factory_getter)),
        frame_tree_node_id_(frame_tree_node_id) {}
  ~PrefetchURLLoaderFactory() override = default;

  // blink::mojom::PrefetchURLLoaderService:
  void GetFactory(network::mojom::URLLoaderFactoryRequest request) override {
    Clone(std::move(request));
  }

 private:
  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& resource_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
    if (!network_loader_factory_ ||
        network_loader_factory_.encountered_error()) {
      loader_factory_getter_->GetNetworkFactory()->Clone(
          mojo::MakeRequest(&network_loader_factory_));
    }
    service_->CreateLoaderAndStart(
        std::move(request), routing_id, request_id, options, resource_request,
        std::move(client), traffic_annotation,
        // NOTE: This should be fine in most cases, where the loader
        // factory may become invalid if Network Service process is killed
        // and restarted. The load can just fail in the case here, but if
        // we want to be extra sure this should create a SharedURLLoaderFactory
        // that internally holds a ref to the URLLoaderFactoryGetter so that
        // it can re-obtain the factory if necessary.
        base::MakeRefCounted<WeakWrapperSharedURLLoaderFactory>(
            network_loader_factory_.get()),
        frame_tree_node_id_);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    bindings_.AddBinding(this, std::move(request));
  }

  scoped_refptr<content::PrefetchURLLoaderService> service_;

  network::mojom::URLLoaderFactoryPtr network_loader_factory_;
  scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter_;
  int frame_tree_node_id_;

  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchURLLoaderFactory);
};

}  // namespace

PrefetchURLLoaderService::PrefetchURLLoaderService(
    scoped_refptr<URLLoaderFactoryGetter> factory_getter)
    : loader_factory_getter_(std::move(factory_getter)) {}

void PrefetchURLLoaderService::InitializeResourceContext(
    ResourceContext* resource_context,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!resource_context_);
  DCHECK(!request_context_getter_);
  resource_context_ = resource_context;
  request_context_getter_ = request_context_getter;
}

void PrefetchURLLoaderService::ConnectToService(
    int frame_tree_node_id,
    blink::mojom::PrefetchURLLoaderServiceRequest request) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&PrefetchURLLoaderService::ConnectToService, this,
                       frame_tree_node_id, std::move(request)));
    return;
  }
  service_bindings_.AddBinding(
      std::make_unique<PrefetchURLLoaderFactory>(this, loader_factory_getter_,
                                                 frame_tree_node_id),
      std::move(request));
}

void PrefetchURLLoaderService::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<SharedURLLoaderFactory> network_loader_factory,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(RESOURCE_TYPE_PREFETCH, resource_request.resource_type);
  DCHECK(resource_context_);

  if (prefetch_load_callback_for_testing_)
    prefetch_load_callback_for_testing_.Run();

  // For now we strongly bind the loader to the request, while we can
  // also possibly make the new loader owned by the factory so that
  // they can live longer than the client (i.e. run in detached mode).
  // TODO(kinuko): Revisit this.
  mojo::MakeStrongBinding(
      std::make_unique<PrefetchURLLoader>(
          routing_id, request_id, options, resource_request, std::move(client),
          traffic_annotation, std::move(network_loader_factory),
          base::BindRepeating(
              &PrefetchURLLoaderService::CreateURLLoaderThrottles, this,
              resource_request, frame_tree_node_id),
          resource_context_, request_context_getter_),
      std::move(request));
}

PrefetchURLLoaderService::~PrefetchURLLoaderService() = default;

std::vector<std::unique_ptr<content::URLLoaderThrottle>>
PrefetchURLLoaderService::CreateURLLoaderThrottles(
    const network::ResourceRequest& request,
    int frame_tree_node_id) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService) ||
      !request_context_getter_ ||
      !request_context_getter_->GetURLRequestContext())
    return std::vector<std::unique_ptr<content::URLLoaderThrottle>>();

  return GetContentClient()->browser()->CreateURLLoaderThrottles(
      request, resource_context_,
      base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                          frame_tree_node_id),
      nullptr /* navigation_ui_data */, frame_tree_node_id);
}

}  // namespace content
