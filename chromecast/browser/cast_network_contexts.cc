// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_network_contexts.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chromecast/browser/url_request_context_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/cross_thread_shared_url_loader_factory_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromecast {
namespace shell {

// SharedURLLoaderFactory backed by a CastNetworkContexts and its system
// NetworkContext. Transparently handles crashes.
class CastNetworkContexts::URLLoaderFactoryForSystem
    : public network::SharedURLLoaderFactory {
 public:
  explicit URLLoaderFactoryForSystem(CastNetworkContexts* network_context)
      : network_context_(network_context) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!network_context_)
      return;
    network_context_->GetSystemURLLoaderFactory()->CreateLoaderAndStart(
        std::move(request), routing_id, request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    if (!network_context_)
      return;
    network_context_->GetSystemURLLoaderFactory()->Clone(std::move(request));
  }

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return std::make_unique<network::CrossThreadSharedURLLoaderFactoryInfo>(
        this);
  }

  void Shutdown() { network_context_ = nullptr; }

 private:
  friend class base::RefCounted<URLLoaderFactoryForSystem>;
  ~URLLoaderFactoryForSystem() override {}

  SEQUENCE_CHECKER(sequence_checker_);
  CastNetworkContexts* network_context_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryForSystem);
};

// Class to own the NetworkContext wrapping the system URLRequestContext when
// the network service is disabled.
//
// Created on the UI thread, but must be initialized and destroyed on the IO
// thread.
class CastNetworkContexts::SystemNetworkContextOwner {
 public:
  SystemNetworkContextOwner() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  ~SystemNetworkContextOwner() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  }

  void Initialize(network::mojom::NetworkContextRequest network_context_request,
                  scoped_refptr<net::URLRequestContextGetter> context_getter) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    context_getter_ = std::move(context_getter);
    network_context_ = std::make_unique<network::NetworkContext>(
        content::GetNetworkServiceImpl(), std::move(network_context_request),
        context_getter_->GetURLRequestContext());
  }

 private:
  // Reference to the URLRequestContextGetter for the URLRequestContext used by
  // NetworkContext. Depending on the embedder's implementation, this may be
  // needed to keep the URLRequestContext alive until the NetworkContext is
  // destroyed.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  std::unique_ptr<network::mojom::NetworkContext> network_context_;

  DISALLOW_COPY_AND_ASSIGN(SystemNetworkContextOwner);
};

CastNetworkContexts::CastNetworkContexts(
    URLRequestContextFactory* url_request_context_factory)
    : url_request_context_factory_(url_request_context_factory) {
  system_shared_url_loader_factory_ =
      base::MakeRefCounted<URLLoaderFactoryForSystem>(this);
}

CastNetworkContexts::~CastNetworkContexts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  system_shared_url_loader_factory_->Shutdown();
}

network::mojom::NetworkContext* CastNetworkContexts::GetSystemContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    if (!system_network_context_) {
      system_network_context_owner_.reset(new SystemNetworkContextOwner);
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::IO},
          base::BindOnce(&SystemNetworkContextOwner::Initialize,
                         base::Unretained(system_network_context_owner_.get()),
                         MakeRequest(&system_network_context_),
                         base::WrapRefCounted(
                             url_request_context_factory_->GetSystemGetter())));
    }
    return system_network_context_.get();
  }

  if (!system_network_context_ || system_network_context_.encountered_error()) {
    // This should call into OnNetworkServiceCreated(), which will re-create
    // the network service, if needed. There's a chance that it won't be
    // invoked, if the NetworkContext has encountered an error but the
    // NetworkService has not yet noticed its pipe was closed. In that case,
    // trying to create a new NetworkContext would fail, anyways, and hopefully
    // a new NetworkContext will be created on the next GetContext() call.
    content::GetNetworkService();
    DCHECK(system_network_context_);
  }

  return system_network_context_.get();
}

network::mojom::URLLoaderFactory*
CastNetworkContexts::GetSystemURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Create the URLLoaderFactory as needed.
  if (system_url_loader_factory_ &&
      !system_url_loader_factory_.encountered_error()) {
    return system_url_loader_factory_.get();
  }

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  GetSystemContext()->CreateURLLoaderFactory(
      mojo::MakeRequest(&system_url_loader_factory_), std::move(params));
  return system_shared_url_loader_factory_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
CastNetworkContexts::GetSystemSharedURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return system_shared_url_loader_factory_;
}

network::mojom::NetworkContextPtr CastNetworkContexts::CreateNetworkContext(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));

  network::mojom::NetworkContextPtr network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      CreateDefaultNetworkContextParams();

  // Copy of what's in ContentBrowserClient::CreateNetworkContext for now.
  context_params->accept_language = "en-us,en";
  context_params->enable_data_url_support = true;

  content::GetNetworkService()->CreateNetworkContext(
      MakeRequest(&network_context), std::move(context_params));
  return network_context;
}

void CastNetworkContexts::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  // The system NetworkContext must be created first, since it sets
  // |primary_network_context| to true.
  network_service->CreateNetworkContext(MakeRequest(&system_network_context_),
                                        CreateSystemNetworkContextParams());
}

network::mojom::NetworkContextParamsPtr
CastNetworkContexts::CreateDefaultNetworkContextParams() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();

  return network_context_params;
}

network::mojom::NetworkContextParamsPtr
CastNetworkContexts::CreateSystemNetworkContextParams() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      CreateDefaultNetworkContextParams();

  network_context_params->context_name = std::string("system");

  network_context_params->primary_network_context = true;

  return network_context_params;
}

}  // namespace shell
}  // namespace chromecast
