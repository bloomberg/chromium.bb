// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/network_context.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/network/cache_url_loader.h"
#include "content/network/network_service_url_loader_factory_impl.h"
#include "content/network/url_loader_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_network_session.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace content {

namespace {

std::unique_ptr<net::URLRequestContext> MakeURLRequestContext() {
  net::URLRequestContextBuilder builder;
  net::HttpNetworkSession::Params params;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kIgnoreCertificateErrors))
    params.ignore_certificate_errors = true;

  if (command_line->HasSwitch(switches::kTestingFixedHttpPort)) {
    int value;
    base::StringToInt(
        command_line->GetSwitchValueASCII(switches::kTestingFixedHttpPort),
        &value);
    params.testing_fixed_http_port = value;
  }
  if (command_line->HasSwitch(switches::kTestingFixedHttpsPort)) {
    int value;
    base::StringToInt(
        command_line->GetSwitchValueASCII(switches::kTestingFixedHttpsPort),
        &value);
    params.testing_fixed_https_port = value;
  }
  builder.set_http_network_session_params(params);
  if (command_line->HasSwitch(switches::kHostResolverRules)) {
    std::unique_ptr<net::HostResolver> host_resolver(
        net::HostResolver::CreateDefaultResolver(nullptr));
    std::unique_ptr<net::MappedHostResolver> remapped_host_resolver(
        new net::MappedHostResolver(std::move(host_resolver)));
    remapped_host_resolver->SetRulesFromString(
        command_line->GetSwitchValueASCII(switches::kHostResolverRules));
    builder.set_host_resolver(std::move(remapped_host_resolver));
  }
  builder.set_accept_language("en-us,en");
  builder.set_user_agent(GetContentClient()->GetUserAgent());
  net::URLRequestContextBuilder::HttpCacheParams cache_params;

  // We store the cache in memory so we can run many shells in parallel when
  // running tests, otherwise the network services in each shell will corrupt
  // the disk cache.
  cache_params.type = net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;

  builder.EnableHttpCache(cache_params);
  builder.set_file_enabled(true);
  builder.set_data_enabled(true);

  if (command_line->HasSwitch(switches::kProxyServer)) {
    net::ProxyConfig config;
    config.proxy_rules().ParseFromString(
        command_line->GetSwitchValueASCII(switches::kProxyServer));
    std::unique_ptr<net::ProxyConfigService> fixed_config_service =
        base::MakeUnique<net::ProxyConfigServiceFixed>(config);
    builder.set_proxy_config_service(std::move(fixed_config_service));
  } else {
    builder.set_proxy_service(net::ProxyService::CreateDirect());
  }

  return builder.Build();
}

}  // namespace

NetworkContext::NetworkContext(mojom::NetworkContextRequest request,
                               mojom::NetworkContextParamsPtr params)
    : url_request_context_(MakeURLRequestContext()),
      in_shutdown_(false),
      params_(std::move(params)),
      binding_(this, std::move(request)) {}

NetworkContext::~NetworkContext() {
  in_shutdown_ = true;
  // Call each URLLoaderImpl and ask it to release its net::URLRequest, as the
  // corresponding net::URLRequestContext is going away with this
  // NetworkContext. The loaders can be deregistering themselves in Cleanup(),
  // so iterate over a copy.
  for (auto* url_loader : url_loaders_)
    url_loader->Cleanup();
}

std::unique_ptr<NetworkContext> NetworkContext::CreateForTesting() {
  return base::WrapUnique(new NetworkContext);
}

void NetworkContext::RegisterURLLoader(URLLoaderImpl* url_loader) {
  DCHECK(url_loaders_.count(url_loader) == 0);
  url_loaders_.insert(url_loader);
}

void NetworkContext::DeregisterURLLoader(URLLoaderImpl* url_loader) {
  if (!in_shutdown_) {
    size_t removed_count = url_loaders_.erase(url_loader);
    DCHECK(removed_count);
  }
}

void NetworkContext::CreateURLLoaderFactory(
    mojom::URLLoaderFactoryRequest request,
    uint32_t process_id) {
  loader_factory_bindings_.AddBinding(
      base::MakeUnique<NetworkServiceURLLoaderFactoryImpl>(this, process_id),
      std::move(request));
}

void NetworkContext::HandleViewCacheRequest(const GURL& url,
                                            mojom::URLLoaderClientPtr client) {
  StartCacheURLLoader(url, url_request_context_.get(), std::move(client));
}

NetworkContext::NetworkContext()
    : url_request_context_(MakeURLRequestContext()),
      in_shutdown_(false),
      binding_(this) {}

}  // namespace content
