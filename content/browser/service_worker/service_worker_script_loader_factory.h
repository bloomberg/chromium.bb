// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_LOADER_FACTORY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_LOADER_FACTORY_H_

#include "base/macros.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class URLLoaderFactoryGetter;

// S13nServiceWorker:
// Created per one running service worker for loading its scripts. This is kept
// alive while ServiceWorkerNetworkProvider in the renderer process is alive.
//
// This factory handles requests for the scripts of a new (installing)
// service worker. For installed workers, service worker script streaming
// (ServiceWorkerInstalledScriptsSender) is typically used instead. However,
// this factory can still be used when an installed worker imports a
// non-installed script (https://crbug.com/719052).
//
// This factory creates either a ServiceWorkerNewScriptLoader or a
// ServiceWorkerInstalledScriptLoader to load a script.
class ServiceWorkerScriptLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  // |loader_factory_getter| is used to get the network factory for
  // loading the script.
  //
  // |non_network_loader_factory| is non-null when the service worker main
  // script URL has a non-http(s) scheme (e.g., a chrome-extension:// URL).
  // It will be used to load the main script and any non-http(s) imported
  // script.
  //
  // It's assumed that the non-network factory can handle any non-http(s) URL
  // that this service worker may load.
  // TODO(crbug.com/836720): We probably need to pass the whole (scheme ->
  // factory) map instead of just giving a single factory, since it's
  // conceivable a service worker can import scripts from multiple schemes:
  // importScripts(['scheme1://blah', 'scheme2://blah', 'scheme3://blah']). So
  // far it's not needed with the Chrome embedder since it's only used for
  // chrome-extension:// URLs.
  ServiceWorkerScriptLoaderFactory(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter,
      network::mojom::URLLoaderFactoryPtr non_network_loader_factory);
  ~ServiceWorkerScriptLoaderFactory() override;

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
  bool ShouldHandleScriptRequest(
      const network::ResourceRequest& resource_request);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter_;
  network::mojom::URLLoaderFactoryPtr non_network_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_LOADER_FACTORY_H_
