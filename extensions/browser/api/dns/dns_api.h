// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DNS_DNS_API_H_
#define EXTENSIONS_BROWSER_API_DNS_DNS_API_H_

#include <string>

#include "extensions/browser/extension_function.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/address_list.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace extensions {

class DnsResolveFunction : public UIThreadExtensionFunction,
                           public network::ResolveHostClientBase {
 public:
  DECLARE_EXTENSION_FUNCTION("dns.resolve", DNS_RESOLVE)

  DnsResolveFunction();

 protected:
  ~DnsResolveFunction() override;

  // UIThreadExtensionFunction:
  ResponseAction Run() override;

 private:
  // network::mojom::ResolveHostClient implementation:
  void OnComplete(
      int result,
      const base::Optional<net::AddressList>& resolved_addresses) override;

  // A reference to |this| must be taken while the request is being made on this
  // binding so the object is alive when the request completes.
  mojo::Binding<network::mojom::ResolveHostClient> binding_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DNS_DNS_API_H_
