// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_SUBRESOURCE_URL_FACTORY_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_SUBRESOURCE_URL_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "content/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace content {

class AppCacheJob;
class AppCacheServiceImpl;
class URLLoaderFactoryGetter;

// Implements the URLLoaderFactory mojom for AppCache subresource requests.
class AppCacheSubresourceURLFactory : public mojom::URLLoaderFactory {
 public:
  ~AppCacheSubresourceURLFactory() override;

  // Factory function to create an instance of the factory.
  // 1. The |factory_getter| parameter is used to query the network service
  //    to pass network requests to.
  // Returns a URLLoaderFactoryPtr instance which controls the lifetime of the
  // factory.
  static mojom::URLLoaderFactoryPtr CreateURLLoaderFactory(
      URLLoaderFactoryGetter* factory_getter);

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojom::URLLoaderAssociatedRequest url_loader_request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const ResourceRequest& request,
      mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void SyncLoad(int32_t routing_id,
                int32_t request_id,
                const ResourceRequest& request,
                SyncLoadCallback callback) override;

 private:
  AppCacheSubresourceURLFactory(mojom::URLLoaderFactoryRequest request,
                                URLLoaderFactoryGetter* factory_getter);

  void OnConnectionError();

  // Mojo binding.
  mojo::Binding<mojom::URLLoaderFactory> binding_;

  // Used to retrieve the network service factory to pass unhandled requests to
  // the network service.
  scoped_refptr<URLLoaderFactoryGetter> default_url_loader_factory_getter_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheSubresourceURLFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_FACTORY_H_
