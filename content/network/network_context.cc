// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/network_context.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "content/network/url_loader_impl.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/log/net_log_util.h"
#include "net/log/write_to_file_net_log_observer.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace content {

namespace {

std::unique_ptr<net::URLRequestContext> MakeURLRequestContext() {
  net::URLRequestContextBuilder builder;
  net::URLRequestContextBuilder::HttpNetworkSessionParams params;
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

class NetworkContext::MojoNetLog : public net::NetLog {
 public:
  MojoNetLog() {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(switches::kLogNetLog))
      return;
    base::FilePath log_path =
        command_line->GetSwitchValuePath(switches::kLogNetLog);
    base::ScopedFILE file;
#if defined(OS_WIN)
    file.reset(_wfopen(log_path.value().c_str(), L"w"));
#elif defined(OS_POSIX)
    file.reset(fopen(log_path.value().c_str(), "w"));
#endif
    if (!file) {
      LOG(ERROR) << "Could not open file " << log_path.value()
                 << " for net logging";
    } else {
      write_to_file_observer_.reset(new net::WriteToFileNetLogObserver());
      write_to_file_observer_->set_capture_mode(
          net::NetLogCaptureMode::IncludeCookiesAndCredentials());
      write_to_file_observer_->StartObserving(this, std::move(file), nullptr,
                                              nullptr);
    }
  }
  ~MojoNetLog() override {
    if (write_to_file_observer_)
      write_to_file_observer_->StopObserving(nullptr);
  }

 private:
  std::unique_ptr<net::WriteToFileNetLogObserver> write_to_file_observer_;
  DISALLOW_COPY_AND_ASSIGN(MojoNetLog);
};

NetworkContext::NetworkContext()
    : net_log_(new MojoNetLog),
      url_request_context_(MakeURLRequestContext()),
      in_shutdown_(false) {}

NetworkContext::~NetworkContext() {
  in_shutdown_ = true;
  // Call each URLLoaderImpl and ask it to release its net::URLRequest, as the
  // corresponding net::URLRequestContext is going away with this
  // NetworkContext. The loaders can be deregistering themselves in Cleanup(),
  // so iterate over a copy.
  for (auto* url_loader : url_loaders_)
    url_loader->Cleanup();
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

}  // namespace content
