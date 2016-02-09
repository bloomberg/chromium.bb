// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_io_thread.h"

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
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/net_log/chrome_net_log.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/net/cookie_util.h"
#include "ios/chrome/browser/net/ios_chrome_network_delegate.h"
#include "ios/chrome/browser/net/proxy_service_factory.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/user_agent.h"
#include "ios/web/public/web_client.h"
#include "ios/web/public/web_thread.h"
#include "net/base/external_estimate_provider.h"
#include "net/base/net_util.h"
#include "net/base/network_quality_estimator.h"
#include "net/base/sdch_manager.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_known_logs_static.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cert_net/nss_ocsp.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/socket/tcp_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_request_backoff_manager.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "url/url_constants.h"

// The IOSChromeIOThread object must outlive any tasks posted to the IO thread
// before the Quit task, so base::Bind() calls are not refcounted.

namespace {

const char kSupportedAuthSchemes[] = "basic,digest,ntlm";

const char kTCPFastOpenFieldTrialName[] = "TCPFastOpen";
const char kTCPFastOpenHttpsEnabledGroupName[] = "HttpsEnabled";

const char kQuicFieldTrialName[] = "QUIC";
const char kQuicFieldTrialEnabledGroupName[] = "Enabled";
const char kQuicFieldTrialHttpsEnabledGroupName[] = "HttpsEnabled";

// The SPDY trial composes two different trial plus control groups:
//  * A "holdback" group with SPDY disabled, and corresponding control
//  (SPDY/3.1). The primary purpose of the holdback group is to encourage site
//  operators to do feature detection rather than UA-sniffing. As such, this
//  trial runs continuously.
//  * A SPDY/4 experiment, for SPDY/4 (aka HTTP/2) vs SPDY/3.1 comparisons and
//  eventual SPDY/4 deployment.
const char kSpdyFieldTrialName[] = "SPDY";
const char kSpdyFieldTrialHoldbackGroupNamePrefix[] = "SpdyDisabled";
const char kSpdyFieldTrialSpdy31GroupNamePrefix[] = "Spdy31Enabled";
const char kSpdyFieldTrialSpdy4GroupNamePrefix[] = "Spdy4Enabled";
const char kSpdyFieldTrialParametrizedPrefix[] = "Parametrized";

// The AltSvc trial controls whether Alt-Svc headers are parsed.
// Disabled:
//     Alt-Svc headers are not parsed.
//     Alternate-Protocol headers are parsed.
// Enabled:
//     Alt-Svc headers are parsed, but only same-host entries are used by
//     default.  (Use "enable_alternative_service_with_different_host" QUIC
//     parameter to enable entries with different hosts.)
//     Alternate-Protocol headers are ignored for responses that have an Alt-Svc
//     header.
const char kAltSvcFieldTrialName[] = "ParseAltSvc";
const char kAltSvcFieldTrialDisabledPrefix[] = "AltSvcDisabled";
const char kAltSvcFieldTrialEnabledPrefix[] = "AltSvcEnabled";

// Field trial for network quality estimator. Seeds RTT and downstream
// throughput observations with values that correspond to the connection type
// determined by the operating system.
const char kNetworkQualityEstimatorFieldTrialName[] = "NetworkQualityEstimator";

// Field trial for NPN.
const char kNpnTrialName[] = "NPN";
const char kNpnTrialEnabledGroupNamePrefix[] = "Enable";
const char kNpnTrialDisabledGroupNamePrefix[] = "Disable";

// Used for the "system" URLRequestContext.
class SystemURLRequestContext : public net::URLRequestContext {
 public:
  SystemURLRequestContext() { net::SetURLRequestContextForNSSHttpIO(this); }

 private:
  ~SystemURLRequestContext() override {
    AssertNoURLRequests();
    net::SetURLRequestContextForNSSHttpIO(nullptr);
  }
};

scoped_ptr<net::HostResolver> CreateGlobalHostResolver(net::NetLog* net_log) {
  TRACE_EVENT0("startup", "IOSChromeIOThread::CreateGlobalHostResolver");
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  scoped_ptr<net::HostResolver> global_host_resolver =
      net::HostResolver::CreateSystemResolver(net::HostResolver::Options(),
                                              net_log);

  // If hostname remappings were specified on the command-line, layer these
  // rules on top of the real host resolver. This allows forwarding all requests
  // through a designated test server.
  if (!command_line.HasSwitch(switches::kIOSHostResolverRules))
    return global_host_resolver;

  scoped_ptr<net::MappedHostResolver> remapped_resolver(
      new net::MappedHostResolver(std::move(global_host_resolver)));
  remapped_resolver->SetRulesFromString(
      command_line.GetSwitchValueASCII(switches::kIOSHostResolverRules));
  return std::move(remapped_resolver);
}

int GetSwitchValueAsInt(const base::CommandLine& command_line,
                        const std::string& switch_name) {
  int value;
  if (!base::StringToInt(command_line.GetSwitchValueASCII(switch_name),
                         &value)) {
    return 0;
  }
  return value;
}

// Returns the value associated with |key| in |params| or "" if the
// key is not present in the map.
const std::string& GetVariationParam(
    const std::map<std::string, std::string>& params,
    const std::string& key) {
  std::map<std::string, std::string>::const_iterator it = params.find(key);
  if (it == params.end())
    return base::EmptyString();

  return it->second;
}

}  // namespace

class IOSChromeIOThread::LoggingNetworkChangeObserver
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

    net_log_->AddGlobalEntry(net::NetLog::TYPE_NETWORK_IP_ADDRESSES_CHANGED);
  }

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    std::string type_as_string =
        net::NetworkChangeNotifier::ConnectionTypeToString(type);

    VLOG(1) << "Observed a change to network connectivity state "
            << type_as_string;

    net_log_->AddGlobalEntry(
        net::NetLog::TYPE_NETWORK_CONNECTIVITY_CHANGED,
        net::NetLog::StringCallback("new_connection_type", &type_as_string));
  }

  // NetworkChangeNotifier::NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    std::string type_as_string =
        net::NetworkChangeNotifier::ConnectionTypeToString(type);

    VLOG(1) << "Observed a network change to state " << type_as_string;

    net_log_->AddGlobalEntry(
        net::NetLog::TYPE_NETWORK_CHANGED,
        net::NetLog::StringCallback("new_connection_type", &type_as_string));
  }

 private:
  net::NetLog* net_log_;
  DISALLOW_COPY_AND_ASSIGN(LoggingNetworkChangeObserver);
};

class SystemURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit SystemURLRequestContextGetter(IOSChromeIOThread* io_thread);

  // Implementation for net::UrlRequestContextGetter.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 protected:
  ~SystemURLRequestContextGetter() override;

 private:
  IOSChromeIOThread* const
      io_thread_;  // Weak pointer, owned by ApplicationContext.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  base::debug::LeakTracker<SystemURLRequestContextGetter> leak_tracker_;
};

SystemURLRequestContextGetter::SystemURLRequestContextGetter(
    IOSChromeIOThread* io_thread)
    : io_thread_(io_thread),
      network_task_runner_(
          web::WebThread::GetTaskRunnerForThread(web::WebThread::IO)) {}

SystemURLRequestContextGetter::~SystemURLRequestContextGetter() {}

net::URLRequestContext* SystemURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  DCHECK(io_thread_->globals()->system_request_context.get());

  return io_thread_->globals()->system_request_context.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SystemURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

IOSChromeIOThread::Globals::SystemRequestContextLeakChecker::
    SystemRequestContextLeakChecker(Globals* globals)
    : globals_(globals) {
  DCHECK(globals_);
}

IOSChromeIOThread::Globals::SystemRequestContextLeakChecker::
    ~SystemRequestContextLeakChecker() {
  if (globals_->system_request_context.get())
    globals_->system_request_context->AssertNoURLRequests();
}

IOSChromeIOThread::Globals::Globals()
    : system_request_context_leak_checker(this),
      testing_fixed_http_port(0),
      testing_fixed_https_port(0) {}

IOSChromeIOThread::Globals::~Globals() {}

// |local_state| is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOSChromeIOThread more flexible for testing.
IOSChromeIOThread::IOSChromeIOThread(PrefService* local_state,
                                     net_log::ChromeNetLog* net_log)
    : net_log_(net_log),
      globals_(nullptr),
      creation_time_(base::TimeTicks::Now()),
      weak_factory_(this) {
  pref_proxy_config_tracker_ =
      ios::ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
          local_state);
  IOSChromeNetworkDelegate::InitializePrefsOnUIThread(nullptr, local_state);
  ssl_config_service_manager_.reset(
      ssl_config::SSLConfigServiceManager::CreateDefaultManager(
          local_state,
          web::WebThread::GetTaskRunnerForThread(web::WebThread::IO)));

  web::WebThread::SetDelegate(web::WebThread::IO, this);
}

IOSChromeIOThread::~IOSChromeIOThread() {
  // This isn't needed for production code, but in tests, IOSChromeIOThread may
  // be multiply constructed.
  web::WebThread::SetDelegate(web::WebThread::IO, nullptr);

  pref_proxy_config_tracker_->DetachFromPrefService();
  DCHECK(!globals_);
}

IOSChromeIOThread::Globals* IOSChromeIOThread::globals() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  return globals_;
}

void IOSChromeIOThread::SetGlobalsForTesting(Globals* globals) {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  DCHECK(!globals || !globals_);
  globals_ = globals;
}

net_log::ChromeNetLog* IOSChromeIOThread::net_log() {
  return net_log_;
}

void IOSChromeIOThread::ChangedToOnTheRecord() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&IOSChromeIOThread::ChangedToOnTheRecordOnIOThread,
                 base::Unretained(this)));
}

net::URLRequestContextGetter*
IOSChromeIOThread::system_url_request_context_getter() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::UI);
  if (!system_url_request_context_getter_.get()) {
    InitSystemRequestContext();
  }
  return system_url_request_context_getter_.get();
}

void IOSChromeIOThread::Init() {
  TRACE_EVENT0("startup", "IOSChromeIOThread::Init");
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  net::SetMessageLoopForNSSHttpIO();

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  DCHECK(!globals_);
  globals_ = new Globals;

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(new LoggingNetworkChangeObserver(net_log_));

  // Setup the HistogramWatcher to run on the IO thread.
  net::NetworkChangeNotifier::InitHistogramWatcher();

  scoped_ptr<IOSChromeNetworkDelegate> chrome_network_delegate(
      new IOSChromeNetworkDelegate());

  globals_->system_network_delegate = std::move(chrome_network_delegate);
  globals_->host_resolver = CreateGlobalHostResolver(net_log_);

  std::map<std::string, std::string> network_quality_estimator_params;
  variations::GetVariationParams(kNetworkQualityEstimatorFieldTrialName,
                                 &network_quality_estimator_params);

  scoped_ptr<net::ExternalEstimateProvider> external_estimate_provider;
  // Pass ownership.
  globals_->network_quality_estimator.reset(new net::NetworkQualityEstimator(
      std::move(external_estimate_provider), network_quality_estimator_params));

  globals_->cert_verifier.reset(
      new net::MultiThreadedCertVerifier(net::CertVerifyProc::CreateDefault()));

  globals_->transport_security_state.reset(new net::TransportSecurityState());

  std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs(
      net::ct::CreateLogVerifiersForKnownLogs());

  net::MultiLogCTVerifier* ct_verifier = new net::MultiLogCTVerifier();
  globals_->cert_transparency_verifier.reset(ct_verifier);
  // Add built-in logs
  ct_verifier->AddLogs(ct_logs);

  net::CTPolicyEnforcer* policy_enforcer = new net::CTPolicyEnforcer;
  globals_->ct_policy_enforcer.reset(policy_enforcer);

  globals_->ssl_config_service = GetSSLConfigService();

  CreateDefaultAuthHandlerFactory();
  globals_->http_server_properties.reset(new net::HttpServerPropertiesImpl());
  // In-memory cookie store.
  globals_->system_cookie_store = new net::CookieMonster(nullptr, nullptr);
  // In-memory channel ID store.
  globals_->system_channel_id_service.reset(
      new net::ChannelIDService(new net::DefaultChannelIDStore(nullptr),
                                base::WorkerPool::GetTaskRunner(true)));
  globals_->http_user_agent_settings.reset(new net::StaticHttpUserAgentSettings(
      std::string(), web::GetWebClient()->GetUserAgent(false)));
  if (command_line.HasSwitch(switches::kIOSTestingFixedHttpPort)) {
    globals_->testing_fixed_http_port =
        GetSwitchValueAsInt(command_line, switches::kIOSTestingFixedHttpPort);
  }
  if (command_line.HasSwitch(switches::kIOSTestingFixedHttpsPort)) {
    globals_->testing_fixed_https_port =
        GetSwitchValueAsInt(command_line, switches::kIOSTestingFixedHttpsPort);
  }
  ConfigureQuic();
  InitializeNetworkOptions();

  const version_info::Channel channel = ::GetChannel();
  if (channel == version_info::Channel::UNKNOWN ||
      channel == version_info::Channel::CANARY ||
      channel == version_info::Channel::DEV) {
    globals_->url_request_backoff_manager.reset(
        new net::URLRequestBackoffManager());
  }

  // InitSystemRequestContext turns right around and posts a task back
  // to the IO thread, so we can't let it run until we know the IO
  // thread has started.
  //
  // Note that since we are at WebThread::Init time, the UI thread
  // is blocked waiting for the thread to start.  Therefore, posting
  // this task to the main thread's message loop here is guaranteed to
  // get it onto the message loop while the IOSChromeIOThread object still
  // exists.  However, the message might not be processed on the UI
  // thread until after IOSChromeIOThread is gone, so use a weak pointer.
  web::WebThread::PostTask(
      web::WebThread::UI, FROM_HERE,
      base::Bind(&IOSChromeIOThread::InitSystemRequestContext,
                 weak_factory_.GetWeakPtr()));
}

void IOSChromeIOThread::CleanUp() {
  net::ShutdownNSSHttpIO();

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

void IOSChromeIOThread::InitializeNetworkOptions() {
  std::string group = base::FieldTrialList::FindFullName(kSpdyFieldTrialName);
  VariationParameters params;
  if (!variations::GetVariationParams(kSpdyFieldTrialName, &params)) {
    params.clear();
  }
  ConfigureSpdyGlobals(group, params, globals_);

  ConfigureAltSvcGlobals(
      base::FieldTrialList::FindFullName(kAltSvcFieldTrialName), globals_);

  ConfigureSSLTCPFastOpen();

  ConfigureNPNGlobals(base::FieldTrialList::FindFullName(kNpnTrialName),
                      globals_);

  // TODO(rch): Make the client socket factory a per-network session
  // instance, constructed from a NetworkSession::Params, to allow us
  // to move this option to IOSChromeIOThread::Globals &
  // HttpNetworkSession::Params.
}

void IOSChromeIOThread::ConfigureSSLTCPFastOpen() {
  const std::string trial_group =
      base::FieldTrialList::FindFullName(kTCPFastOpenFieldTrialName);
  if (trial_group == kTCPFastOpenHttpsEnabledGroupName)
    globals_->enable_tcp_fast_open_for_ssl.set(true);
}

// static
void IOSChromeIOThread::ConfigureSpdyGlobals(
    base::StringPiece spdy_trial_group,
    const VariationParameters& spdy_trial_params,
    IOSChromeIOThread::Globals* globals) {
  // No SPDY command-line flags have been specified. Examine trial groups.
  if (spdy_trial_group.starts_with(kSpdyFieldTrialHoldbackGroupNamePrefix)) {
    net::HttpStreamFactory::set_spdy_enabled(false);
    return;
  }
  if (spdy_trial_group.starts_with(kSpdyFieldTrialSpdy31GroupNamePrefix)) {
    globals->enable_spdy31.set(true);
    globals->enable_http2.set(false);
    return;
  }
  if (spdy_trial_group.starts_with(kSpdyFieldTrialSpdy4GroupNamePrefix)) {
    globals->enable_spdy31.set(true);
    globals->enable_http2.set(true);
    return;
  }
  if (spdy_trial_group.starts_with(kSpdyFieldTrialParametrizedPrefix)) {
    bool spdy_enabled = false;
    globals->enable_spdy31.set(false);
    globals->enable_http2.set(false);
    if (base::LowerCaseEqualsASCII(
            GetVariationParam(spdy_trial_params, "enable_http2"), "true")) {
      spdy_enabled = true;
      globals->enable_http2.set(true);
    }
    if (base::LowerCaseEqualsASCII(
            GetVariationParam(spdy_trial_params, "enable_spdy31"), "true")) {
      spdy_enabled = true;
      globals->enable_spdy31.set(true);
    }
    // TODO(bnc): https://crbug.com/521597
    // HttpStreamFactory::spdy_enabled_ is redundant with globals->enable_http2
    // and enable_spdy31, can it be eliminated?
    net::HttpStreamFactory::set_spdy_enabled(spdy_enabled);
    return;
  }

  // By default, enable HTTP/2.
  globals->enable_spdy31.set(true);
  globals->enable_http2.set(true);
}

// static
void IOSChromeIOThread::ConfigureAltSvcGlobals(
    base::StringPiece altsvc_trial_group,
    IOSChromeIOThread::Globals* globals) {
  if (altsvc_trial_group.starts_with(kAltSvcFieldTrialEnabledPrefix)) {
    globals->parse_alternative_services.set(true);
    return;
  }
  if (altsvc_trial_group.starts_with(kAltSvcFieldTrialDisabledPrefix)) {
    globals->parse_alternative_services.set(false);
  }
}

// static
void IOSChromeIOThread::ConfigureNPNGlobals(
    base::StringPiece npn_trial_group,
    IOSChromeIOThread::Globals* globals) {
  if (npn_trial_group.starts_with(kNpnTrialEnabledGroupNamePrefix)) {
    globals->enable_npn.set(true);
  } else if (npn_trial_group.starts_with(kNpnTrialDisabledGroupNamePrefix)) {
    globals->enable_npn.set(false);
  }
}

void IOSChromeIOThread::CreateDefaultAuthHandlerFactory() {
  std::vector<std::string> supported_schemes =
      base::SplitString(kSupportedAuthSchemes, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  globals_->http_auth_preferences.reset(
      new net::HttpAuthPreferences(supported_schemes, std::string()));
  globals_->http_auth_handler_factory =
      net::HttpAuthHandlerRegistryFactory::Create(
          globals_->http_auth_preferences.get(), globals_->host_resolver.get());
}

void IOSChromeIOThread::ClearHostCache() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  net::HostCache* host_cache = globals_->host_resolver->GetHostCache();
  if (host_cache)
    host_cache->clear();
}

void IOSChromeIOThread::InitializeNetworkSessionParams(
    net::HttpNetworkSession::Params* params) {
  InitializeNetworkSessionParamsFromGlobals(*globals_, params);
}

void IOSChromeIOThread::InitializeNetworkSessionParamsFromGlobals(
    const IOSChromeIOThread::Globals& globals,
    net::HttpNetworkSession::Params* params) {
  //  The next two properties of the params don't seem to be
  // elements of URLRequestContext, so they must be set here.
  params->ct_policy_enforcer = globals.ct_policy_enforcer.get();

  params->ignore_certificate_errors = false;
  params->testing_fixed_http_port = globals.testing_fixed_http_port;
  params->testing_fixed_https_port = globals.testing_fixed_https_port;
  globals.enable_tcp_fast_open_for_ssl.CopyToIfSet(
      &params->enable_tcp_fast_open_for_ssl);

  globals.enable_spdy31.CopyToIfSet(&params->enable_spdy31);
  globals.enable_http2.CopyToIfSet(&params->enable_http2);
  globals.parse_alternative_services.CopyToIfSet(
      &params->parse_alternative_services);
  globals.enable_alternative_service_with_different_host.CopyToIfSet(
      &params->enable_alternative_service_with_different_host);
  globals.alternative_service_probability_threshold.CopyToIfSet(
      &params->alternative_service_probability_threshold);

  globals.enable_npn.CopyToIfSet(&params->enable_npn);

  globals.enable_quic.CopyToIfSet(&params->enable_quic);
  globals.enable_quic_for_proxies.CopyToIfSet(&params->enable_quic_for_proxies);
  globals.quic_always_require_handshake_confirmation.CopyToIfSet(
      &params->quic_always_require_handshake_confirmation);
  globals.quic_disable_connection_pooling.CopyToIfSet(
      &params->quic_disable_connection_pooling);
  globals.quic_load_server_info_timeout_srtt_multiplier.CopyToIfSet(
      &params->quic_load_server_info_timeout_srtt_multiplier);
  globals.quic_enable_connection_racing.CopyToIfSet(
      &params->quic_enable_connection_racing);
  globals.quic_enable_non_blocking_io.CopyToIfSet(
      &params->quic_enable_non_blocking_io);
  globals.quic_prefer_aes.CopyToIfSet(&params->quic_prefer_aes);
  globals.quic_disable_disk_cache.CopyToIfSet(&params->quic_disable_disk_cache);
  globals.quic_max_number_of_lossy_connections.CopyToIfSet(
      &params->quic_max_number_of_lossy_connections);
  globals.quic_packet_loss_threshold.CopyToIfSet(
      &params->quic_packet_loss_threshold);
  globals.quic_socket_receive_buffer_size.CopyToIfSet(
      &params->quic_socket_receive_buffer_size);
  globals.quic_delay_tcp_race.CopyToIfSet(&params->quic_delay_tcp_race);
  params->enable_quic_port_selection = false;
  globals.quic_max_packet_length.CopyToIfSet(&params->quic_max_packet_length);
  globals.quic_user_agent_id.CopyToIfSet(&params->quic_user_agent_id);
  globals.quic_supported_versions.CopyToIfSet(&params->quic_supported_versions);
  params->quic_connection_options = globals.quic_connection_options;
  globals.quic_close_sessions_on_ip_change.CopyToIfSet(
      &params->quic_close_sessions_on_ip_change);

  params->enable_user_alternate_protocol_ports = false;
}

base::TimeTicks IOSChromeIOThread::creation_time() const {
  return creation_time_;
}

net::SSLConfigService* IOSChromeIOThread::GetSSLConfigService() {
  return ssl_config_service_manager_->Get();
}

void IOSChromeIOThread::ChangedToOnTheRecordOnIOThread() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);

  // Clear the host cache to avoid showing entries from the OTR session
  // in about:net-internals.
  ClearHostCache();
}

void IOSChromeIOThread::InitSystemRequestContext() {
  if (system_url_request_context_getter_.get())
    return;
  // If we're in unit_tests, IOSChromeIOThread may not be run.
  if (!web::WebThread::IsMessageLoopValid(web::WebThread::IO))
    return;
  system_proxy_config_service_ =
      ios::ProxyServiceFactory::CreateProxyConfigService(
          pref_proxy_config_tracker_.get());

  system_url_request_context_getter_ = new SystemURLRequestContextGetter(this);
  // Safe to post an unretained this pointer, since IOSChromeIOThread is
  // guaranteed to outlive the IO WebThread.
  web::WebThread::PostTask(
      web::WebThread::IO, FROM_HERE,
      base::Bind(&IOSChromeIOThread::InitSystemRequestContextOnIOThread,
                 base::Unretained(this)));
}

void IOSChromeIOThread::InitSystemRequestContextOnIOThread() {
  DCHECK_CURRENTLY_ON_WEB_THREAD(web::WebThread::IO);
  DCHECK(!globals_->system_proxy_service.get());
  DCHECK(system_proxy_config_service_.get());

  globals_->system_proxy_service = ios::ProxyServiceFactory::CreateProxyService(
      net_log_, nullptr, globals_->system_network_delegate.get(),
      std::move(system_proxy_config_service_), true /* quick_check_enabled */);

  globals_->system_request_context.reset(
      ConstructSystemRequestContext(globals_, net_log_));
}

void IOSChromeIOThread::ConfigureQuic() {
  // Always fetch the field trial group to ensure it is reported correctly.
  // The command line flags will be associated with a group that is reported
  // so long as trial is actually queried.
  std::string group = base::FieldTrialList::FindFullName(kQuicFieldTrialName);
  VariationParameters params;
  if (!variations::GetVariationParams(kQuicFieldTrialName, &params)) {
    params.clear();
  }

  ConfigureQuicGlobals(group, params, globals_);
}

void IOSChromeIOThread::ConfigureQuicGlobals(
    base::StringPiece quic_trial_group,
    const VariationParameters& quic_trial_params,
    IOSChromeIOThread::Globals* globals) {
  bool enable_quic = ShouldEnableQuic(quic_trial_group);
  globals->enable_quic.set(enable_quic);
  bool enable_quic_for_proxies = ShouldEnableQuicForProxies(quic_trial_group);
  globals->enable_quic_for_proxies.set(enable_quic_for_proxies);
  globals->enable_alternative_service_with_different_host.set(
      ShouldQuicEnableAlternativeServicesForDifferentHost(quic_trial_params));
  if (enable_quic) {
    globals->quic_always_require_handshake_confirmation.set(
        ShouldQuicAlwaysRequireHandshakeConfirmation(quic_trial_params));
    globals->quic_disable_connection_pooling.set(
        ShouldQuicDisableConnectionPooling(quic_trial_params));
    int receive_buffer_size = GetQuicSocketReceiveBufferSize(quic_trial_params);
    if (receive_buffer_size != 0) {
      globals->quic_socket_receive_buffer_size.set(receive_buffer_size);
    }
    globals->quic_delay_tcp_race.set(ShouldQuicDelayTcpRace(quic_trial_params));
    float load_server_info_timeout_srtt_multiplier =
        GetQuicLoadServerInfoTimeoutSrttMultiplier(quic_trial_params);
    if (load_server_info_timeout_srtt_multiplier != 0) {
      globals->quic_load_server_info_timeout_srtt_multiplier.set(
          load_server_info_timeout_srtt_multiplier);
    }
    globals->quic_enable_connection_racing.set(
        ShouldQuicEnableConnectionRacing(quic_trial_params));
    globals->quic_enable_non_blocking_io.set(
        ShouldQuicEnableNonBlockingIO(quic_trial_params));
    globals->quic_disable_disk_cache.set(
        ShouldQuicDisableDiskCache(quic_trial_params));
    globals->quic_prefer_aes.set(ShouldQuicPreferAes(quic_trial_params));
    int max_number_of_lossy_connections =
        GetQuicMaxNumberOfLossyConnections(quic_trial_params);
    if (max_number_of_lossy_connections != 0) {
      globals->quic_max_number_of_lossy_connections.set(
          max_number_of_lossy_connections);
    }
    float packet_loss_threshold = GetQuicPacketLossThreshold(quic_trial_params);
    if (packet_loss_threshold != 0)
      globals->quic_packet_loss_threshold.set(packet_loss_threshold);
    globals->quic_connection_options =
        GetQuicConnectionOptions(quic_trial_params);
    globals->quic_close_sessions_on_ip_change.set(
        ShouldQuicCloseSessionsOnIpChange(quic_trial_params));
  }

  size_t max_packet_length = GetQuicMaxPacketLength(quic_trial_params);
  if (max_packet_length != 0) {
    globals->quic_max_packet_length.set(max_packet_length);
  }

  std::string quic_user_agent_id = ::GetChannelString();
  if (!quic_user_agent_id.empty())
    quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(
      version_info::GetProductNameAndVersionForUserAgent());
  quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(web::BuildOSCpuInfo());
  globals->quic_user_agent_id.set(quic_user_agent_id);

  net::QuicVersion version = GetQuicVersion(quic_trial_params);
  if (version != net::QUIC_VERSION_UNSUPPORTED) {
    net::QuicVersionVector supported_versions;
    supported_versions.push_back(version);
    globals->quic_supported_versions.set(supported_versions);
  }

  double threshold =
      GetAlternativeProtocolProbabilityThreshold(quic_trial_params);
  if (threshold >= 0 && threshold <= 1) {
    globals->alternative_service_probability_threshold.set(threshold);
    globals->http_server_properties->SetAlternativeServiceProbabilityThreshold(
        threshold);
  }
}

bool IOSChromeIOThread::ShouldEnableQuic(base::StringPiece quic_trial_group) {
  return quic_trial_group.starts_with(kQuicFieldTrialEnabledGroupName) ||
         quic_trial_group.starts_with(kQuicFieldTrialHttpsEnabledGroupName);
}

bool IOSChromeIOThread::ShouldEnableQuicForProxies(
    base::StringPiece quic_trial_group) {
  return ShouldEnableQuic(quic_trial_group) ||
         ShouldEnableQuicForDataReductionProxy();
}

bool IOSChromeIOThread::ShouldEnableQuicForDataReductionProxy() {
  return data_reduction_proxy::params::IsIncludedInQuicFieldTrial();
}

net::QuicTagVector IOSChromeIOThread::GetQuicConnectionOptions(
    const VariationParameters& quic_trial_params) {
  VariationParameters::const_iterator it =
      quic_trial_params.find("connection_options");
  if (it == quic_trial_params.end()) {
    return net::QuicTagVector();
  }

  return net::QuicUtils::ParseQuicConnectionOptions(it->second);
}

double IOSChromeIOThread::GetAlternativeProtocolProbabilityThreshold(
    const VariationParameters& quic_trial_params) {
  double value;
  // TODO(bnc): Remove when new parameter name rolls out and server
  // configuration is changed.
  if (base::StringToDouble(
          GetVariationParam(quic_trial_params,
                            "alternate_protocol_probability_threshold"),
          &value)) {
    return value;
  }
  if (base::StringToDouble(
          GetVariationParam(quic_trial_params,
                            "alternative_service_probability_threshold"),
          &value)) {
    return value;
  }
  return -1;
}

bool IOSChromeIOThread::ShouldQuicAlwaysRequireHandshakeConfirmation(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params,
                        "always_require_handshake_confirmation"),
      "true");
}

bool IOSChromeIOThread::ShouldQuicDisableConnectionPooling(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_connection_pooling"),
      "true");
}

float IOSChromeIOThread::GetQuicLoadServerInfoTimeoutSrttMultiplier(
    const VariationParameters& quic_trial_params) {
  double value;
  if (base::StringToDouble(
          GetVariationParam(quic_trial_params, "load_server_info_time_to_srtt"),
          &value)) {
    return static_cast<float>(value);
  }
  return 0.0f;
}

bool IOSChromeIOThread::ShouldQuicEnableConnectionRacing(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "enable_connection_racing"), "true");
}

bool IOSChromeIOThread::ShouldQuicEnableNonBlockingIO(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "enable_non_blocking_io"), "true");
}

bool IOSChromeIOThread::ShouldQuicDisableDiskCache(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_disk_cache"), "true");
}

bool IOSChromeIOThread::ShouldQuicPreferAes(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "prefer_aes"), "true");
}

bool IOSChromeIOThread::ShouldQuicEnableAlternativeServicesForDifferentHost(
    const VariationParameters& quic_trial_params) {
  // TODO(bnc): Remove inaccurately named "use_alternative_services" parameter.
  return base::LowerCaseEqualsASCII(
             GetVariationParam(quic_trial_params, "use_alternative_services"),
             "true") ||
         base::LowerCaseEqualsASCII(
             GetVariationParam(
                 quic_trial_params,
                 "enable_alternative_service_with_different_host"),
             "true");
}

int IOSChromeIOThread::GetQuicMaxNumberOfLossyConnections(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(GetVariationParam(quic_trial_params,
                                          "max_number_of_lossy_connections"),
                        &value)) {
    return value;
  }
  return 0;
}

float IOSChromeIOThread::GetQuicPacketLossThreshold(
    const VariationParameters& quic_trial_params) {
  double value;
  if (base::StringToDouble(
          GetVariationParam(quic_trial_params, "packet_loss_threshold"),
          &value)) {
    return static_cast<float>(value);
  }
  return 0.0f;
}

int IOSChromeIOThread::GetQuicSocketReceiveBufferSize(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(
          GetVariationParam(quic_trial_params, "receive_buffer_size"),
          &value)) {
    return value;
  }
  return 0;
}

bool IOSChromeIOThread::ShouldQuicDelayTcpRace(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "delay_tcp_race"), "true");
}

bool IOSChromeIOThread::ShouldQuicCloseSessionsOnIpChange(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "close_sessions_on_ip_change"),
      "true");
}

size_t IOSChromeIOThread::GetQuicMaxPacketLength(
    const VariationParameters& quic_trial_params) {
  unsigned value;
  if (base::StringToUint(
          GetVariationParam(quic_trial_params, "max_packet_length"), &value)) {
    return value;
  }
  return 0;
}

net::QuicVersion IOSChromeIOThread::GetQuicVersion(
    const VariationParameters& quic_trial_params) {
  return ParseQuicVersion(GetVariationParam(quic_trial_params, "quic_version"));
}

net::QuicVersion IOSChromeIOThread::ParseQuicVersion(
    const std::string& quic_version) {
  net::QuicVersionVector supported_versions = net::QuicSupportedVersions();
  for (size_t i = 0; i < supported_versions.size(); ++i) {
    net::QuicVersion version = supported_versions[i];
    if (net::QuicVersionToString(version) == quic_version) {
      return version;
    }
  }

  return net::QUIC_VERSION_UNSUPPORTED;
}

net::URLRequestContext* IOSChromeIOThread::ConstructSystemRequestContext(
    IOSChromeIOThread::Globals* globals,
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

  net::URLRequestJobFactoryImpl* system_job_factory =
      new net::URLRequestJobFactoryImpl();
  // Data URLs are always loaded through the system request context on iOS
  // (due to UIWebView limitations).
  bool set_protocol = system_job_factory->SetProtocolHandler(
      url::kDataScheme, make_scoped_ptr(new net::DataProtocolHandler()));
  DCHECK(set_protocol);
  globals->system_url_request_job_factory.reset(system_job_factory);
  context->set_job_factory(globals->system_url_request_job_factory.get());

  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_channel_id_service(globals->system_channel_id_service.get());
  context->set_network_delegate(globals->system_network_delegate.get());
  context->set_http_user_agent_settings(
      globals->http_user_agent_settings.get());
  context->set_network_quality_estimator(
      globals->network_quality_estimator.get());
  context->set_backoff_manager(globals->url_request_backoff_manager.get());

  context->set_http_server_properties(
      globals->http_server_properties->GetWeakPtr());

  net::HttpNetworkSession::Params system_params;
  InitializeNetworkSessionParamsFromGlobals(*globals, &system_params);
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(
      context, &system_params);

  globals->system_http_network_session.reset(
      new net::HttpNetworkSession(system_params));
  globals->system_http_transaction_factory.reset(
      new net::HttpNetworkLayer(globals->system_http_network_session.get()));
  context->set_http_transaction_factory(
      globals->system_http_transaction_factory.get());

  return context;
}
