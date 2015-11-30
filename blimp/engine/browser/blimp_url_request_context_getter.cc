// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/blimp_url_request_context_getter.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/worker_pool.h"
#include "blimp/engine/browser/blimp_network_delegate.h"
#include "blimp/engine/common/blimp_content_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/common/content_switches.h"
#include "net/base/cache_type.h"
#include "net/cert/cert_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_intercepting_job_factory.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "url/url_constants.h"

namespace blimp {
namespace engine {

namespace {

void InstallProtocolHandlers(net::URLRequestJobFactoryImpl* job_factory,
                             content::ProtocolHandlerMap* protocol_handlers) {
  for (content::ProtocolHandlerMap::iterator it = protocol_handlers->begin();
       it != protocol_handlers->end(); ++it) {
    bool set_protocol = job_factory->SetProtocolHandler(
        it->first, make_scoped_ptr(it->second.release()));
    DCHECK(set_protocol);
  }
  protocol_handlers->clear();
}

}  // namespace

BlimpURLRequestContextGetter::BlimpURLRequestContextGetter(
    bool ignore_certificate_errors,
    const base::FilePath& base_path,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_loop_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_loop_task_runner,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors,
    net::NetLog* net_log)
    : ignore_certificate_errors_(ignore_certificate_errors),
      base_path_(base_path),
      io_loop_task_runner_(io_loop_task_runner),
      file_loop_task_runner_(file_loop_task_runner),
      net_log_(net_log),
      request_interceptors_(std::move(request_interceptors)) {
  // Must first be created on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::swap(protocol_handlers_, *protocol_handlers);

  // We must create the proxy config service on the UI loop on Linux because it
  // must synchronously run on the glib message loop. This will be passed to
  // the URLRequestContextStorage on the IO thread in GetURLRequestContext().
  proxy_config_service_ = GetProxyConfigService();
}

BlimpURLRequestContextGetter::~BlimpURLRequestContextGetter() {}

scoped_ptr<net::NetworkDelegate>
BlimpURLRequestContextGetter::CreateNetworkDelegate() {
  return make_scoped_ptr(new BlimpNetworkDelegate);
}

scoped_ptr<net::ProxyConfigService>
BlimpURLRequestContextGetter::GetProxyConfigService() {
  return net::ProxyService::CreateSystemProxyConfigService(
      io_loop_task_runner_, file_loop_task_runner_);
}

scoped_ptr<net::ProxyService> BlimpURLRequestContextGetter::GetProxyService() {
  return net::ProxyService::CreateUsingSystemProxyResolver(
      std::move(proxy_config_service_), 0, url_request_context_->net_log());
}

net::URLRequestContext* BlimpURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!url_request_context_) {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();

    url_request_context_.reset(new net::URLRequestContext());
    url_request_context_->set_net_log(net_log_);
    network_delegate_ = CreateNetworkDelegate();
    url_request_context_->set_network_delegate(network_delegate_.get());
    storage_.reset(
        new net::URLRequestContextStorage(url_request_context_.get()));
    storage_->set_cookie_store(
        content::CreateCookieStore(content::CookieStoreConfig()));
    storage_->set_channel_id_service(make_scoped_ptr(
        new net::ChannelIDService(new net::DefaultChannelIDStore(NULL),
                                  base::WorkerPool::GetTaskRunner(true))));
    storage_->set_http_user_agent_settings(
        make_scoped_ptr(new net::StaticHttpUserAgentSettings(
            "en-us,en", GetBlimpEngineUserAgent())));

    scoped_ptr<net::HostResolver> host_resolver(
        net::HostResolver::CreateDefaultResolver(
            url_request_context_->net_log()));

    storage_->set_cert_verifier(net::CertVerifier::CreateDefault());
    storage_->set_transport_security_state(
        make_scoped_ptr(new net::TransportSecurityState));
    storage_->set_proxy_service(GetProxyService());
    storage_->set_ssl_config_service(new net::SSLConfigServiceDefaults);
    storage_->set_http_auth_handler_factory(
        net::HttpAuthHandlerFactory::CreateDefault(host_resolver.get()));
    storage_->set_http_server_properties(
        make_scoped_ptr(new net::HttpServerPropertiesImpl()));

    base::FilePath cache_path = base_path_.Append(FILE_PATH_LITERAL("Cache"));
    scoped_ptr<net::HttpCache::DefaultBackend> main_backend(
        new net::HttpCache::DefaultBackend(
            net::DISK_CACHE, net::CACHE_BACKEND_DEFAULT, cache_path, 0,
            content::BrowserThread::GetMessageLoopProxyForThread(
                content::BrowserThread::CACHE)));

    net::HttpNetworkSession::Params network_session_params;
    network_session_params.cert_verifier =
        url_request_context_->cert_verifier();
    network_session_params.transport_security_state =
        url_request_context_->transport_security_state();
    network_session_params.channel_id_service =
        url_request_context_->channel_id_service();
    network_session_params.proxy_service =
        url_request_context_->proxy_service();
    network_session_params.ssl_config_service =
        url_request_context_->ssl_config_service();
    network_session_params.http_auth_handler_factory =
        url_request_context_->http_auth_handler_factory();
    network_session_params.network_delegate = network_delegate_.get();
    network_session_params.http_server_properties =
        url_request_context_->http_server_properties();
    network_session_params.net_log = url_request_context_->net_log();
    network_session_params.ignore_certificate_errors =
        ignore_certificate_errors_;
    if (command_line.HasSwitch(switches::kHostResolverRules)) {
      scoped_ptr<net::MappedHostResolver> mapped_host_resolver(
          new net::MappedHostResolver(std::move(host_resolver)));
      mapped_host_resolver->SetRulesFromString(
          command_line.GetSwitchValueASCII(switches::kHostResolverRules));
      host_resolver = std::move(mapped_host_resolver);
    }

    // Give |storage_| ownership at the end in case it's |mapped_host_resolver|.
    storage_->set_host_resolver(std::move(host_resolver));
    network_session_params.host_resolver =
        url_request_context_->host_resolver();

    storage_->set_http_network_session(
        make_scoped_ptr(new net::HttpNetworkSession(network_session_params)));
    storage_->set_http_transaction_factory(make_scoped_ptr(new net::HttpCache(
        storage_->http_network_session(), std::move(main_backend), true)));

    scoped_ptr<net::URLRequestJobFactoryImpl> job_factory(
        new net::URLRequestJobFactoryImpl());
    // Keep ProtocolHandlers added in sync with
    // BlimpContentBrowserClient::IsHandledURL().
    InstallProtocolHandlers(job_factory.get(), &protocol_handlers_);
    bool set_protocol = job_factory->SetProtocolHandler(
        url::kDataScheme, make_scoped_ptr(new net::DataProtocolHandler));
    DCHECK(set_protocol);

    // Set up interceptors in the reverse order so that the last inceptor is at
    // the end of the linked list of job factories.
    scoped_ptr<net::URLRequestJobFactory> top_job_factory =
        std::move(job_factory);
    for (auto i = request_interceptors_.rbegin();
         i != request_interceptors_.rend(); ++i) {
      top_job_factory.reset(new net::URLRequestInterceptingJobFactory(
          std::move(top_job_factory), make_scoped_ptr(*i)));
    }
    request_interceptors_.weak_clear();
    // Save the head of the job factory list at storage_.
    storage_->set_job_factory(std::move(top_job_factory));
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
BlimpURLRequestContextGetter::GetNetworkTaskRunner() const {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::IO);
}

net::HostResolver* BlimpURLRequestContextGetter::host_resolver() {
  return url_request_context_->host_resolver();
}

}  // namespace engine
}  // namespace blimp
