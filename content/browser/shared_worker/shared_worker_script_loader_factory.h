// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SCRIPT_LOADER_FACTORY_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SCRIPT_LOADER_FACTORY_H_

#include "base/macros.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class ServiceWorkerContextWrapper;
class ServiceWorkerProviderHost;
class URLLoaderFactoryGetter;
class ResourceContext;

// S13nServiceWorker:
// Created per one running shared worker for loading its script.
//
// Shared worker script loads require special logic because they are similiar to
// navigations from the point of view of web platform features like service
// worker.
//
// This creates a SharedWorkerScriptLoader to load the script, which follows
// redirects and sets the controller service worker on the shared worker if
// needed.
class SharedWorkerScriptLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  SharedWorkerScriptLoaderFactory(
      ServiceWorkerContextWrapper* context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ResourceContext* resource_context,
      scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter);
  ~SharedWorkerScriptLoaderFactory() override;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& resource_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;

 private:
  base::WeakPtr<ServiceWorkerProviderHost> service_worker_provider_host_;
  ResourceContext* resource_context_ = nullptr;
  scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerScriptLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_SCRIPT_LOADER_FACTORY_H_
