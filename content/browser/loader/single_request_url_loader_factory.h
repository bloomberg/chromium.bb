// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SINGLE_REQUEST_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_LOADER_SINGLE_REQUEST_URL_LOADER_FACTORY_H_

#include "services/network/public/cpp/shared_url_loader_factory.h"

#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

// An implementation of SharedURLLoaderFactory which handles only a single
// request. It's an error to call CreateLoaderAndStart() more than a total of
// one time across this object or any of its clones.
class SingleRequestURLLoaderFactory : public network::SharedURLLoaderFactory {
 public:
  using RequestHandler =
      base::OnceCallback<void(const network::ResourceRequest& resource_request,
                              network::mojom::URLLoaderRequest,
                              network::mojom::URLLoaderClientPtr)>;

  explicit SingleRequestURLLoaderFactory(RequestHandler handler);

  // SharedURLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override;

 private:
  class FactoryInfo;
  class HandlerState;

  explicit SingleRequestURLLoaderFactory(scoped_refptr<HandlerState> state);
  ~SingleRequestURLLoaderFactory() override;

  scoped_refptr<HandlerState> state_;

  DISALLOW_COPY_AND_ASSIGN(SingleRequestURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SINGLE_REQUEST_URL_LOADER_FACTORY_H_
