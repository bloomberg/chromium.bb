// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_HINTS_BROWSER_NETWORK_HINTS_MESSAGE_FILTER_H_
#define COMPONENTS_NETWORK_HINTS_BROWSER_NETWORK_HINTS_MESSAGE_FILTER_H_

#include "base/macros.h"
#include "components/network_hints/public/interfaces/network_hints.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace net {
class HostResolver;
}

namespace network_hints {
struct LookupRequest;

// Simple browser-side handler for DNS prefetch requests.
// Passes prefetch requests to the provided net::HostResolver.
// Each renderer process requires its own filter.
class NetworkHintsImpl : public network_hints::mojom::NetworkHints {
 public:
  explicit NetworkHintsImpl(net::HostResolver* host_resolver);
  ~NetworkHintsImpl() override;

  void Bind(mojo::InterfaceRequest<mojom::NetworkHints> request);

 private:
  // network_hints::mojom::NetworkHints implementation:
  void DNSPrefetch(const LookupRequest& lookup_request) override;
  void Preconnect(const GURL& url,
                  bool allow_credentials,
                  int32_t count) override {}

  net::HostResolver* host_resolver_;

  mojo::BindingSet<mojom::NetworkHints> bindings_;

  DISALLOW_COPY_AND_ASSIGN(NetworkHintsImpl);
};

}  // namespace network_hints

#endif  // COMPONENTS_NETWORK_HINTS_BROWSER_NETWORK_HINTS_MESSAGE_FILTER_H_
