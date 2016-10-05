// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/url_loader_factory_impl.h"

#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/common/resource_request.h"
#include "content/common/url_loader.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

URLLoaderFactoryImpl::URLLoaderFactoryImpl(
    scoped_refptr<ResourceMessageFilter> resource_message_filter)
    : resource_message_filter_(std::move(resource_message_filter)) {
  DCHECK(resource_message_filter_);
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

URLLoaderFactoryImpl::~URLLoaderFactoryImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void URLLoaderFactoryImpl::CreateLoaderAndStart(
    mojom::URLLoaderRequest request,
    int32_t routing_id,
    int32_t request_id,
    const ResourceRequest& url_request,
    mojom::URLLoaderClientPtr client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
  rdh->OnRequestResourceWithMojo(routing_id, request_id, url_request,
                                 std::move(request), std::move(client),
                                 resource_message_filter_.get());
}

void URLLoaderFactoryImpl::Create(
    scoped_refptr<ResourceMessageFilter> filter,
    mojo::InterfaceRequest<mojom::URLLoaderFactory> request) {
  mojo::MakeStrongBinding(
      base::WrapUnique(new URLLoaderFactoryImpl(std::move(filter))),
      std::move(request));
}

}  // namespace content
