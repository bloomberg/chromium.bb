// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_SUBRESOURCE_URL_FACTORY_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_SUBRESOURCE_URL_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace content {

class AppCacheHost;
class AppCacheJob;
class AppCacheServiceImpl;
class URLLoaderFactoryGetter;

// Implements the URLLoaderFactory mojom for AppCache subresource requests.
class CONTENT_EXPORT AppCacheSubresourceURLFactory
    : public mojom::URLLoaderFactory {
 public:
  ~AppCacheSubresourceURLFactory() override;

  // Factory function to create an instance of the factory.
  // 1. The |factory_getter| parameter is used to query the network service
  //    to pass network requests to.
  // 2. The |host| parameter contains the appcache host instance. This is used
  //    to create the AppCacheRequestHandler instances for handling subresource
  //    requests.
  static void CreateURLLoaderFactory(
      URLLoaderFactoryGetter* factory_getter,
      base::WeakPtr<AppCacheHost> host,
      mojom::URLLoaderFactoryPtr* loader_factory);

  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojom::URLLoaderRequest url_loader_request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& request,
                            mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojom::URLLoaderFactoryRequest request) override;

 private:
  friend class AppCacheNetworkServiceBrowserTest;

  AppCacheSubresourceURLFactory(URLLoaderFactoryGetter* factory_getter,
                                base::WeakPtr<AppCacheHost> host);

  void OnConnectionError();

  // Notifies the |client| if there is a failure. The |error_code| contains the
  // actual error.
  void NotifyError(mojom::URLLoaderClientPtr client, int error_code);

  // Mojo bindings.
  mojo::BindingSet<mojom::URLLoaderFactory> bindings_;

  // Used to retrieve the network service factory to pass unhandled requests to
  // the network service.
  scoped_refptr<URLLoaderFactoryGetter> default_url_loader_factory_getter_;

  base::WeakPtr<AppCacheHost> appcache_host_;

  DISALLOW_COPY_AND_ASSIGN(AppCacheSubresourceURLFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_URL_LOADER_FACTORY_H_
