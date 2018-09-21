// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/url_loader_factory_bundle.h"

#include <utility>

#include "base/logging.h"
#include "url/gurl.h"

namespace content {

URLLoaderFactoryBundleInfo::URLLoaderFactoryBundleInfo() = default;

URLLoaderFactoryBundleInfo::URLLoaderFactoryBundleInfo(
    network::mojom::URLLoaderFactoryPtrInfo default_factory_info,
    SchemeMap scheme_specific_factory_infos,
    bool bypass_redirect_checks)
    : default_factory_info_(std::move(default_factory_info)),
      scheme_specific_factory_infos_(std::move(scheme_specific_factory_infos)),
      bypass_redirect_checks_(bypass_redirect_checks) {}

URLLoaderFactoryBundleInfo::~URLLoaderFactoryBundleInfo() = default;

scoped_refptr<network::SharedURLLoaderFactory>
URLLoaderFactoryBundleInfo::CreateFactory() {
  auto other = std::make_unique<URLLoaderFactoryBundleInfo>();
  other->default_factory_info_ = std::move(default_factory_info_);
  other->scheme_specific_factory_infos_ =
      std::move(scheme_specific_factory_infos_);
  other->bypass_redirect_checks_ = bypass_redirect_checks_;

  return base::MakeRefCounted<URLLoaderFactoryBundle>(std::move(other));
}

// -----------------------------------------------------------------------------

URLLoaderFactoryBundle::URLLoaderFactoryBundle() = default;

URLLoaderFactoryBundle::URLLoaderFactoryBundle(
    std::unique_ptr<URLLoaderFactoryBundleInfo> info) {
  Update(std::move(info));
}

URLLoaderFactoryBundle::~URLLoaderFactoryBundle() = default;

void URLLoaderFactoryBundle::SetDefaultFactory(
    network::mojom::URLLoaderFactoryPtr factory) {
  default_factory_ = std::move(factory);
}

network::mojom::URLLoaderFactory* URLLoaderFactoryBundle::GetFactoryForURL(
    const GURL& url) {
  auto it = scheme_specific_factories_.find(url.scheme());
  if (it != scheme_specific_factories_.end())
    return it->second.get();

  return default_factory_.get();
}

void URLLoaderFactoryBundle::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  network::mojom::URLLoaderFactory* factory_ptr = GetFactoryForURL(request.url);

  factory_ptr->CreateLoaderAndStart(std::move(loader), routing_id, request_id,
                                    options, request, std::move(client),
                                    traffic_annotation);
}

void URLLoaderFactoryBundle::Clone(
    network::mojom::URLLoaderFactoryRequest request) {
  NOTREACHED();
}

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
URLLoaderFactoryBundle::Clone() {
  network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
  if (default_factory_)
    default_factory_->Clone(mojo::MakeRequest(&default_factory_info));

  URLLoaderFactoryBundleInfo::SchemeMap scheme_specific_factory_infos;
  for (auto& factory : scheme_specific_factories_) {
    network::mojom::URLLoaderFactoryPtrInfo factory_info;
    factory.second->Clone(mojo::MakeRequest(&factory_info));
    scheme_specific_factory_infos.emplace(factory.first,
                                          std::move(factory_info));
  }

  return std::make_unique<URLLoaderFactoryBundleInfo>(
      std::move(default_factory_info), std::move(scheme_specific_factory_infos),
      bypass_redirect_checks_);
}

bool URLLoaderFactoryBundle::BypassRedirectChecks() const {
  return bypass_redirect_checks_;
}

void URLLoaderFactoryBundle::Update(
    std::unique_ptr<URLLoaderFactoryBundleInfo> info) {
  if (info->default_factory_info())
    default_factory_.Bind(std::move(info->default_factory_info()));
  for (auto& factory_info : info->scheme_specific_factory_infos())
    scheme_specific_factories_[factory_info.first].Bind(
        std::move(factory_info.second));
  bypass_redirect_checks_ = info->bypass_redirect_checks();
}

}  // namespace content
