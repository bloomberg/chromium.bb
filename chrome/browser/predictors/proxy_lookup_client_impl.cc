// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/proxy_lookup_client_impl.h"

#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace predictors {

ProxyLookupClientImpl::ProxyLookupClientImpl(
    const GURL& url,
    ProxyLookupCallback callback,
    network::mojom::NetworkContext* network_context)
    : binding_(this), callback_(std::move(callback)) {
  network::mojom::ProxyLookupClientPtr proxy_lookup_client_ptr;
  binding_.Bind(mojo::MakeRequest(&proxy_lookup_client_ptr));
  network_context->LookUpProxyForURL(url, std::move(proxy_lookup_client_ptr));
  binding_.set_connection_error_handler(
      base::BindOnce(&ProxyLookupClientImpl::OnProxyLookupComplete,
                     base::Unretained(this), base::nullopt));
}

ProxyLookupClientImpl::~ProxyLookupClientImpl() = default;

void ProxyLookupClientImpl::OnProxyLookupComplete(
    const base::Optional<net::ProxyInfo>& proxy_info) {
  bool success = proxy_info.has_value() && !proxy_info->is_direct();
  std::move(callback_).Run(success);
}

}  // namespace predictors
