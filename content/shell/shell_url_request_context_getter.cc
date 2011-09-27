// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_url_request_context_getter.h"

#include "base/logging.h"
#include "base/string_split.h"
#include "content/browser/browser_thread.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/default_origin_bound_cert_store.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/base/origin_bound_cert_service.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory.h"

namespace content {

ShellURLRequestContextGetter::ShellURLRequestContextGetter(
    const FilePath& base_path_,
    MessageLoop* io_loop,
    MessageLoop* file_loop)
    : io_loop_(io_loop),
      file_loop_(file_loop) {
}

ShellURLRequestContextGetter::~ShellURLRequestContextGetter() {
}

net::URLRequestContext* ShellURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!url_request_context_) {
    FilePath cache_path = base_path_.Append(FILE_PATH_LITERAL("Cache"));
    net::HttpCache::DefaultBackend* main_backend =
        new net::HttpCache::DefaultBackend(
            net::DISK_CACHE,
            cache_path,
            0,
            BrowserThread::GetMessageLoopProxyForThread(
                BrowserThread::CACHE));

    net::NetLog* net_log = NULL;
    host_resolver_.reset(net::CreateSystemHostResolver(
        net::HostResolver::kDefaultParallelism,
        net::HostResolver::kDefaultRetryAttempts,
        net_log));

    cert_verifier_.reset(new net::CertVerifier());

    origin_bound_cert_service_.reset(new net::OriginBoundCertService(
        new net::DefaultOriginBoundCertStore(NULL)));

    dnsrr_resolver_.reset(new net::DnsRRResolver());

    net::ProxyConfigService* proxy_config_service =
        net::ProxyService::CreateSystemProxyConfigService(
            io_loop_,
            file_loop_);

    // TODO(jam): use v8 if possible, look at chrome code.
    proxy_service_.reset(
        net::ProxyService::CreateUsingSystemProxyResolver(
        proxy_config_service,
        0,
        net_log));

    url_security_manager_.reset(net::URLSecurityManager::Create(NULL, NULL));

    std::vector<std::string> supported_schemes;
    base::SplitString("basic,digest,ntlm,negotiate", ',', &supported_schemes);
    http_auth_handler_factory_.reset(
        net::HttpAuthHandlerRegistryFactory::Create(
            supported_schemes,
            url_security_manager_.get(),
            host_resolver_.get(),
            std::string(),  // gssapi_library_name
            false,  // negotiate_disable_cname_lookup
            false)); // negotiate_enable_port

    net::HttpCache* main_cache = new net::HttpCache(
        host_resolver_.get(),
        cert_verifier_.get(),
        origin_bound_cert_service_.get(),
        dnsrr_resolver_.get(),
        NULL, //dns_cert_checker
        proxy_service_.get(),
        new net::SSLConfigServiceDefaults(),
        http_auth_handler_factory_.get(),
        NULL, // network_delegate
        net_log,
        main_backend);
    main_http_factory_.reset(main_cache);

    scoped_refptr<net::CookieStore> cookie_store =
        new net::CookieMonster(NULL, NULL);

    url_request_context_ = new net::URLRequestContext();
    job_factory_.reset(new net::URLRequestJobFactory);
    url_request_context_->set_job_factory(job_factory_.get());
    url_request_context_->set_http_transaction_factory(main_cache);
    url_request_context_->set_origin_bound_cert_service(
        origin_bound_cert_service_.get());
    url_request_context_->set_dnsrr_resolver(dnsrr_resolver_.get());
    url_request_context_->set_proxy_service(proxy_service_.get());
    url_request_context_->set_cookie_store(cookie_store);
  }

  return url_request_context_;
}

net::CookieStore* ShellURLRequestContextGetter::DONTUSEME_GetCookieStore() {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO))
    return GetURLRequestContext()->cookie_store();
  NOTIMPLEMENTED();
  return NULL;
}

scoped_refptr<base::MessageLoopProxy>
    ShellURLRequestContextGetter::GetIOMessageLoopProxy() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

}  // namespace content
