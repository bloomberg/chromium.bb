// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/network_context.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "build/build_config.h"
#include "components/cookie_config/cookie_store_util.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "content/network/cache_url_loader.h"
#include "content/network/http_server_properties_pref_delegate.h"
#include "content/network/network_service_impl.h"
#include "content/network/network_service_url_loader_factory.h"
#include "content/network/restricted_cookie_manager.h"
#include "content/network/throttling/network_conditions.h"
#include "content/network/throttling/throttling_controller.h"
#include "content/network/throttling/throttling_network_transaction_factory.h"
#include "content/network/url_loader.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/network/ignore_errors_cert_verifier.h"
#include "content/public/network/url_request_context_builder_mojo.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/cookies/cookie_monster.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/extras/sqlite/sqlite_channel_id_store.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_server_properties_manager.h"
#include "net/http/http_transaction_factory.h"
#include "net/proxy/proxy_config.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/proxy_config_service_mojo.h"

namespace content {

namespace {

net::CertVerifier* g_cert_verifier_for_testing = nullptr;

// A CertVerifier that forwards all requests to |g_cert_verifier_for_testing|.
// This is used to allow NetworkContexts to have their own
// std::unique_ptr<net::CertVerifier> while forwarding calls to the shared
// verifier.
class WrappedTestingCertVerifier : public net::CertVerifier {
 public:
  ~WrappedTestingCertVerifier() override = default;

  // CertVerifier implementation
  int Verify(const RequestParams& params,
             net::CRLSet* crl_set,
             net::CertVerifyResult* verify_result,
             const net::CompletionCallback& callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    verify_result->Reset();
    if (!g_cert_verifier_for_testing)
      return net::ERR_FAILED;
    return g_cert_verifier_for_testing->Verify(params, crl_set, verify_result,
                                               callback, out_req, net_log);
  }
};

}  // namespace

NetworkContext::NetworkContext(NetworkServiceImpl* network_service,
                               network::mojom::NetworkContextRequest request,
                               network::mojom::NetworkContextParamsPtr params)
    : network_service_(network_service),
      params_(std::move(params)),
      binding_(this, std::move(request)) {
  url_request_context_owner_ = MakeURLRequestContext(params_.get());
  url_request_context_ = url_request_context_owner_.url_request_context.get();
  cookie_manager_ = std::make_unique<network::CookieManager>(
      url_request_context_->cookie_store());
  network_service_->RegisterNetworkContext(this);
  binding_.set_connection_error_handler(base::BindOnce(
      &NetworkContext::OnConnectionError, base::Unretained(this)));
}

// TODO(mmenke): Share URLRequestContextBulder configuration between two
// constructors. Can only share them once consumer code is ready for its
// corresponding options to be overwritten.
NetworkContext::NetworkContext(
    NetworkServiceImpl* network_service,
    network::mojom::NetworkContextRequest request,
    network::mojom::NetworkContextParamsPtr params,
    std::unique_ptr<URLRequestContextBuilderMojo> builder)
    : network_service_(network_service),
      params_(std::move(params)),
      binding_(this, std::move(request)) {
  url_request_context_owner_ = ApplyContextParamsToBuilder(
      builder.get(), params_.get(), network_service->quic_disabled(),
      network_service->net_log());
  url_request_context_ = url_request_context_owner_.url_request_context.get();
  network_service_->RegisterNetworkContext(this);
  cookie_manager_ = std::make_unique<network::CookieManager>(
      url_request_context_->cookie_store());
}

NetworkContext::NetworkContext(NetworkServiceImpl* network_service,
                               network::mojom::NetworkContextRequest request,
                               net::URLRequestContext* url_request_context)
    : network_service_(network_service),
      url_request_context_(url_request_context),
      binding_(this, std::move(request)),
      cookie_manager_(std::make_unique<network::CookieManager>(
          url_request_context->cookie_store())) {
  // May be nullptr in tests.
  if (network_service_)
    network_service_->RegisterNetworkContext(this);
}

NetworkContext::~NetworkContext() {
  // Call each URLLoader and ask it to release its net::URLRequest, as the
  // corresponding net::URLRequestContext is going away with this
  // NetworkContext. The loaders can be deregistering themselves in Cleanup(),
  // so have to be careful.
  while (!url_loaders_.empty())
    (*url_loaders_.begin())->Cleanup();

  // May be nullptr in tests.
  if (network_service_)
    network_service_->DeregisterNetworkContext(this);
}

std::unique_ptr<NetworkContext> NetworkContext::CreateForTesting() {
  return base::WrapUnique(
      new NetworkContext(network::mojom::NetworkContextParams::New()));
}

void NetworkContext::SetCertVerifierForTesting(
    net::CertVerifier* cert_verifier) {
  g_cert_verifier_for_testing = cert_verifier;
}

void NetworkContext::RegisterURLLoader(URLLoader* url_loader) {
  DCHECK(url_loaders_.count(url_loader) == 0);
  url_loaders_.insert(url_loader);
}

void NetworkContext::DeregisterURLLoader(URLLoader* url_loader) {
  size_t removed_count = url_loaders_.erase(url_loader);
  DCHECK(removed_count);
}

void NetworkContext::CreateURLLoaderFactory(
    network::mojom::URLLoaderFactoryRequest request,
    uint32_t process_id) {
  loader_factory_bindings_.AddBinding(
      std::make_unique<NetworkServiceURLLoaderFactory>(this, process_id),
      std::move(request));
}

void NetworkContext::HandleViewCacheRequest(
    const GURL& url,
    network::mojom::URLLoaderClientPtr client) {
  StartCacheURLLoader(url, url_request_context_, std::move(client));
}

void NetworkContext::GetCookieManager(
    network::mojom::CookieManagerRequest request) {
  cookie_manager_->AddRequest(std::move(request));
}

void NetworkContext::GetRestrictedCookieManager(
    network::mojom::RestrictedCookieManagerRequest request,
    int32_t render_process_id,
    int32_t render_frame_id) {
  // TODO(crbug.com/729800): RestrictedCookieManager should own its bindings
  //     and NetworkContext should own the RestrictedCookieManager
  //     instances.
  mojo::MakeStrongBinding(std::make_unique<RestrictedCookieManager>(
                              url_request_context_->cookie_store(),
                              render_process_id, render_frame_id),
                          std::move(request));
}

void NetworkContext::DisableQuic() {
  url_request_context_->http_transaction_factory()->GetSession()->DisableQuic();
}

void NetworkContext::Cleanup() {
  // The NetworkService is going away, so have to destroy the
  // net::URLRequestContext held by this NetworkContext.
  delete this;
}

NetworkContext::NetworkContext(network::mojom::NetworkContextParamsPtr params)
    : network_service_(nullptr), params_(std::move(params)), binding_(this) {
  url_request_context_owner_ = MakeURLRequestContext(params_.get());
  url_request_context_ = url_request_context_owner_.url_request_context.get();
}

void NetworkContext::OnConnectionError() {
  // Don't delete |this| in response to connection errors when it was created by
  // CreateForTesting.
  if (network_service_)
    delete this;
}

URLRequestContextOwner NetworkContext::MakeURLRequestContext(
    network::mojom::NetworkContextParams* network_context_params) {
  URLRequestContextBuilderMojo builder;
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

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

  // The cookie configuration is in this method, which is only used by the
  // network process, and not ApplyContextParamsToBuilder which is used by the
  // browser as well. This is because this code path doesn't handle encryption
  // and other configuration done in QuotaPolicyCookieStore yet (and we still
  // have to figure out which of the latter needs to move to the network
  // process). TODO: http://crbug.com/789644
  if (network_context_params->cookie_path) {
    DCHECK(network_context_params->channel_id_path);
    net::CookieCryptoDelegate* crypto_delegate = nullptr;

    scoped_refptr<base::SequencedTaskRunner> client_task_runner =
        base::MessageLoop::current()->task_runner();
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BACKGROUND,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

    scoped_refptr<net::SQLiteChannelIDStore> channel_id_db =
        new net::SQLiteChannelIDStore(
            network_context_params->channel_id_path.value(),
            background_task_runner);
    std::unique_ptr<net::ChannelIDService> channel_id_service(
        std::make_unique<net::ChannelIDService>(
            new net::DefaultChannelIDStore(channel_id_db.get())));

    scoped_refptr<net::SQLitePersistentCookieStore> sqlite_store(
        new net::SQLitePersistentCookieStore(
            network_context_params->cookie_path.value(), client_task_runner,
            background_task_runner,
            network_context_params->restore_old_session_cookies,
            crypto_delegate));

    std::unique_ptr<net::CookieMonster> cookie_store =
        std::make_unique<net::CookieMonster>(sqlite_store.get(),
                                             channel_id_service.get());
    if (network_context_params->persist_session_cookies)
      cookie_store->SetPersistSessionCookies(true);

    cookie_store->SetChannelIDServiceID(channel_id_service->GetUniqueID());
    builder.SetCookieAndChannelIdStores(std::move(cookie_store),
                                        std::move(channel_id_service));
  } else {
    DCHECK(!network_context_params->restore_old_session_cookies);
    DCHECK(!network_context_params->persist_session_cookies);
  }

  if (g_cert_verifier_for_testing) {
    builder.SetCertVerifier(std::make_unique<WrappedTestingCertVerifier>());
  } else {
    std::unique_ptr<net::CertVerifier> cert_verifier =
        net::CertVerifier::CreateDefault();
    builder.SetCertVerifier(
        content::IgnoreErrorsCertVerifier::MaybeWrapCertVerifier(
            *command_line, nullptr, std::move(cert_verifier)));
  }

  // |network_service_| may be nullptr in tests.
  return ApplyContextParamsToBuilder(
      &builder, network_context_params,
      network_service_ ? network_service_->quic_disabled() : false,
      network_service_ ? network_service_->net_log() : nullptr);
}

URLRequestContextOwner NetworkContext::ApplyContextParamsToBuilder(
    URLRequestContextBuilderMojo* builder,
    network::mojom::NetworkContextParams* network_context_params,
    bool quic_disabled,
    net::NetLog* net_log) {
  URLRequestContextOwner url_request_owner;
  if (net_log)
    builder->set_net_log(net_log);

  builder->set_enable_brotli(network_context_params->enable_brotli);
  if (network_context_params->context_name)
    builder->set_name(*network_context_params->context_name);

  if (network_context_params->proxy_resolver_factory) {
    builder->SetMojoProxyResolverFactory(
        proxy_resolver::mojom::ProxyResolverFactoryPtr(
            std::move(network_context_params->proxy_resolver_factory)));
  }

  if (!network_context_params->http_cache_enabled) {
    builder->DisableHttpCache();
  } else {
    net::URLRequestContextBuilder::HttpCacheParams cache_params;
    cache_params.max_size = network_context_params->http_cache_max_size;
    if (!network_context_params->http_cache_path) {
      cache_params.type =
          net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;
    } else {
      cache_params.path = *network_context_params->http_cache_path;
      cache_params.type = network_session_configurator::ChooseCacheType(
          *base::CommandLine::ForCurrentProcess());
    }

    builder->EnableHttpCache(cache_params);
  }

  if (!network_context_params->initial_proxy_config &&
      !network_context_params->proxy_config_client_request.is_pending()) {
    network_context_params->initial_proxy_config =
        net::ProxyConfig::CreateDirect();
  }
  builder->set_proxy_config_service(
      std::make_unique<network::ProxyConfigServiceMojo>(
          std::move(network_context_params->proxy_config_client_request),
          std::move(network_context_params->initial_proxy_config),
          std::move(network_context_params->proxy_config_poller_client)));

  if (network_context_params->http_server_properties_path) {
    scoped_refptr<JsonPrefStore> json_pref_store(new JsonPrefStore(
        *network_context_params->http_server_properties_path,
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
             base::TaskPriority::BACKGROUND})));
    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_user_prefs(json_pref_store);
    pref_service_factory.set_async(true);
    scoped_refptr<PrefRegistrySimple> pref_registry(new PrefRegistrySimple());
    HttpServerPropertiesPrefDelegate::RegisterPrefs(pref_registry.get());
    url_request_owner.pref_service =
        pref_service_factory.Create(pref_registry.get());

    builder->SetHttpServerProperties(
        std::make_unique<net::HttpServerPropertiesManager>(
            std::make_unique<HttpServerPropertiesPrefDelegate>(
                url_request_owner.pref_service.get()),
            net_log));
  }

  builder->set_data_enabled(network_context_params->enable_data_url_support);
#if !BUILDFLAG(DISABLE_FILE_SUPPORT)
  builder->set_file_enabled(network_context_params->enable_file_url_support);
#else  // BUILDFLAG(DISABLE_FILE_SUPPORT)
  DCHECK(!network_context_params->enable_file_url_support);
#endif
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  builder->set_ftp_enabled(network_context_params->enable_ftp_url_support);
#else  // BUILDFLAG(DISABLE_FTP_SUPPORT)
  DCHECK(!network_context_params->enable_ftp_url_support);
#endif

  net::HttpNetworkSession::Params session_params;
  bool is_quic_force_disabled = false;
  if (quic_disabled)
    is_quic_force_disabled = true;

  network_session_configurator::ParseCommandLineAndFieldTrials(
      *base::CommandLine::ForCurrentProcess(), is_quic_force_disabled,
      network_context_params->quic_user_agent_id, &session_params);

  session_params.http_09_on_non_default_ports_enabled =
      network_context_params->http_09_on_non_default_ports_enabled;

  builder->set_http_network_session_params(session_params);

  builder->SetCreateHttpTransactionFactoryCallback(
      base::BindOnce([](net::HttpNetworkSession* session)
                         -> std::unique_ptr<net::HttpTransactionFactory> {
        return std::make_unique<ThrottlingNetworkTransactionFactory>(session);
      }));
  url_request_owner.url_request_context = builder->Build();
  return url_request_owner;
}

void NetworkContext::ClearNetworkingHistorySince(
    base::Time time,
    base::OnceClosure completion_callback) {
  // TODO(mmenke): Neither of these methods waits until the changes have been
  // commited to disk. They probably should, as most similar methods net/
  // exposes do.

  // Completes synchronously.
  url_request_context_->transport_security_state()->DeleteAllDynamicDataSince(
      time);

  url_request_context_->http_server_properties()->Clear(
      std::move(completion_callback));
}

void NetworkContext::SetNetworkConditions(
    const std::string& profile_id,
    network::mojom::NetworkConditionsPtr conditions) {
  std::unique_ptr<NetworkConditions> network_conditions;
  if (conditions) {
    network_conditions.reset(new NetworkConditions(
        conditions->offline, conditions->latency.InMillisecondsF(),
        conditions->download_throughput, conditions->upload_throughput));
  }
  ThrottlingController::SetConditions(profile_id,
                                      std::move(network_conditions));
}

void NetworkContext::AddHSTSForTesting(const std::string& host,
                                       base::Time expiry,
                                       bool include_subdomains,
                                       AddHSTSForTestingCallback callback) {
  net::TransportSecurityState* state =
      url_request_context_->transport_security_state();
  state->AddHSTS(host, expiry, include_subdomains);
  std::move(callback).Run();
}

}  // namespace content
