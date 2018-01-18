// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_NETWORK_NETWORK_SERVICE_H_
#define CONTENT_PUBLIC_NETWORK_NETWORK_SERVICE_H_

#include <memory>

#include "content/common/content_export.h"
#include "services/network/public/interfaces/network_service.mojom.h"

namespace net {
class NetLog;
class URLRequestContext;
}  // namespace net

namespace content {

class URLRequestContextBuilderMojo;

// Allows an in-process NetworkService to be set up.
class CONTENT_EXPORT NetworkService : public network::mojom::NetworkService {
 public:
  // Creates a NetworkService instance on the current thread, optionally using
  // the passed-in NetLog. Does not take ownership of |net_log|. Must be
  // destroyed before |net_log|.
  //
  // TODO(https://crbug.com/767450): Make it so NetworkService can always create
  // its own NetLog, instead of sharing one.
  static std::unique_ptr<NetworkService> Create(
      network::mojom::NetworkServiceRequest request,
      net::NetLog* net_log = nullptr);

  // Can be used to seed a NetworkContext with a consumer-configured
  // URLRequestContextBuilder, which |params| will then be applied to. The
  // results URLRequestContext will be written to |url_request_context|, which
  // is owned by the NetworkContext, and can be further modified before first
  // use. The returned NetworkContext must be destroyed before the
  // NetworkService.
  //
  // This method is intended to ease the transition to an out-of-process
  // NetworkService, and will be removed once that ships. It should only be
  // called if the network service is disabled.
  virtual std::unique_ptr<network::mojom::NetworkContext>
  CreateNetworkContextWithBuilder(
      network::mojom::NetworkContextRequest request,
      network::mojom::NetworkContextParamsPtr params,
      std::unique_ptr<URLRequestContextBuilderMojo> builder,
      net::URLRequestContext** url_request_context) = 0;

  ~NetworkService() override {}

 protected:
  NetworkService() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_NETWORK_NETWORK_SERVICE_H_
