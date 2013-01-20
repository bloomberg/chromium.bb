// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_url_request_context_getter.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/shell/shell_network_delegate.h"
#include "content/shell/shell_switches.h"
#include "net/base/cert_verifier.h"
#include "net/base/default_server_bound_cert_store.h"
#include "net/base/host_resolver.h"
#include "net/base/mapped_host_resolver.h"
#include "net/base/server_bound_cert_service.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/cookies/cookie_monster.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace content {

ShellURLRequestContextGetter::ShellURLRequestContextGetter(
    bool ignore_certificate_errors,
    const FilePath& base_path,
    MessageLoop* io_loop,
    MessageLoop* file_loop)
    : ignore_certificate_errors_(ignore_certificate_errors),
      base_path_(base_path),
      io_loop_(io_loop),
      file_loop_(file_loop) {
  // Must first be created on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We must create the proxy config service on the UI loop on Linux because it
  // must synchronously run on the glib message loop. This will be passed to
  // the URLRequestContextStorage on the IO thread in GetURLRequestContext().
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    proxy_config_service_.reset(
        net::ProxyService::CreateSystemProxyConfigService(
            io_loop_->message_loop_proxy(), file_loop_));
  }
}

ShellURLRequestContextGetter::~ShellURLRequestContextGetter() {
}

net::URLRequestContext* ShellURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!url_request_context_.get()) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();

    url_request_context_.reset(new net::URLRequestContext());
    network_delegate_.reset(new ShellNetworkDelegate);
    url_request_context_->set_network_delegate(network_delegate_.get());
    storage_.reset(
        new net::URLRequestContextStorage(url_request_context_.get()));
    storage_->set_cookie_store(new net::CookieMonster(NULL, NULL));
    storage_->set_server_bound_cert_service(new net::ServerBoundCertService(
        new net::DefaultServerBoundCertStore(NULL),
        base::WorkerPool::GetTaskRunner(true)));
    storage_->set_http_user_agent_settings(
        new net::StaticHttpUserAgentSettings(
            "en-us,en", "iso-8859-1,*,utf-8", EmptyString()));

    scoped_ptr<net::HostResolver> host_resolver(
        net::HostResolver::CreateDefaultResolver(NULL));

    storage_->set_cert_verifier(net::CertVerifier::CreateDefault());
    if (command_line.HasSwitch(switches::kDumpRenderTree)) {
      storage_->set_proxy_service(net::ProxyService::CreateDirect());
    } else {
      // TODO(jam): use v8 if possible, look at chrome code.
      storage_->set_proxy_service(
          net::ProxyService::CreateUsingSystemProxyResolver(
          proxy_config_service_.release(),
          0,
          NULL));
    }
    storage_->set_ssl_config_service(new net::SSLConfigServiceDefaults);
    storage_->set_http_auth_handler_factory(
        net::HttpAuthHandlerFactory::CreateDefault(host_resolver.get()));
    storage_->set_http_server_properties(new net::HttpServerPropertiesImpl);

    FilePath cache_path = base_path_.Append(FILE_PATH_LITERAL("Cache"));
    net::HttpCache::DefaultBackend* main_backend =
        new net::HttpCache::DefaultBackend(
            net::DISK_CACHE,
            cache_path,
            0,
            BrowserThread::GetMessageLoopProxyForThread(
                BrowserThread::CACHE));

    net::HttpNetworkSession::Params network_session_params;
    network_session_params.cert_verifier =
        url_request_context_->cert_verifier();
    network_session_params.server_bound_cert_service =
        url_request_context_->server_bound_cert_service();
    network_session_params.proxy_service =
        url_request_context_->proxy_service();
    network_session_params.ssl_config_service =
        url_request_context_->ssl_config_service();
    network_session_params.http_auth_handler_factory =
        url_request_context_->http_auth_handler_factory();
    network_session_params.network_delegate =
        url_request_context_->network_delegate();
    network_session_params.http_server_properties =
        url_request_context_->http_server_properties();
    network_session_params.ignore_certificate_errors =
        ignore_certificate_errors_;
    if (command_line.HasSwitch(switches::kTestingFixedHttpPort)) {
      int value;
      base::StringToInt(command_line.GetSwitchValueASCII(
          switches::kTestingFixedHttpPort), &value);
      network_session_params.testing_fixed_http_port = value;
    }
    if (command_line.HasSwitch(switches::kTestingFixedHttpsPort)) {
      int value;
      base::StringToInt(command_line.GetSwitchValueASCII(
          switches::kTestingFixedHttpsPort), &value);
      network_session_params.testing_fixed_https_port = value;
    }
    if (command_line.HasSwitch(switches::kHostResolverRules)) {
      scoped_ptr<net::MappedHostResolver> mapped_host_resolver(
          new net::MappedHostResolver(host_resolver.Pass()));
      mapped_host_resolver->SetRulesFromString(
          command_line.GetSwitchValueASCII(switches::kHostResolverRules));
      host_resolver = mapped_host_resolver.Pass();
    }

    // Give |storage_| ownership at the end in case it's |mapped_host_resolver|.
    storage_->set_host_resolver(host_resolver.Pass());
    network_session_params.host_resolver =
        url_request_context_->host_resolver();

    net::HttpCache* main_cache = new net::HttpCache(
        network_session_params, main_backend);
    storage_->set_http_transaction_factory(main_cache);

    storage_->set_job_factory(new net::URLRequestJobFactoryImpl);
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
    ShellURLRequestContextGetter::GetNetworkTaskRunner() const {
  return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
}

net::HostResolver* ShellURLRequestContextGetter::host_resolver() {
  return url_request_context_->host_resolver();
}

}  // namespace content
