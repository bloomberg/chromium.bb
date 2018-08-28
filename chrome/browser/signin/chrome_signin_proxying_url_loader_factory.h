// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PROXYING_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PROXYING_URL_LOADER_FACTORY_H_

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "content/public/browser/resource_request_info.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

#include <memory>
#include <set>

namespace signin {

class HeaderModificationDelegate;

// This class is used to modify sub-resource requests made by the renderer
// that is displaying the GAIA signin realm, to the GAIA signin realm. When
// such a request is made a proxy is inserted between the renderer and the
// Network Service to modify request and response headers.
class ProxyingURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  ProxyingURLLoaderFactory();
  ~ProxyingURLLoaderFactory() override;

  void StartProxying(
      std::unique_ptr<HeaderModificationDelegate> delegate,
      content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
      network::mojom::URLLoaderFactoryRequest request,
      network::mojom::URLLoaderFactoryPtrInfo target_factory,
      base::OnceClosure on_disconnect);

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader_request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest loader_request) override;

 private:
  friend class base::DeleteHelper<ProxyingURLLoaderFactory>;
  friend class base::RefCountedDeleteOnSequence<ProxyingURLLoaderFactory>;

  class InProgressRequest;
  class ProxyRequestAdapter;
  class ProxyResponseAdapter;

  void OnTargetFactoryError();
  void OnProxyBindingError();
  void RemoveRequest(InProgressRequest* request);
  void MaybeDestroySelf();

  const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter() {
    return web_contents_getter_;
  }

  std::unique_ptr<HeaderModificationDelegate> delegate_;
  content::ResourceRequestInfo::WebContentsGetter web_contents_getter_;

  mojo::BindingSet<network::mojom::URLLoaderFactory> proxy_bindings_;
  std::set<std::unique_ptr<InProgressRequest>, base::UniquePtrComparator>
      requests_;
  network::mojom::URLLoaderFactoryPtr target_factory_;
  base::OnceClosure on_disconnect_;

  DISALLOW_COPY_AND_ASSIGN(ProxyingURLLoaderFactory);
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PROXYING_URL_LOADER_FACTORY_H_
