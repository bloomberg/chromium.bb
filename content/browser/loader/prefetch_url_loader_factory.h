// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_FACTORY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/WebKit/common/loader/prefetch_url_loader_service.mojom.h"

namespace content {

class URLLoaderFactoryGetter;

class CONTENT_EXPORT PrefetchURLLoaderFactory final
    : public base::RefCountedThreadSafe<PrefetchURLLoaderFactory,
                                        BrowserThread::DeleteOnIOThread>,
      public network::mojom::URLLoaderFactory,
      public blink::mojom::PrefetchURLLoaderService {
 public:
  // |factory_getter| could be null in non-NetworkService case.
  explicit PrefetchURLLoaderFactory(
      scoped_refptr<URLLoaderFactoryGetter> network_loader_factory);

  void ConnectToService(blink::mojom::PrefetchURLLoaderServiceRequest request);

  // blink::mojom::PrefetchURLLoaderService:
  void GetFactory(network::mojom::URLLoaderFactoryRequest request) override;

  // Used only when NetworkService is not enabled (or indirectly via the
  // other CreateLoaderAndStart when NetworkService is enabled).
  // This creates a loader and starts fetching using the given
  // |network_lader_factory|.
  void CreateLoaderAndStart(
      network::mojom::URLLoaderRequest request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      network::mojom::URLLoaderFactory* factory);

  // Register a callback that is fired right before a prefetch load is started
  // by this factory.
  void RegisterPrefetchLoaderCallbackForTest(
      const base::RepeatingClosure& prefetch_load_callback) {
    prefetch_load_callback_for_testing_ = prefetch_load_callback;
  }

 private:
  friend class base::DeleteHelper<content::PrefetchURLLoaderFactory>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  ~PrefetchURLLoaderFactory() override;

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

  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;
  mojo::BindingSet<blink::mojom::PrefetchURLLoaderService> service_bindings_;

  scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter_;
  network::mojom::URLLoaderFactoryPtr network_loader_factory_;

  base::RepeatingClosure prefetch_load_callback_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_PREFETCH_URL_LOADER_FACTORY_H_
