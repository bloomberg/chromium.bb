// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/url_loader_factory_bundle.h"

#include <map>
#include <string>

#include "base/macros.h"
#include "services/network/public/interfaces/url_loader_factory.mojom.h"
#include "url/gurl.h"

class GURL;

namespace content {

URLLoaderFactoryBundleInfo::URLLoaderFactoryBundleInfo(
    URLLoaderFactoryBundleInfo&&) = default;

URLLoaderFactoryBundleInfo::URLLoaderFactoryBundleInfo(
    network::mojom::URLLoaderFactoryPtrInfo default_factory_info,
    std::map<std::string, network::mojom::URLLoaderFactoryPtrInfo>
        factories_info)
    : default_factory_info(std::move(default_factory_info)),
      factories_info(std::move(factories_info)) {}

URLLoaderFactoryBundleInfo::~URLLoaderFactoryBundleInfo() = default;

URLLoaderFactoryBundle::URLLoaderFactoryBundle() = default;

URLLoaderFactoryBundle::URLLoaderFactoryBundle(URLLoaderFactoryBundle&&) =
    default;

URLLoaderFactoryBundle::URLLoaderFactoryBundle(
    URLLoaderFactoryBundleInfo info) {
  default_factory_.Bind(std::move(info.default_factory_info));
  for (auto& factory_info : info.factories_info)
    factories_[factory_info.first].Bind(std::move(factory_info.second));
}

URLLoaderFactoryBundle::~URLLoaderFactoryBundle() = default;

URLLoaderFactoryBundle& URLLoaderFactoryBundle::operator=(
    URLLoaderFactoryBundle&&) = default;

void URLLoaderFactoryBundle::SetDefaultFactory(
    network::mojom::URLLoaderFactoryPtr factory) {
  default_factory_ = std::move(factory);
}

void URLLoaderFactoryBundle::RegisterFactory(
    const base::StringPiece& scheme,
    network::mojom::URLLoaderFactoryPtr factory) {
  DCHECK(factory.is_bound());
  auto result = factories_.emplace(std::string(scheme), std::move(factory));
  DCHECK(result.second);
}

network::mojom::URLLoaderFactory* URLLoaderFactoryBundle::GetFactoryForRequest(
    const GURL& url) {
  auto it = factories_.find(url.scheme());
  if (it == factories_.end()) {
    DCHECK(default_factory_.is_bound());
    return default_factory_.get();
  }
  return it->second.get();
}

URLLoaderFactoryBundleInfo URLLoaderFactoryBundle::PassInfo() {
  std::map<std::string, network::mojom::URLLoaderFactoryPtrInfo> factories_info;
  for (auto& factory : factories_)
    factories_info.emplace(factory.first, factory.second.PassInterface());
  DCHECK(default_factory_.is_bound());
  return URLLoaderFactoryBundleInfo(default_factory_.PassInterface(),
                                    std::move(factories_info));
}

URLLoaderFactoryBundle URLLoaderFactoryBundle::Clone() {
  DCHECK(default_factory_.is_bound());
  network::mojom::URLLoaderFactoryPtr cloned_default_factory;
  default_factory_->Clone(mojo::MakeRequest(&cloned_default_factory));

  URLLoaderFactoryBundle new_bundle;
  new_bundle.SetDefaultFactory(std::move(cloned_default_factory));
  for (auto& factory : factories_) {
    network::mojom::URLLoaderFactoryPtr cloned_factory;
    factory.second->Clone(mojo::MakeRequest(&cloned_factory));
    new_bundle.RegisterFactory(factory.first, std::move(cloned_factory));
  }

  return new_bundle;
}

}  // namespace content
