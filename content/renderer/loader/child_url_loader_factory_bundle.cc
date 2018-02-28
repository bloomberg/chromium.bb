// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/child_url_loader_factory_bundle.h"

#include "base/logging.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

ChildURLLoaderFactoryBundleInfo::ChildURLLoaderFactoryBundleInfo() = default;

ChildURLLoaderFactoryBundleInfo::ChildURLLoaderFactoryBundleInfo(
    network::mojom::URLLoaderFactoryPtrInfo default_factory_info,
    std::map<std::string, network::mojom::URLLoaderFactoryPtrInfo>
        factories_info,
    PossiblyAssociatedURLLoaderFactoryPtrInfo direct_network_factory_info)
    : URLLoaderFactoryBundleInfo(std::move(default_factory_info),
                                 std::move(factories_info)),
      direct_network_factory_info_(std::move(direct_network_factory_info)) {}

ChildURLLoaderFactoryBundleInfo::~ChildURLLoaderFactoryBundleInfo() = default;

scoped_refptr<SharedURLLoaderFactory>
ChildURLLoaderFactoryBundleInfo::CreateFactory() {
  auto other = std::make_unique<ChildURLLoaderFactoryBundleInfo>();
  other->default_factory_info_ = std::move(default_factory_info_);
  other->factories_info_ = std::move(factories_info_);
  other->direct_network_factory_info_ = std::move(direct_network_factory_info_);

  return base::MakeRefCounted<ChildURLLoaderFactoryBundle>(std::move(other));
}

// -----------------------------------------------------------------------------

ChildURLLoaderFactoryBundle::ChildURLLoaderFactoryBundle() = default;

ChildURLLoaderFactoryBundle::ChildURLLoaderFactoryBundle(
    std::unique_ptr<ChildURLLoaderFactoryBundleInfo> info) {
  Update(std::move(info));
}

ChildURLLoaderFactoryBundle::ChildURLLoaderFactoryBundle(
    PossiblyAssociatedFactoryGetterCallback direct_network_factory_getter,
    FactoryGetterCallback default_blob_factory_getter)
    : direct_network_factory_getter_(std::move(direct_network_factory_getter)),
      default_blob_factory_getter_(std::move(default_blob_factory_getter)) {}

ChildURLLoaderFactoryBundle::~ChildURLLoaderFactoryBundle() = default;

network::mojom::URLLoaderFactory* ChildURLLoaderFactoryBundle::GetFactoryForURL(
    const GURL& url) {
  if (url.SchemeIsBlob())
    InitDefaultBlobFactoryIfNecessary();

  auto it = factories_.find(url.scheme());
  if (it != factories_.end())
    return it->second.get();

  if (default_factory_)
    return default_factory_.get();

  InitDirectNetworkFactoryIfNecessary();
  DCHECK(direct_network_factory_);
  return direct_network_factory_.get();
}

void ChildURLLoaderFactoryBundle::CreateLoaderAndStart(
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

std::unique_ptr<SharedURLLoaderFactoryInfo>
ChildURLLoaderFactoryBundle::Clone() {
  InitDefaultBlobFactoryIfNecessary();
  InitDirectNetworkFactoryIfNecessary();

  network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
  if (default_factory_)
    default_factory_->Clone(mojo::MakeRequest(&default_factory_info));

  std::map<std::string, network::mojom::URLLoaderFactoryPtrInfo> factories_info;
  for (auto& factory : factories_) {
    network::mojom::URLLoaderFactoryPtrInfo factory_info;
    factory.second->Clone(mojo::MakeRequest(&factory_info));
    factories_info.emplace(factory.first, std::move(factory_info));
  }

  network::mojom::URLLoaderFactoryPtrInfo direct_network_factory_info;
  if (direct_network_factory_) {
    direct_network_factory_->Clone(
        mojo::MakeRequest(&direct_network_factory_info));
  }

  return std::make_unique<ChildURLLoaderFactoryBundleInfo>(
      std::move(default_factory_info), std::move(factories_info),
      std::move(direct_network_factory_info));
}

void ChildURLLoaderFactoryBundle::Update(
    std::unique_ptr<ChildURLLoaderFactoryBundleInfo> info) {
  if (info->direct_network_factory_info()) {
    direct_network_factory_.Bind(
        std::move(info->direct_network_factory_info()));
  }
  URLLoaderFactoryBundle::Update(std::move(info));
}

bool ChildURLLoaderFactoryBundle::IsHostChildURLLoaderFactoryBundle() const {
  return false;
}

void ChildURLLoaderFactoryBundle::InitDefaultBlobFactoryIfNecessary() {
  if (default_blob_factory_getter_.is_null())
    return;

  if (factories_.find(url::kBlobScheme) == factories_.end()) {
    network::mojom::URLLoaderFactoryPtr blob_factory =
        std::move(default_blob_factory_getter_).Run();
    if (blob_factory)
      factories_.emplace(url::kBlobScheme, std::move(blob_factory));
  } else {
    default_blob_factory_getter_.Reset();
  }
}

void ChildURLLoaderFactoryBundle::InitDirectNetworkFactoryIfNecessary() {
  if (direct_network_factory_getter_.is_null())
    return;

  if (!direct_network_factory_) {
    direct_network_factory_ = std::move(direct_network_factory_getter_).Run();
  } else {
    direct_network_factory_getter_.Reset();
  }
}

std::unique_ptr<ChildURLLoaderFactoryBundleInfo>
ChildURLLoaderFactoryBundle::PassInterface() {
  InitDefaultBlobFactoryIfNecessary();
  InitDirectNetworkFactoryIfNecessary();

  network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
  if (default_factory_)
    default_factory_info = default_factory_.PassInterface();

  std::map<std::string, network::mojom::URLLoaderFactoryPtrInfo> factories_info;
  for (auto& factory : factories_) {
    factories_info.emplace(factory.first, factory.second.PassInterface());
  }

  PossiblyAssociatedInterfacePtrInfo<network::mojom::URLLoaderFactory>
      direct_network_factory_info;
  if (direct_network_factory_) {
    direct_network_factory_info = direct_network_factory_.PassInterface();
  }

  return std::make_unique<ChildURLLoaderFactoryBundleInfo>(
      std::move(default_factory_info), std::move(factories_info),
      std::move(direct_network_factory_info));
}

}  // namespace content
