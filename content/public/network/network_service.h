// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_NETWORK_NETWORK_SERVICE_H_
#define CONTENT_PUBLIC_NETWORK_NETWORK_SERVICE_H_

#include <memory>

#include "content/common/content_export.h"
#include "content/public/common/network_service.mojom.h"

namespace net {
class URLRequestContext;
class URLRequestContextBuilder;
}  // namespace net

namespace content {

// Allows an in-process NetworkService to be set up.
class CONTENT_EXPORT NetworkService : public mojom::NetworkService {
 public:
  // TODO(mmenke): Take in a NetworkServiceRequest.
  static std::unique_ptr<NetworkService> Create();

  // Can be used to seed a NetworkContext with a consumer-configured
  // URLRequestContextBuilder, which |params| will then be applied to. The
  // results URLRequestContext will be written to |url_request_context|, which
  // is owned by the NetworkContext, and can be further modified before first
  // use. The returned NetworkContext must be destroyed before the
  // NetworkService.
  //
  // This method is intended to ease the transition to an out-of-process
  // NetworkService, and will be removed once that ships.
  virtual std::unique_ptr<mojom::NetworkContext>
  CreateNetworkContextWithBuilder(
      content::mojom::NetworkContextRequest request,
      content::mojom::NetworkContextParamsPtr params,
      std::unique_ptr<net::URLRequestContextBuilder> builder,
      net::URLRequestContext** url_request_context) = 0;

  ~NetworkService() override {}

 protected:
  NetworkService() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_NETWORK_NETWORK_SERVICE_H_
