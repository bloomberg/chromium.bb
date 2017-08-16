// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_URL_LOADER_FACTORY_H_

#include "base/macros.h"
#include "content/public/common/url_loader_factory.mojom.h"

namespace storage {
class BlobStorageContext;
}  // namespace storage

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class URLLoaderFactoryGetter;

// S13nServiceWorker:
// Created per one controller worker for script loading (only during
// installation, eventually). This is kept alive while
// ServiceWorkerNetworkProvider in the renderer process is alive.
// Used only when IsServicificationEnabled is true.
class ServiceWorkerScriptURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  ServiceWorkerScriptURLLoaderFactory(
      base::WeakPtr<ServiceWorkerContextCore> context,
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter);
  ~ServiceWorkerScriptURLLoaderFactory() override;

  // mojom::URLLoaderFactory:
  void CreateLoaderAndStart(mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& resource_request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojom::URLLoaderFactoryRequest request) override;

 private:
  bool ShouldHandleScriptRequest(const ResourceRequest& resource_request);

  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerScriptURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_SCRIPT_URL_LOADER_FACTORY_H_
