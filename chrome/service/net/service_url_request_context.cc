// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/net/service_url_request_context.h"

#include "base/message_loop_proxy.h"
#include "chrome/service/service_process.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_policy.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/proxy/proxy_service.h"

ServiceURLRequestContext::ServiceURLRequestContext() {
  host_resolver_ =
      net::CreateSystemHostResolver(net::HostResolver::kDefaultParallelism,
                                    NULL, NULL);
  DCHECK(g_service_process);
  // TODO(sanjeevr): Change CreateSystemProxyConfigService to accept a
  // MessageLoopProxy* instead of MessageLoop*.
  // Also this needs to be created on the UI thread on Linux. Fix this.
  net::ProxyConfigService * proxy_config_service =
      net::ProxyService::CreateSystemProxyConfigService(
          g_service_process->io_thread()->message_loop(),
          g_service_process->file_thread()->message_loop());
  proxy_service_ = net::ProxyService::CreateUsingSystemProxyResolver(
      proxy_config_service, 0u, NULL);
  dnsrr_resolver_ = new net::DnsRRResolver;
  ftp_transaction_factory_ = new net::FtpNetworkLayer(host_resolver_);
  ssl_config_service_ = new net::SSLConfigServiceDefaults;
  http_auth_handler_factory_ = net::HttpAuthHandlerFactory::CreateDefault(
      host_resolver_);
  http_transaction_factory_ = new net::HttpCache(
      net::HttpNetworkLayer::CreateFactory(host_resolver_,
                                           dnsrr_resolver_,
                                           NULL /* ssl_host_info_factory */,
                                           proxy_service_,
                                           ssl_config_service_,
                                           http_auth_handler_factory_,
                                           NULL /* network_delegate */,
                                           NULL /* net_log */),
      net::HttpCache::DefaultBackend::InMemory(0));
  // In-memory cookie store.
  cookie_store_ = new net::CookieMonster(NULL, NULL);
  accept_language_ = "en-us,fr";
  accept_charset_ = "iso-8859-1,*,utf-8";
}

ServiceURLRequestContext::~ServiceURLRequestContext() {
  delete ftp_transaction_factory_;
  delete http_transaction_factory_;
  delete http_auth_handler_factory_;
  delete dnsrr_resolver_;
}

ServiceURLRequestContextGetter::ServiceURLRequestContextGetter()
    : io_message_loop_proxy_(
          g_service_process->io_thread()->message_loop_proxy()) {
}

URLRequestContext*
ServiceURLRequestContextGetter::GetURLRequestContext() {
  if (!url_request_context_)
    url_request_context_ = new ServiceURLRequestContext();
  return url_request_context_;
}

scoped_refptr<base::MessageLoopProxy>
ServiceURLRequestContextGetter::GetIOMessageLoopProxy() const {
  return io_message_loop_proxy_;
}

ServiceURLRequestContextGetter::~ServiceURLRequestContextGetter() {}
