// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/io_thread/ios_io_thread.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_tracker.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/net_log/chrome_net_log.h"
#include "components/network_session_configurator/browser/network_session_configurator.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/ios/proxy_service_factory.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "ios/web/public/user_agent.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_thread.h"
#include "net/base/sdch_manager.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/log/net_log_event_type.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/tcp_client_socket.h"
#include "net/spdy/chromium/spdy_session.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "url/url_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// The IOSIOThread object must outlive any tasks posted to the IO thread
// before the Quit task, so base::Bind() calls are not refcounted.

namespace io_thread {

namespace {

const char kSupportedAuthSchemes[] = "basic,digest,ntlm";

}  // namespace

// Used for the "system" URLRequestContext.
class SystemURLRequestContext : public net::URLRequestContext {
 public:
  SystemURLRequestContext() = default;

 private:
  ~SystemURLRequestContext() override { AssertNoURLRequests(); }
};

std::unique_ptr<net::HostResolver> CreateGlobalHostResolver(
    net::NetLog* net_log) {
  TRACE_EVENT0("startup", "IOSIOThread::CreateGlobalHostResolver");

  std::unique_ptr<net::HostResolver> global_host_resolver =
      net::HostResolver::CreateSystemResolver(net::HostResolver::Options(),
                                              net_log);

  return global_host_resolver;
}

class IOSIOThread::LoggingNetworkChangeObserver
    : public net::NetworkChangeNotifier::IPAddressObserver,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // |net_log| must remain valid throughout our lifetime.
  explicit LoggingNetworkChangeObserver(net::NetLog* net_log)
      : net_log_(net_log) {
    net::NetworkChangeNotifier::AddIPAddressObserver(this);
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
    net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }

  ~LoggingNetworkChangeObserver() override {
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  // NetworkChangeNotifier::IPAddressObserver implementation.
  void OnIPAddressChanged() override {
    VLOG(1) << "Observed a change to the network IP addresses";

    net_log_->AddGlobalEntry(
        net::NetLogEventType::NETWORK_IP_ADDRESSES_CHANGED);
  }

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    std::string type_as_string =
        net::NetworkChangeNotifier::ConnectionTypeToString(type);

    VLOG(1) << "Observed a change to network connectivity state "
            << type_as_string;

    net_log_->AddGlobalEntry(
        net::NetLogEventType::NETWORK_CONNECTIVITY_CHANGED,
        net::NetLog::StringCallback("new_connection_type", &type_as_string));
  }

  // NetworkChangeNotifier::NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    std::string type_as_string =
        net::NetworkChangeNotifier::ConnectionTypeToString(type);

    VLOG(1) << "Observed a network change to state " << type_as_string;

    net_log_->AddGlobalEntry(
        net::NetLogEventType::NETWORK_CHANGED,
        net::NetLog::StringCallback("new_connection_type", &type_as_string));
  }

 private:
  net::NetLog* net_log_;
  DISALLOW_COPY_AND_ASSIGN(LoggingNetworkChangeObserver);
};

class SystemURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit SystemURLRequestContextGetter(IOSIOThread* io_thread);

  // Implementation for net::UrlRequestContextGetter.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

  // Tells the getter that the URLRequestContext is about to be shut down.
  void Shutdown();

 protected:
  ~SystemURLRequestContextGetter() override;

 private:
  IOSIOThread* io_thread_;  // Weak pointer, owned by ApplicationContext.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  base::debug::LeakTracker<SystemURLRequestContextGetter> leak_tracker_;
};

SystemURLRequestContextGetter::SystemURLRequestContextGetter(
    IOSIOThread* io_thread)
    : io_thread_(io_thread),
      network_task_runner_(
          web::WebThread::GetTaskRunnerForThread(web::WebThread::IO)) {}

SystemURLRequestContextGetter::~SystemURLRequestContextGetter() {}

net::URLRequestContext* SystemURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  if (!io_thread_)
    return nullptr;
  DCHECK(io_thread_->globals()->system_request_context.get());

  return io_thread_->globals()->system_request_context.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SystemURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

void SystemURLRequestContextGetter::Shutdown() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  io_thread_ = nullptr;
  NotifyContextShuttingDown();
}

IOSIOThread::Globals::SystemRequestContextLeakChecker::
    SystemRequestContextLeakChecker(Globals* globals)
    : globals_(globals) {
  DCHECK(globals_);
}

IOSIOThread::Globals::SystemRequestContextLeakChecker::
    ~SystemRequestContextLeakChecker() {
  if (globals_->system_request_context.get())
    globals_->system_request_context->AssertNoURLRequests();
}

IOSIOThread::Globals::Globals() : system_request_context_leak_checker(this) {}

IOSIOThread::Globals::~Globals() {}

// |local_state| is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOSIOThread more flexible for testing.
IOSIOThread::IOSIOThread(PrefService* local_state,
                         net_log::ChromeNetLog* net_log)
    : net_log_(net_log),
      globals_(nullptr),
      weak_factory_(this) {
  pref_proxy_config_tracker_ =
      ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
          local_state);
  system_proxy_config_service_ = ProxyServiceFactory::CreateProxyConfigService(
      pref_proxy_config_tracker_.get());

  ssl_config_service_manager_.reset(
      ssl_config::SSLConfigServiceManager::CreateDefaultManager(
          local_state,
          web::WebThread::GetTaskRunnerForThread(web::WebThread::IO)));

  web::WebThread::SetDelegate(web::WebThread::IO, this);
}

IOSIOThread::~IOSIOThread() {
  // This isn't needed for production code, but in tests, IOSIOThread may
  // be multiply constructed.
  web::WebThread::SetDelegate(web::WebThread::IO, nullptr);

  pref_proxy_config_tracker_->DetachFromPrefService();
  DCHECK(!globals_);
}

IOSIOThread::Globals* IOSIOThread::globals() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  return globals_;
}

void IOSIOThread::SetGlobalsForTesting(Globals* globals) {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);
  DCHECK(!globals || !globals_);
  globals_ = globals;
}

net_log::ChromeNetLog* IOSIOThread::net_log() {
  return net_log_;
}

void IOSIOThread::ChangedToOnTheRecord() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&IOSIOThread::ChangedToOnTheRecordOnIOThread,
                 base::Unretained(this)));
}

net::URLRequestContextGetter* IOSIOThread::system_url_request_context_getter() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (!system_url_request_context_getter_.get()) {
    // If we're in unit_tests, IOSIOThread may not be run.
    if (!web::WebThread::IsMessageLoopValid(web::WebThread::IO))
      return nullptr;
    system_url_request_context_getter_ =
        new SystemURLRequestContextGetter(this);
  }
  return system_url_request_context_getter_.get();
}

void IOSIOThread::Init() {
  TRACE_EVENT0("startup", "IOSIOThread::Init");
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  DCHECK(!globals_);
  globals_ = new Globals;

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(new LoggingNetworkChangeObserver(net_log_));

  // Setup the HistogramWatcher to run on the IO thread.
  net::NetworkChangeNotifier::InitHistogramWatcher();

  globals_->system_network_delegate = CreateSystemNetworkDelegate();
  globals_->host_resolver = CreateGlobalHostResolver(net_log_);

  globals_->cert_verifier = net::CertVerifier::CreateDefault();

  globals_->transport_security_state.reset(new net::TransportSecurityState());

  std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs(
      net::ct::CreateLogVerifiersForKnownLogs());

  net::MultiLogCTVerifier* ct_verifier = new net::MultiLogCTVerifier();
  globals_->cert_transparency_verifier.reset(ct_verifier);
  // Add built-in logs
  ct_verifier->AddLogs(ct_logs);

  globals_->ct_policy_enforcer.reset(new net::CTPolicyEnforcer());

  globals_->ssl_config_service = GetSSLConfigService();

  CreateDefaultAuthHandlerFactory();
  globals_->http_server_properties.reset(new net::HttpServerPropertiesImpl());
  // In-memory cookie store.
  globals_->system_cookie_store.reset(new net::CookieMonster(nullptr, nullptr));
  // In-memory channel ID store.
  globals_->system_channel_id_service.reset(
      new net::ChannelIDService(new net::DefaultChannelIDStore(nullptr)));
  globals_->system_cookie_store->SetChannelIDServiceID(
      globals_->system_channel_id_service->GetUniqueID());
  globals_->http_user_agent_settings.reset(new net::StaticHttpUserAgentSettings(
      std::string(),
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE)));

  params_.ignore_certificate_errors = false;
  params_.enable_user_alternate_protocol_ports = false;

  std::string quic_user_agent_id = GetChannelString();
  if (!quic_user_agent_id.empty())
    quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(
      version_info::GetProductNameAndVersionForUserAgent());
  quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(web::BuildOSCpuInfo());

  // Set up field trials, ignoring debug command line options.
  network_session_configurator::ParseCommandLineAndFieldTrials(
      base::CommandLine(base::CommandLine::NO_PROGRAM),
      /*is_quic_force_disabled=*/false, quic_user_agent_id, &params_);

  globals_->system_proxy_service = ProxyServiceFactory::CreateProxyService(
      net_log_, nullptr, globals_->system_network_delegate.get(),
      std::move(system_proxy_config_service_), true /* quick_check_enabled */);

  globals_->system_request_context.reset(
      ConstructSystemRequestContext(globals_, params_, net_log_));
}

void IOSIOThread::CleanUp() {
  system_url_request_context_getter_->Shutdown();
  system_url_request_context_getter_ = nullptr;

  // Release objects that the net::URLRequestContext could have been pointing
  // to.

  // Shutdown the HistogramWatcher on the IO thread.
  net::NetworkChangeNotifier::ShutdownHistogramWatcher();

  // This must be reset before the ChromeNetLog is destroyed.
  network_change_observer_.reset();

  system_proxy_config_service_.reset();

  delete globals_;
  globals_ = nullptr;

  base::debug::LeakTracker<SystemURLRequestContextGetter>::CheckForLeaks();
}

void IOSIOThread::CreateDefaultAuthHandlerFactory() {
  std::vector<std::string> supported_schemes =
      base::SplitString(kSupportedAuthSchemes, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  globals_->http_auth_preferences.reset(
      new net::HttpAuthPreferences(supported_schemes, std::string()));
  globals_->http_auth_handler_factory =
      net::HttpAuthHandlerRegistryFactory::Create(
          globals_->http_auth_preferences.get(), globals_->host_resolver.get());
}

void IOSIOThread::ClearHostCache() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  net::HostCache* host_cache = globals_->host_resolver->GetHostCache();
  if (host_cache)
    host_cache->clear();
}

const net::HttpNetworkSession::Params& IOSIOThread::NetworkSessionParams()
    const {
  return params_;
}

net::SSLConfigService* IOSIOThread::GetSSLConfigService() {
  return ssl_config_service_manager_->Get();
}

void IOSIOThread::ChangedToOnTheRecordOnIOThread() {
  DCHECK_CURRENTLY_ON(web::WebThread::IO);

  // Clear the host cache to avoid showing entries from the OTR session
  // in about:net-internals.
  ClearHostCache();
}

net::URLRequestContext* IOSIOThread::ConstructSystemRequestContext(
    IOSIOThread::Globals* globals,
    const net::HttpNetworkSession::Params& params,
    net::NetLog* net_log) {
  net::URLRequestContext* context = new SystemURLRequestContext;
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_transport_security_state(
      globals->transport_security_state.get());
  context->set_cert_transparency_verifier(
      globals->cert_transparency_verifier.get());
  context->set_ssl_config_service(globals->ssl_config_service.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->system_proxy_service.get());
  context->set_ct_policy_enforcer(globals->ct_policy_enforcer.get());

  net::URLRequestJobFactoryImpl* system_job_factory =
      new net::URLRequestJobFactoryImpl();
  // Data URLs are always loaded through the system request context on iOS
  // (due to UIWebView limitations).
  bool set_protocol = system_job_factory->SetProtocolHandler(
      url::kDataScheme, base::MakeUnique<net::DataProtocolHandler>());
  DCHECK(set_protocol);
  globals->system_url_request_job_factory.reset(system_job_factory);
  context->set_job_factory(globals->system_url_request_job_factory.get());

  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_channel_id_service(globals->system_channel_id_service.get());
  context->set_network_delegate(globals->system_network_delegate.get());
  context->set_http_user_agent_settings(
      globals->http_user_agent_settings.get());

  context->set_http_server_properties(globals->http_server_properties.get());

  net::HttpNetworkSession::Context system_context;
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(
      context, &system_context);

  globals->system_http_network_session.reset(
      new net::HttpNetworkSession(params, system_context));
  globals->system_http_transaction_factory.reset(
      new net::HttpNetworkLayer(globals->system_http_network_session.get()));
  context->set_http_transaction_factory(
      globals->system_http_transaction_factory.get());

  return context;
}

}  // namespace io_thread
