// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/web_view_url_request_context_getter.h"

#include <utility>

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task_scheduler/post_task.h"
#import "ios/net/cookies/cookie_store_ios_persistent.h"
#import "ios/web/public/web_client.h"
#include "ios/web_view/internal/web_view_network_delegate.h"
#include "net/base/cache_type.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_persister.h"
#include "net/http/transport_security_state.h"
#include "net/proxy/proxy_config_service_ios.h"
#include "net/proxy/proxy_service.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/ssl/ssl_config_service_defaults.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_storage.h"
#include "net/url_request/url_request_job_factory_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewURLRequestContextGetter::WebViewURLRequestContextGetter(
    const base::FilePath& base_path,
    const scoped_refptr<base::SingleThreadTaskRunner>& network_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner)
    : base_path_(base_path),
      file_task_runner_(file_task_runner),
      network_task_runner_(network_task_runner),
      proxy_config_service_(new net::ProxyConfigServiceIOS),
      net_log_(new net::NetLog()) {}

WebViewURLRequestContextGetter::~WebViewURLRequestContextGetter() = default;

net::URLRequestContext* WebViewURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());

  if (!url_request_context_) {
    url_request_context_.reset(new net::URLRequestContext());
    url_request_context_->set_net_log(net_log_.get());
    DCHECK(!network_delegate_.get());
    network_delegate_ = base::MakeUnique<WebViewNetworkDelegate>();
    url_request_context_->set_network_delegate(network_delegate_.get());

    storage_.reset(
        new net::URLRequestContextStorage(url_request_context_.get()));

    // Setup the cookie store.
    base::FilePath cookie_path;
    bool cookie_path_found = PathService::Get(base::DIR_APP_DATA, &cookie_path);
    DCHECK(cookie_path_found);
    cookie_path = cookie_path.Append("WebShell").Append("Cookies");
    scoped_refptr<net::CookieMonster::PersistentCookieStore> persistent_store =
        new net::SQLitePersistentCookieStore(
            cookie_path, network_task_runner_,
            base::CreateSequencedTaskRunnerWithTraits(
                {base::MayBlock(), base::TaskPriority::BACKGROUND}),
            true, nullptr);
    std::unique_ptr<net::CookieStoreIOS> cookie_store(
        new net::CookieStoreIOSPersistent(persistent_store.get()));
    storage_->set_cookie_store(std::move(cookie_store));

    std::string user_agent =
        web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE);

    storage_->set_http_user_agent_settings(
        base::MakeUnique<net::StaticHttpUserAgentSettings>("en-us,en",
                                                           user_agent));
    storage_->set_proxy_service(
        net::ProxyService::CreateUsingSystemProxyResolver(
            std::move(proxy_config_service_), url_request_context_->net_log()));
    storage_->set_ssl_config_service(new net::SSLConfigServiceDefaults);
    storage_->set_cert_verifier(net::CertVerifier::CreateDefault());

    storage_->set_transport_security_state(
        base::MakeUnique<net::TransportSecurityState>());
    storage_->set_cert_transparency_verifier(
        base::WrapUnique(new net::MultiLogCTVerifier));
    storage_->set_ct_policy_enforcer(
        base::WrapUnique(new net::CTPolicyEnforcer));
    transport_security_persister_.reset(new net::TransportSecurityPersister(
        url_request_context_->transport_security_state(), base_path_,
        file_task_runner_, false));
    storage_->set_channel_id_service(base::MakeUnique<net::ChannelIDService>(
        new net::DefaultChannelIDStore(nullptr)));
    storage_->set_http_server_properties(
        std::unique_ptr<net::HttpServerProperties>(
            new net::HttpServerPropertiesImpl()));

    std::unique_ptr<net::HostResolver> host_resolver(
        net::HostResolver::CreateDefaultResolver(
            url_request_context_->net_log()));
    storage_->set_http_auth_handler_factory(
        net::HttpAuthHandlerFactory::CreateDefault(host_resolver.get()));
    storage_->set_host_resolver(std::move(host_resolver));

    net::HttpNetworkSession::Context network_session_context;
    network_session_context.cert_verifier =
        url_request_context_->cert_verifier();
    network_session_context.transport_security_state =
        url_request_context_->transport_security_state();
    network_session_context.cert_transparency_verifier =
        url_request_context_->cert_transparency_verifier();
    network_session_context.channel_id_service =
        url_request_context_->channel_id_service();
    network_session_context.net_log = url_request_context_->net_log();
    network_session_context.proxy_service =
        url_request_context_->proxy_service();
    network_session_context.ssl_config_service =
        url_request_context_->ssl_config_service();
    network_session_context.http_auth_handler_factory =
        url_request_context_->http_auth_handler_factory();
    network_session_context.http_server_properties =
        url_request_context_->http_server_properties();
    network_session_context.host_resolver =
        url_request_context_->host_resolver();
    network_session_context.ct_policy_enforcer =
        url_request_context_->ct_policy_enforcer();

    base::FilePath cache_path = base_path_.Append(FILE_PATH_LITERAL("Cache"));
    std::unique_ptr<net::HttpCache::DefaultBackend> main_backend(
        new net::HttpCache::DefaultBackend(
            net::DISK_CACHE, net::CACHE_BACKEND_DEFAULT, cache_path, 0));

    storage_->set_http_network_session(
        base::MakeUnique<net::HttpNetworkSession>(
            net::HttpNetworkSession::Params(), network_session_context));
    storage_->set_http_transaction_factory(base::MakeUnique<net::HttpCache>(
        storage_->http_network_session(), std::move(main_backend),
        true /* set_up_quic_server_info */));

    std::unique_ptr<net::URLRequestJobFactoryImpl> job_factory(
        new net::URLRequestJobFactoryImpl());
    bool set_protocol = job_factory->SetProtocolHandler(
        "data", base::MakeUnique<net::DataProtocolHandler>());
    DCHECK(set_protocol);

    storage_->set_job_factory(std::move(job_factory));
  }

  return url_request_context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
WebViewURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

}  // namespace ios_web_view
