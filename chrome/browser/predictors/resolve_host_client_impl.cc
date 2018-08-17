// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resolve_host_client_impl.h"

#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace predictors {

ResolveHostClientImpl::ResolveHostClientImpl(
    const GURL& url,
    ResolveHostCallback callback,
    network::mojom::NetworkContext* network_context)
    : binding_(this), callback_(std::move(callback)) {
  network::mojom::ResolveHostClientPtr resolve_host_client_ptr;
  binding_.Bind(mojo::MakeRequest(&resolve_host_client_ptr));
  network_context->ResolveHost(net::HostPortPair::FromURL(url), nullptr,
                               std::move(resolve_host_client_ptr));
  binding_.set_connection_error_handler(base::BindOnce(
      &ResolveHostClientImpl::OnConnectionError, base::Unretained(this)));
}

ResolveHostClientImpl::~ResolveHostClientImpl() = default;

void ResolveHostClientImpl::OnComplete(
    int result,
    const base::Optional<net::AddressList>& resolved_addresses) {
  std::move(callback_).Run(result == net::OK);
}

void ResolveHostClientImpl::OnConnectionError() {
  std::move(callback_).Run(false);
}

}  // namespace predictors
