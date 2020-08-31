// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/child_url_loader_factory_bundle.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

class URLLoaderRelay : public network::mojom::URLLoaderClient,
                       public network::mojom::URLLoader {
 public:
  URLLoaderRelay(
      mojo::PendingRemote<network::mojom::URLLoader> loader_sink,
      mojo::PendingReceiver<network::mojom::URLLoaderClient> client_source,
      mojo::Remote<network::mojom::URLLoaderClient> client_sink)
      : loader_sink_(std::move(loader_sink)),
        client_source_receiver_(this, std::move(client_source)),
        client_sink_(std::move(client_sink)) {}

  // network::mojom::URLLoader implementation:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_request_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_request_headers,
      const base::Optional<GURL>& new_url) override {
    DCHECK(removed_headers.empty() && modified_request_headers.IsEmpty() &&
           modified_cors_exempt_request_headers.IsEmpty())
        << "Redirect with removed or modified headers was not supported yet. "
           "crbug.com/845683";
    DCHECK(!new_url.has_value())
        << "Redirect with modified URL was not supported yet. "
           "crbug.com/845683";
    loader_sink_->FollowRedirect(
        {} /* removed_headers */, {} /* modified_headers */,
        {} /* modified_cors_exempt_headers */, base::nullopt /* new_url */);
  }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    loader_sink_->SetPriority(priority, intra_priority_value);
  }

  void PauseReadingBodyFromNet() override {
    loader_sink_->PauseReadingBodyFromNet();
  }

  void ResumeReadingBodyFromNet() override {
    loader_sink_->ResumeReadingBodyFromNet();
  }

  // network::mojom::URLLoaderClient implementation:
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override {
    client_sink_->OnReceiveResponse(std::move(head));
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {
    client_sink_->OnReceiveRedirect(redirect_info, std::move(head));
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {
    client_sink_->OnUploadProgress(current_position, total_size,
                                   std::move(callback));
  }

  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override {
    client_sink_->OnReceiveCachedMetadata(std::move(data));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    client_sink_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    client_sink_->OnStartLoadingResponseBody(std::move(body));
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    client_sink_->OnComplete(status);
  }

 private:
  mojo::Remote<network::mojom::URLLoader> loader_sink_;
  mojo::Receiver<network::mojom::URLLoaderClient> client_source_receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> client_sink_;
};

template <typename TKey>
static std::map<TKey, mojo::PendingRemote<network::mojom::URLLoaderFactory>>
BoundRemoteMapToPendingRemoteMap(
    std::map<TKey, mojo::Remote<network::mojom::URLLoaderFactory>> input) {
  std::map<TKey, mojo::PendingRemote<network::mojom::URLLoaderFactory>> output;
  for (auto& it : input) {
    const TKey& key = it.first;
    mojo::Remote<network::mojom::URLLoaderFactory>& factory = it.second;
    if (factory.is_bound())
      output.emplace(key, factory.Unbind());
  }
  return output;
}

}  // namespace

ChildPendingURLLoaderFactoryBundle::ChildPendingURLLoaderFactoryBundle() =
    default;

ChildPendingURLLoaderFactoryBundle::ChildPendingURLLoaderFactoryBundle(
    std::unique_ptr<PendingURLLoaderFactoryBundle> base_factories)
    : PendingURLLoaderFactoryBundle(
          std::move(base_factories->pending_default_factory()),
          std::move(base_factories->pending_scheme_specific_factories()),
          std::move(base_factories->pending_isolated_world_factories()),
          base_factories->bypass_redirect_checks()) {
  pending_appcache_factory_ =
      std::move(base_factories->pending_appcache_factory());
}

ChildPendingURLLoaderFactoryBundle::ChildPendingURLLoaderFactoryBundle(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_default_factory,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_appcache_factory,
    SchemeMap pending_scheme_specific_factories,
    OriginMap pending_isolated_world_factories,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        direct_network_factory_remote,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_prefetch_loader_factory,
    bool bypass_redirect_checks)
    : PendingURLLoaderFactoryBundle(
          std::move(pending_default_factory),
          std::move(pending_scheme_specific_factories),
          std::move(pending_isolated_world_factories),
          bypass_redirect_checks),
      direct_network_factory_remote_(std::move(direct_network_factory_remote)),
      pending_prefetch_loader_factory_(
          std::move(pending_prefetch_loader_factory)) {
  pending_appcache_factory_ = std::move(pending_appcache_factory);
}

ChildPendingURLLoaderFactoryBundle::~ChildPendingURLLoaderFactoryBundle() =
    default;

scoped_refptr<network::SharedURLLoaderFactory>
ChildPendingURLLoaderFactoryBundle::CreateFactory() {
  auto other = std::make_unique<ChildPendingURLLoaderFactoryBundle>();
  other->pending_default_factory_ = std::move(pending_default_factory_);
  other->pending_appcache_factory_ = std::move(pending_appcache_factory_);
  other->pending_scheme_specific_factories_ =
      std::move(pending_scheme_specific_factories_);
  other->pending_isolated_world_factories_ =
      std::move(pending_isolated_world_factories_);
  other->direct_network_factory_remote_ =
      std::move(direct_network_factory_remote_);
  other->pending_prefetch_loader_factory_ =
      std::move(pending_prefetch_loader_factory_);
  other->bypass_redirect_checks_ = bypass_redirect_checks_;

  return base::MakeRefCounted<ChildURLLoaderFactoryBundle>(std::move(other));
}

// -----------------------------------------------------------------------------

ChildURLLoaderFactoryBundle::ChildURLLoaderFactoryBundle() = default;

ChildURLLoaderFactoryBundle::ChildURLLoaderFactoryBundle(
    std::unique_ptr<ChildPendingURLLoaderFactoryBundle> pending_factories) {
  Update(std::move(pending_factories));
}

ChildURLLoaderFactoryBundle::ChildURLLoaderFactoryBundle(
    FactoryGetterCallback direct_network_factory_getter)
    : direct_network_factory_getter_(std::move(direct_network_factory_getter)) {
}

ChildURLLoaderFactoryBundle::~ChildURLLoaderFactoryBundle() = default;

network::mojom::URLLoaderFactory* ChildURLLoaderFactoryBundle::GetFactory(
    const network::ResourceRequest& request) {
  network::mojom::URLLoaderFactory* base_result =
      URLLoaderFactoryBundle::GetFactory(request);
  if (base_result)
    return base_result;

  InitDirectNetworkFactoryIfNecessary();
  DCHECK(direct_network_factory_);
  return direct_network_factory_.get();
}

void ChildURLLoaderFactoryBundle::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  auto override_iter = subresource_overrides_.find(request.url);
  if (override_iter != subresource_overrides_.end()) {
    mojom::TransferrableURLLoaderPtr transferrable_loader =
        std::move(override_iter->second);
    subresource_overrides_.erase(override_iter);

    mojo::Remote<network::mojom::URLLoaderClient> client_remote(
        std::move(client));
    client_remote->OnReceiveResponse(std::move(transferrable_loader->head));
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<URLLoaderRelay>(
            std::move(transferrable_loader->url_loader),
            std::move(transferrable_loader->url_loader_client),
            std::move(client_remote)),
        std::move(loader));

    return;
  }

  // Use |prefetch_loader_factory_| for prefetch requests to send the requests
  // to the PrefetchURLLoaderService in the browser process and triger the
  // special prefetch handling.
  // TODO(horo): Move this routing logic to network service, when we will have
  // the special prefetch handling in network service.
  if ((request.resource_type ==
       static_cast<int>(blink::mojom::ResourceType::kPrefetch)) &&
      prefetch_loader_factory_) {
    prefetch_loader_factory_->CreateLoaderAndStart(
        std::move(loader), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
    return;
  }

  URLLoaderFactoryBundle::CreateLoaderAndStart(
      std::move(loader), routing_id, request_id, options, request,
      std::move(client), traffic_annotation);
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
ChildURLLoaderFactoryBundle::Clone() {
  return CloneInternal(true /* include_appcache */);
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
ChildURLLoaderFactoryBundle::CloneWithoutAppCacheFactory() {
  return CloneInternal(false /* include_appcache */);
}

void ChildURLLoaderFactoryBundle::Update(
    std::unique_ptr<ChildPendingURLLoaderFactoryBundle> info) {
  if (info->direct_network_factory_remote()) {
    direct_network_factory_.Bind(
        std::move(info->direct_network_factory_remote()));
  }
  if (info->pending_prefetch_loader_factory()) {
    prefetch_loader_factory_.Bind(
        std::move(info->pending_prefetch_loader_factory()));
  }
  URLLoaderFactoryBundle::Update(std::move(info));
}

void ChildURLLoaderFactoryBundle::UpdateSubresourceOverrides(
    std::vector<mojom::TransferrableURLLoaderPtr>* subresource_overrides) {
  for (auto& element : *subresource_overrides)
    subresource_overrides_[element->url] = std::move(element);
}

void ChildURLLoaderFactoryBundle::SetPrefetchLoaderFactory(
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        prefetch_loader_factory) {
  prefetch_loader_factory_.Bind(std::move(prefetch_loader_factory));
}

bool ChildURLLoaderFactoryBundle::IsHostChildURLLoaderFactoryBundle() const {
  return false;
}

void ChildURLLoaderFactoryBundle::InitDirectNetworkFactoryIfNecessary() {
  if (direct_network_factory_getter_.is_null())
    return;

  if (!direct_network_factory_) {
    direct_network_factory_.Bind(
        std::move(direct_network_factory_getter_).Run());
  } else {
    direct_network_factory_getter_.Reset();
  }
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
ChildURLLoaderFactoryBundle::CloneInternal(bool include_appcache) {
  InitDirectNetworkFactoryIfNecessary();

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      default_factory_pending_remote;
  if (default_factory_) {
    default_factory_->Clone(
        default_factory_pending_remote.InitWithNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      appcache_factory_pending_remote;
  if (appcache_factory_ && include_appcache) {
    appcache_factory_->Clone(
        appcache_factory_pending_remote.InitWithNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      direct_network_factory_remote;
  if (direct_network_factory_) {
    direct_network_factory_->Clone(
        direct_network_factory_remote.InitWithNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_prefetch_loader_factory;
  if (prefetch_loader_factory_) {
    prefetch_loader_factory_->Clone(
        pending_prefetch_loader_factory.InitWithNewPipeAndPassReceiver());
  }

  // Currently there is no need to override subresources from workers,
  // therefore |subresource_overrides| are not shared with the clones.

  return std::make_unique<ChildPendingURLLoaderFactoryBundle>(
      std::move(default_factory_pending_remote),
      std::move(appcache_factory_pending_remote),
      CloneRemoteMapToPendingRemoteMap(scheme_specific_factories_),
      CloneRemoteMapToPendingRemoteMap(isolated_world_factories_),
      std::move(direct_network_factory_remote),
      std::move(pending_prefetch_loader_factory), bypass_redirect_checks_);
}

std::unique_ptr<ChildPendingURLLoaderFactoryBundle>
ChildURLLoaderFactoryBundle::PassInterface() {
  InitDirectNetworkFactoryIfNecessary();

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  if (default_factory_)
    pending_default_factory = default_factory_.Unbind();

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_appcache_factory;
  if (appcache_factory_)
    pending_appcache_factory = appcache_factory_.Unbind();

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      direct_network_factory_remote;
  if (direct_network_factory_) {
    direct_network_factory_remote = direct_network_factory_.Unbind();
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_prefetch_loader_factory;
  if (prefetch_loader_factory_) {
    pending_prefetch_loader_factory = prefetch_loader_factory_.Unbind();
  }

  return std::make_unique<ChildPendingURLLoaderFactoryBundle>(
      std::move(pending_default_factory), std::move(pending_appcache_factory),
      BoundRemoteMapToPendingRemoteMap(std::move(scheme_specific_factories_)),
      BoundRemoteMapToPendingRemoteMap(std::move(isolated_world_factories_)),
      std::move(direct_network_factory_remote),
      std::move(pending_prefetch_loader_factory), bypass_redirect_checks_);
}

}  // namespace content
