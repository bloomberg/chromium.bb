// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/network_service_url_loader_factory_impl.h"

#include "base/logging.h"
#include "content/common/resource_request.h"
#include "content/network/url_loader_impl.h"

namespace content {

NetworkServiceURLLoaderFactoryImpl::NetworkServiceURLLoaderFactoryImpl(
    NetworkContext* context)
    : context_(context) {}

NetworkServiceURLLoaderFactoryImpl::~NetworkServiceURLLoaderFactoryImpl() =
    default;

void NetworkServiceURLLoaderFactoryImpl::CreateLoaderAndStart(
    mojom::URLLoaderAssociatedRequest request,
    int32_t routing_id,
    int32_t request_id,
    const ResourceRequest& url_request,
    mojom::URLLoaderClientPtr client) {
  new URLLoaderImpl(context_, std::move(request), url_request,
                    std::move(client));
}

void NetworkServiceURLLoaderFactoryImpl::SyncLoad(
    int32_t routing_id,
    int32_t request_id,
    const ResourceRequest& url_request,
    const SyncLoadCallback& callback) {
  NOTIMPLEMENTED();

  SyncLoadResult result;
  result.error_code = net::ERR_NOT_IMPLEMENTED;
  callback.Run(result);
}

}  // namespace content
