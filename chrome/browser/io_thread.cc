// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"

#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_tracker.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/async_dns_field_trial.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_prefs.h"
#include "components/policy/core/common/policy_service.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/net_util.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/quic_protocol.h"
#include "net/socket/tcp_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/ftp_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "url/url_constants.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "policy/policy_constants.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/event_router_forwarder.h"
#endif

#if !defined(USE_OPENSSL)
#include "net/cert/ct_log_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#endif

#if defined(USE_NSS) || defined(OS_IOS)
#include "net/ocsp/nss_ocsp.h"
#endif

#if defined(SPDY_PROXY_AUTH_ORIGIN)
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_protocol.h"
#endif  // defined(SPDY_PROXY_AUTH_ORIGIN)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/net/cert_verify_proc_chromeos.h"
#include "chromeos/network/host_resolver_impl_chromeos.h"
#endif

using content::BrowserThread;

class SafeBrowsingURLRequestContext;

// The IOThread object must outlive any tasks posted to the IO thread before the
// Quit task, so base::Bind() calls are not refcounted.

namespace {

const char kQuicFieldTrialName[] = "QUIC";
const char kQuicFieldTrialEnabledGroupName[] = "Enabled";
const char kQuicFieldTrialHttpsEnabledGroupName[] = "HttpsEnabled";
const char kQuicFieldTrialPacketLengthSuffix[] = "BytePackets";
const char kQuicFieldTrialPacingSuffix[] = "WithPacing";
const char kQuicFieldTrialTimeBasedLossDetectionSuffix[] =
    "WithTimeBasedLossDetection";

// The SPDY trial composes two different trial plus control groups:
//  * A "holdback" group with SPDY disabled, and corresponding control
//  (SPDY/3.1). The primary purpose of the holdback group is to encourage site
//  operators to do feature detection rather than UA-sniffing. As such, this
//  trial runs continuously.
//  * A SPDY/4 experiment, for SPDY/4 (aka HTTP/2) vs SPDY/3.1 comparisons and
//  eventual SPDY/4 deployment.
const char kSpdyFieldTrialName[] = "SPDY";
const char kSpdyFieldTrialHoldbackGroupName[] = "SpdyDisabled";
const char kSpdyFieldTrialHoldbackControlGroupName[] = "Control";
const char kSpdyFieldTrialSpdy4GroupName[] = "Spdy4Enabled";
const char kSpdyFieldTrialSpdy4ControlGroupName[] = "Spdy4Control";

#if defined(OS_MACOSX) && !defined(OS_IOS)
void ObserveKeychainEvents() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  net::CertDatabase::GetInstance()->SetMessageLoopForKeychainEvents();
}
#endif

// Used for the "system" URLRequestContext.
class SystemURLRequestContext : public net::URLRequestContext {
 public:
  SystemURLRequestContext() {
#if defined(USE_NSS) || defined(OS_IOS)
    net::SetURLRequestContextForNSSHttpIO(this);
#endif
  }

 private:
  virtual ~SystemURLRequestContext() {
    AssertNoURLRequests();
#if defined(USE_NSS) || defined(OS_IOS)
    net::SetURLRequestContextForNSSHttpIO(NULL);
#endif
  }
};

scoped_ptr<net::HostResolver> CreateGlobalHostResolver(net::NetLog* net_log) {
  TRACE_EVENT0("startup", "IOThread::CreateGlobalHostResolver");
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  net::HostResolver::Options options;

  // Use the concurrency override from the command-line, if any.
  if (command_line.HasSwitch(switches::kHostResolverParallelism)) {
    std::string s =
        command_line.GetSwitchValueASCII(switches::kHostResolverParallelism);

    // Parse the switch (it should be a positive integer formatted as decimal).
    int n;
    if (base::StringToInt(s, &n) && n > 0) {
      options.max_concurrent_resolves = static_cast<size_t>(n);
    } else {
      LOG(ERROR) << "Invalid switch for host resolver parallelism: " << s;
    }
  }

  // Use the retry attempts override from the command-line, if any.
  if (command_line.HasSwitch(switches::kHostResolverRetryAttempts)) {
    std::string s =
        command_line.GetSwitchValueASCII(switches::kHostResolverRetryAttempts);
    // Parse the switch (it should be a non-negative integer).
    int n;
    if (base::StringToInt(s, &n) && n >= 0) {
      options.max_retry_attempts = static_cast<size_t>(n);
    } else {
      LOG(ERROR) << "Invalid switch for host resolver retry attempts: " << s;
    }
  }

  scoped_ptr<net::HostResolver> global_host_resolver;
#if defined OS_CHROMEOS
  global_host_resolver =
      chromeos::HostResolverImplChromeOS::CreateSystemResolver(options,
                                                               net_log);
#else
  global_host_resolver =
      net::HostResolver::CreateSystemResolver(options, net_log);
#endif

  // Determine if we should disable IPv6 support.
  if (command_line.HasSwitch(switches::kEnableIPv6)) {
    // Disable IPv6 probing.
    global_host_resolver->SetDefaultAddressFamily(
        net::ADDRESS_FAMILY_UNSPECIFIED);
  } else if (command_line.HasSwitch(switches::kDisableIPv6)) {
    global_host_resolver->SetDefaultAddressFamily(net::ADDRESS_FAMILY_IPV4);
  }

  // If hostname remappings were specified on the command-line, layer these
  // rules on top of the real host resolver. This allows forwarding all requests
  // through a designated test server.
  if (!command_line.HasSwitch(switches::kHostResolverRules))
    return global_host_resolver.PassAs<net::HostResolver>();

  scoped_ptr<net::MappedHostResolver> remapped_resolver(
      new net::MappedHostResolver(global_host_resolver.Pass()));
  remapped_resolver->SetRulesFromString(
      command_line.GetSwitchValueASCII(switches::kHostResolverRules));
  return remapped_resolver.PassAs<net::HostResolver>();
}

// TODO(willchan): Remove proxy script fetcher context since it's not necessary
// now that I got rid of refcounting URLRequestContexts.
// See IOThread::Globals for details.
net::URLRequestContext*
ConstructProxyScriptFetcherContext(IOThread::Globals* globals,
                                   net::NetLog* net_log) {
  net::URLRequestContext* context = new net::URLRequestContext;
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_transport_security_state(
      globals->transport_security_state.get());
  context->set_cert_transparency_verifier(
      globals->cert_transparency_verifier.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->proxy_script_fetcher_proxy_service.get());
  context->set_http_transaction_factory(
      globals->proxy_script_fetcher_http_transaction_factory.get());
  context->set_job_factory(
      globals->proxy_script_fetcher_url_request_job_factory.get());
  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_channel_id_service(
      globals->system_channel_id_service.get());
  context->set_network_delegate(globals->system_network_delegate.get());
  context->set_http_user_agent_settings(
      globals->http_user_agent_settings.get());
  // TODO(rtenneti): We should probably use HttpServerPropertiesManager for the
  // system URLRequestContext too. There's no reason this should be tied to a
  // profile.
  return context;
}

net::URLRequestContext*
ConstructSystemRequestContext(IOThread::Globals* globals,
                              net::NetLog* net_log) {
  net::URLRequestContext* context = new SystemURLRequestContext;
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_transport_security_state(
      globals->transport_security_state.get());
  context->set_cert_transparency_verifier(
      globals->cert_transparency_verifier.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->system_proxy_service.get());
  context->set_http_transaction_factory(
      globals->system_http_transaction_factory.get());
  context->set_job_factory(globals->system_url_request_job_factory.get());
  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_channel_id_service(
      globals->system_channel_id_service.get());
  context->set_throttler_manager(globals->throttler_manager.get());
  context->set_network_delegate(globals->system_network_delegate.get());
  context->set_http_user_agent_settings(
      globals->http_user_agent_settings.get());
  return context;
}

int GetSwitchValueAsInt(const CommandLine& command_line,
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

class IOThread::LoggingNetworkChangeObserver
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

  virtual ~LoggingNetworkChangeObserver() {
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  // NetworkChangeNotifier::IPAddressObserver implementation.
  virtual void OnIPAddressChanged() OVERRIDE {
    VLOG(1) << "Observed a change to the network IP addresses";

    net_log_->AddGlobalEntry(net::NetLog::TYPE_NETWORK_IP_ADDRESSES_CHANGED);
  }

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE {
    std::string type_as_string =
        net::NetworkChangeNotifier::ConnectionTypeToString(type);

    VLOG(1) << "Observed a change to network connectivity state "
            << type_as_string;

    net_log_->AddGlobalEntry(
        net::NetLog::TYPE_NETWORK_CONNECTIVITY_CHANGED,
        net::NetLog::StringCallback("new_connection_type", &type_as_string));
  }

  // NetworkChangeNotifier::NetworkChangeObserver implementation.
  virtual void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE {
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
  explicit SystemURLRequestContextGetter(IOThread* io_thread);

  // Implementation for net::UrlRequestContextGetter.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE;

 protected:
  virtual ~SystemURLRequestContextGetter();

 private:
  IOThread* const io_thread_;  // Weak pointer, owned by BrowserProcess.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  base::debug::LeakTracker<SystemURLRequestContextGetter> leak_tracker_;
};

SystemURLRequestContextGetter::SystemURLRequestContextGetter(
    IOThread* io_thread)
    : io_thread_(io_thread),
      network_task_runner_(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)) {
}

SystemURLRequestContextGetter::~SystemURLRequestContextGetter() {}

net::URLRequestContext* SystemURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(io_thread_->globals()->system_request_context.get());

  return io_thread_->globals()->system_request_context.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SystemURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

IOThread::Globals::
SystemRequestContextLeakChecker::SystemRequestContextLeakChecker(
    Globals* globals)
    : globals_(globals) {
  DCHECK(globals_);
}

IOThread::Globals::
SystemRequestContextLeakChecker::~SystemRequestContextLeakChecker() {
  if (globals_->system_request_context.get())
    globals_->system_request_context->AssertNoURLRequests();
}

IOThread::Globals::Globals()
    : system_request_context_leak_checker(this),
      enable_ssl_connect_job_waiting(false),
      ignore_certificate_errors(false),
      testing_fixed_http_port(0),
      testing_fixed_https_port(0),
      enable_user_alternate_protocol_ports(false) {
}

IOThread::Globals::~Globals() {}

// |local_state| is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOThread more flexible for testing.
IOThread::IOThread(
    PrefService* local_state,
    policy::PolicyService* policy_service,
    ChromeNetLog* net_log,
    extensions::EventRouterForwarder* extension_event_router_forwarder)
    : net_log_(net_log),
#if defined(ENABLE_EXTENSIONS)
      extension_event_router_forwarder_(extension_event_router_forwarder),
#endif
      globals_(NULL),
      is_spdy_disabled_by_policy_(false),
      weak_factory_(this),
      creation_time_(base::TimeTicks::Now()) {
  auth_schemes_ = local_state->GetString(prefs::kAuthSchemes);
  negotiate_disable_cname_lookup_ = local_state->GetBoolean(
      prefs::kDisableAuthNegotiateCnameLookup);
  negotiate_enable_port_ = local_state->GetBoolean(
      prefs::kEnableAuthNegotiatePort);
  auth_server_whitelist_ = local_state->GetString(prefs::kAuthServerWhitelist);
  auth_delegate_whitelist_ = local_state->GetString(
      prefs::kAuthNegotiateDelegateWhitelist);
  gssapi_library_name_ = local_state->GetString(prefs::kGSSAPILibraryName);
  pref_proxy_config_tracker_.reset(
      ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
          local_state));
  ChromeNetworkDelegate::InitializePrefsOnUIThread(
      &system_enable_referrers_,
      NULL,
      NULL,
      local_state);
  ssl_config_service_manager_.reset(
      SSLConfigServiceManager::CreateDefaultManager(local_state));

  base::Value* dns_client_enabled_default = new base::FundamentalValue(
      chrome_browser_net::ConfigureAsyncDnsFieldTrial());
  local_state->SetDefaultPrefValue(prefs::kBuiltInDnsClientEnabled,
                                   dns_client_enabled_default);

  dns_client_enabled_.Init(prefs::kBuiltInDnsClientEnabled,
                           local_state,
                           base::Bind(&IOThread::UpdateDnsClientEnabled,
                                      base::Unretained(this)));
  dns_client_enabled_.MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));

  quick_check_enabled_.Init(prefs::kQuickCheckEnabled,
                            local_state);
  quick_check_enabled_.MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));

#if defined(ENABLE_CONFIGURATION_POLICY)
  is_spdy_disabled_by_policy_ = policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string())).Get(
          policy::key::kDisableSpdy) != NULL;
#endif  // ENABLE_CONFIGURATION_POLICY

  BrowserThread::SetDelegate(BrowserThread::IO, this);
}

IOThread::~IOThread() {
  // This isn't needed for production code, but in tests, IOThread may
  // be multiply constructed.
  BrowserThread::SetDelegate(BrowserThread::IO, NULL);

  pref_proxy_config_tracker_->DetachFromPrefService();
  DCHECK(!globals_);
}

IOThread::Globals* IOThread::globals() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return globals_;
}

void IOThread::SetGlobalsForTesting(Globals* globals) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!globals || !globals_);
  globals_ = globals;
}

ChromeNetLog* IOThread::net_log() {
  return net_log_;
}

void IOThread::ChangedToOnTheRecord() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&IOThread::ChangedToOnTheRecordOnIOThread,
                 base::Unretained(this)));
}

net::URLRequestContextGetter* IOThread::system_url_request_context_getter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!system_url_request_context_getter_.get()) {
    InitSystemRequestContext();
  }
  return system_url_request_context_getter_.get();
}

void IOThread::Init() {
  // Prefer to use InitAsync unless you need initialization to block
  // the UI thread
}

void IOThread::InitAsync() {
  TRACE_EVENT0("startup", "IOThread::InitAsync");
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

#if defined(USE_NSS) || defined(OS_IOS)
  net::SetMessageLoopForNSSHttpIO();
#endif

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  DCHECK(!globals_);
  globals_ = new Globals;

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(
      new LoggingNetworkChangeObserver(net_log_));

  // Setup the HistogramWatcher to run on the IO thread.
  net::NetworkChangeNotifier::InitHistogramWatcher();

#if defined(ENABLE_EXTENSIONS)
  globals_->extension_event_router_forwarder =
      extension_event_router_forwarder_;
#endif

  ChromeNetworkDelegate* network_delegate =
      new ChromeNetworkDelegate(extension_event_router_forwarder(),
                                &system_enable_referrers_);

  if (command_line.HasSwitch(switches::kEnableClientHints))
    network_delegate->SetEnableClientHints();

#if defined(ENABLE_EXTENSIONS)
  if (command_line.HasSwitch(switches::kDisableExtensionsHttpThrottling))
    network_delegate->NeverThrottleRequests();
#endif

  globals_->system_network_delegate.reset(network_delegate);
  globals_->host_resolver = CreateGlobalHostResolver(net_log_);
  UpdateDnsClientEnabled();
#if defined(OS_CHROMEOS)
  // Creates a CertVerifyProc that doesn't allow any profile-provided certs.
  globals_->cert_verifier.reset(new net::MultiThreadedCertVerifier(
      new chromeos::CertVerifyProcChromeOS()));
#else
  globals_->cert_verifier.reset(new net::MultiThreadedCertVerifier(
      net::CertVerifyProc::CreateDefault()));
#endif

    globals_->transport_security_state.reset(new net::TransportSecurityState());
#if !defined(USE_OPENSSL)
  // For now, Certificate Transparency is only implemented for platforms
  // that use NSS.
  net::MultiLogCTVerifier* ct_verifier = new net::MultiLogCTVerifier();
  globals_->cert_transparency_verifier.reset(ct_verifier);

  // Add built-in logs
  ct_verifier->AddLogs(net::ct::CreateLogVerifiersForKnownLogs());

  // Add logs from command line
  if (command_line.HasSwitch(switches::kCertificateTransparencyLog)) {
    std::string switch_value = command_line.GetSwitchValueASCII(
        switches::kCertificateTransparencyLog);
    std::vector<std::string> logs;
    base::SplitString(switch_value, ',', &logs);
    for (std::vector<std::string>::iterator it = logs.begin(); it != logs.end();
         ++it) {
      const std::string& curr_log = *it;
      size_t delim_pos = curr_log.find(":");
      CHECK(delim_pos != std::string::npos)
          << "CT log description not provided (switch format"
             " is 'description:base64_key')";
      std::string log_description(curr_log.substr(0, delim_pos));
      std::string ct_public_key_data;
      CHECK(base::Base64Decode(curr_log.substr(delim_pos + 1),
                               &ct_public_key_data))
          << "Unable to decode CT public key.";
      scoped_ptr<net::CTLogVerifier> external_log_verifier(
          net::CTLogVerifier::Create(ct_public_key_data, log_description));
      CHECK(external_log_verifier) << "Unable to parse CT public key.";
      VLOG(1) << "Adding log with description " << log_description;
      ct_verifier->AddLog(external_log_verifier.Pass());
    }
  }
#else
  if (command_line.HasSwitch(switches::kCertificateTransparencyLog)) {
    LOG(DFATAL) << "Certificate Transparency is not yet supported in Chrome "
                   "builds using OpenSSL.";
  }
#endif
  globals_->ssl_config_service = GetSSLConfigService();

#if defined(SPDY_PROXY_AUTH_ORIGIN)
  int drp_flags = 0;
  if (data_reduction_proxy::DataReductionProxyParams::
          IsIncludedInFieldTrial()) {
    drp_flags |=
        (data_reduction_proxy::DataReductionProxyParams::kAllowed |
         data_reduction_proxy::DataReductionProxyParams::kFallbackAllowed);
  }
  if (data_reduction_proxy::DataReductionProxyParams::
          IsIncludedInAlternativeFieldTrial()) {
    drp_flags |=
        data_reduction_proxy::DataReductionProxyParams::kAlternativeAllowed;
  }
  if (data_reduction_proxy::DataReductionProxyParams::
          IsIncludedInPromoFieldTrial())
    drp_flags |= data_reduction_proxy::DataReductionProxyParams::kPromoAllowed;
  if (data_reduction_proxy::DataReductionProxyParams::
          IsIncludedInHoldbackFieldTrial())
    drp_flags |= data_reduction_proxy::DataReductionProxyParams::kHoldback;
  globals_->data_reduction_proxy_params.reset(
      new data_reduction_proxy::DataReductionProxyParams(drp_flags));
  globals_->data_reduction_proxy_auth_request_handler.reset(
      new data_reduction_proxy::DataReductionProxyAuthRequestHandler(
          DataReductionProxyChromeSettings::GetClient(),
          DataReductionProxyChromeSettings::GetBuildAndPatchNumber(),
          globals_->data_reduction_proxy_params.get(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
  // This is the same as in ProfileImplIOData except that we do not collect
  // usage stats.
  network_delegate->set_data_reduction_proxy_params(
      globals_->data_reduction_proxy_params.get());
  network_delegate->set_data_reduction_proxy_auth_request_handler(
      globals_->data_reduction_proxy_auth_request_handler.get());
  network_delegate->set_on_resolve_proxy_handler(
      base::Bind(data_reduction_proxy::OnResolveProxyHandler));
#endif  // defined(SPDY_PROXY_AUTH_ORIGIN)

  globals_->http_auth_handler_factory.reset(CreateDefaultAuthHandlerFactory(
      globals_->host_resolver.get()));
  globals_->http_server_properties.reset(new net::HttpServerPropertiesImpl());
  // For the ProxyScriptFetcher, we use a direct ProxyService.
  globals_->proxy_script_fetcher_proxy_service.reset(
      net::ProxyService::CreateDirectWithNetLog(net_log_));
  // In-memory cookie store.
  globals_->system_cookie_store =
        content::CreateCookieStore(content::CookieStoreConfig());
  // In-memory channel ID store.
  globals_->system_channel_id_service.reset(
      new net::ChannelIDService(
          new net::DefaultChannelIDStore(NULL),
          base::WorkerPool::GetTaskRunner(true)));
  globals_->dns_probe_service.reset(new chrome_browser_net::DnsProbeService());
  globals_->host_mapping_rules.reset(new net::HostMappingRules());
  globals_->http_user_agent_settings.reset(
      new net::StaticHttpUserAgentSettings(std::string(), GetUserAgent()));
  if (command_line.HasSwitch(switches::kHostRules)) {
    TRACE_EVENT_BEGIN0("startup", "IOThread::InitAsync:SetRulesFromString");
    globals_->host_mapping_rules->SetRulesFromString(
        command_line.GetSwitchValueASCII(switches::kHostRules));
    TRACE_EVENT_END0("startup", "IOThread::InitAsync:SetRulesFromString");
  }
  if (command_line.HasSwitch(switches::kEnableSSLConnectJobWaiting))
    globals_->enable_ssl_connect_job_waiting = true;
  if (command_line.HasSwitch(switches::kIgnoreCertificateErrors))
    globals_->ignore_certificate_errors = true;
  if (command_line.HasSwitch(switches::kTestingFixedHttpPort)) {
    globals_->testing_fixed_http_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpPort);
  }
  if (command_line.HasSwitch(switches::kTestingFixedHttpsPort)) {
    globals_->testing_fixed_https_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpsPort);
  }
  ConfigureQuic(command_line);
  if (command_line.HasSwitch(
          switches::kEnableUserAlternateProtocolPorts)) {
    globals_->enable_user_alternate_protocol_ports = true;
  }
  InitializeNetworkOptions(command_line);

  net::HttpNetworkSession::Params session_params;
  InitializeNetworkSessionParams(&session_params);
  session_params.net_log = net_log_;
  session_params.proxy_service =
      globals_->proxy_script_fetcher_proxy_service.get();

  TRACE_EVENT_BEGIN0("startup", "IOThread::InitAsync:HttpNetworkSession");
  scoped_refptr<net::HttpNetworkSession> network_session(
      new net::HttpNetworkSession(session_params));
  globals_->proxy_script_fetcher_http_transaction_factory
      .reset(new net::HttpNetworkLayer(network_session.get()));
  TRACE_EVENT_END0("startup", "IOThread::InitAsync:HttpNetworkSession");
  scoped_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());
  job_factory->SetProtocolHandler(url::kDataScheme,
                                  new net::DataProtocolHandler());
  job_factory->SetProtocolHandler(
      url::kFileScheme,
      new net::FileProtocolHandler(
          content::BrowserThread::GetBlockingPool()->
              GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)));
#if !defined(DISABLE_FTP_SUPPORT)
  globals_->proxy_script_fetcher_ftp_transaction_factory.reset(
      new net::FtpNetworkLayer(globals_->host_resolver.get()));
  job_factory->SetProtocolHandler(
      url::kFtpScheme,
      new net::FtpProtocolHandler(
          globals_->proxy_script_fetcher_ftp_transaction_factory.get()));
#endif
  globals_->proxy_script_fetcher_url_request_job_factory =
      job_factory.PassAs<net::URLRequestJobFactory>();

  globals_->throttler_manager.reset(new net::URLRequestThrottlerManager());
  globals_->throttler_manager->set_net_log(net_log_);
  // Always done in production, disabled only for unit tests.
  globals_->throttler_manager->set_enable_thread_checks(true);

  globals_->proxy_script_fetcher_context.reset(
      ConstructProxyScriptFetcherContext(globals_, net_log_));

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Start observing Keychain events. This needs to be done on the UI thread,
  // as Keychain services requires a CFRunLoop.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&ObserveKeychainEvents));
#endif

  // InitSystemRequestContext turns right around and posts a task back
  // to the IO thread, so we can't let it run until we know the IO
  // thread has started.
  //
  // Note that since we are at BrowserThread::Init time, the UI thread
  // is blocked waiting for the thread to start.  Therefore, posting
  // this task to the main thread's message loop here is guaranteed to
  // get it onto the message loop while the IOThread object still
  // exists.  However, the message might not be processed on the UI
  // thread until after IOThread is gone, so use a weak pointer.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&IOThread::InitSystemRequestContext,
                                     weak_factory_.GetWeakPtr()));
}

void IOThread::CleanUp() {
  base::debug::LeakTracker<SafeBrowsingURLRequestContext>::CheckForLeaks();

#if defined(USE_NSS) || defined(OS_IOS)
  net::ShutdownNSSHttpIO();
#endif

  system_url_request_context_getter_ = NULL;

  // Release objects that the net::URLRequestContext could have been pointing
  // to.

  // Shutdown the HistogramWatcher on the IO thread.
  net::NetworkChangeNotifier::ShutdownHistogramWatcher();

  // This must be reset before the ChromeNetLog is destroyed.
  network_change_observer_.reset();

  system_proxy_config_service_.reset();

  delete globals_;
  globals_ = NULL;

  base::debug::LeakTracker<SystemURLRequestContextGetter>::CheckForLeaks();
}

void IOThread::InitializeNetworkOptions(const CommandLine& command_line) {
  // Only handle use-spdy command line flags if "spdy.disabled" preference is
  // not disabled via policy.
  if (is_spdy_disabled_by_policy_) {
    base::FieldTrial* trial = base::FieldTrialList::Find(kSpdyFieldTrialName);
    if (trial)
      trial->Disable();
  } else {
    if (command_line.HasSwitch(switches::kTrustedSpdyProxy)) {
      globals_->trusted_spdy_proxy.set(
          command_line.GetSwitchValueASCII(switches::kTrustedSpdyProxy));
    }
    if (command_line.HasSwitch(switches::kIgnoreUrlFetcherCertRequests))
      net::URLFetcher::SetIgnoreCertificateRequests(true);

    if (command_line.HasSwitch(switches::kUseSpdy)) {
      std::string spdy_mode =
          command_line.GetSwitchValueASCII(switches::kUseSpdy);
      EnableSpdy(spdy_mode);
    } else if (command_line.HasSwitch(switches::kEnableSpdy4)) {
      globals_->next_protos = net::NextProtosSpdy4Http2();
      globals_->use_alternate_protocols.set(true);
    } else if (command_line.HasSwitch(switches::kDisableSpdy31)) {
      globals_->next_protos = net::NextProtosSpdy3();
      globals_->use_alternate_protocols.set(true);
    } else if (command_line.HasSwitch(switches::kEnableNpnHttpOnly)) {
      globals_->next_protos = net::NextProtosHttpOnly();
      globals_->use_alternate_protocols.set(false);
    } else if (command_line.HasSwitch(switches::kEnableWebSocketOverSpdy)) {
      // Use the current SPDY default (SPDY/3.1).
      globals_->next_protos = net::NextProtosSpdy31();
      globals_->use_alternate_protocols.set(true);
    } else {
      // No SPDY command-line flags have been specified. Examine trial groups.
      ConfigureSpdyFromTrial(
          base::FieldTrialList::FindFullName(kSpdyFieldTrialName), globals_);
    }

    if (command_line.HasSwitch(switches::kEnableWebSocketOverSpdy))
      globals_->enable_websocket_over_spdy.set(true);
  }

  // TODO(rch): Make the client socket factory a per-network session
  // instance, constructed from a NetworkSession::Params, to allow us
  // to move this option to IOThread::Globals &
  // HttpNetworkSession::Params.
  if (command_line.HasSwitch(switches::kEnableTcpFastOpen))
    net::SetTCPFastOpenEnabled(true);
}

void IOThread::ConfigureSpdyFromTrial(const std::string& spdy_trial_group,
                                      Globals* globals) {
  if (spdy_trial_group == kSpdyFieldTrialHoldbackGroupName) {
    // TODO(jgraettinger): Use net::NextProtosHttpOnly() instead?
    net::HttpStreamFactory::set_spdy_enabled(false);
  } else if (spdy_trial_group == kSpdyFieldTrialHoldbackControlGroupName) {
    // Use the current SPDY default (SPDY/3.1).
    globals->next_protos = net::NextProtosSpdy31();
    globals->use_alternate_protocols.set(true);
  } else if (spdy_trial_group == kSpdyFieldTrialSpdy4GroupName) {
    globals->next_protos = net::NextProtosSpdy4Http2();
    globals->use_alternate_protocols.set(true);
  } else if (spdy_trial_group == kSpdyFieldTrialSpdy4ControlGroupName) {
    // This control group is pinned at SPDY/3.1.
    globals->next_protos = net::NextProtosSpdy31();
    globals->use_alternate_protocols.set(true);
  } else {
    // Use the current SPDY default (SPDY/3.1).
    globals->next_protos = net::NextProtosSpdy31();
    globals->use_alternate_protocols.set(true);
  }
}

void IOThread::EnableSpdy(const std::string& mode) {
  static const char kOff[] = "off";
  static const char kSSL[] = "ssl";
  static const char kDisableSSL[] = "no-ssl";
  static const char kDisablePing[] = "no-ping";
  static const char kExclude[] = "exclude";  // Hosts to exclude
  static const char kDisableCompression[] = "no-compress";
  static const char kDisableAltProtocols[] = "no-alt-protocols";
  static const char kForceAltProtocols[] = "force-alt-protocols";
  static const char kSingleDomain[] = "single-domain";

  static const char kInitialMaxConcurrentStreams[] = "init-max-streams";

  std::vector<std::string> spdy_options;
  base::SplitString(mode, ',', &spdy_options);

  for (std::vector<std::string>::iterator it = spdy_options.begin();
       it != spdy_options.end(); ++it) {
    const std::string& element = *it;
    std::vector<std::string> name_value;
    base::SplitString(element, '=', &name_value);
    const std::string& option =
        name_value.size() > 0 ? name_value[0] : std::string();
    const std::string value =
        name_value.size() > 1 ? name_value[1] : std::string();

    if (option == kOff) {
      net::HttpStreamFactory::set_spdy_enabled(false);
    } else if (option == kDisableSSL) {
      globals_->spdy_default_protocol.set(net::kProtoSPDY3);
      globals_->force_spdy_over_ssl.set(false);
      globals_->force_spdy_always.set(true);
    } else if (option == kSSL) {
      globals_->spdy_default_protocol.set(net::kProtoSPDY3);
      globals_->force_spdy_over_ssl.set(true);
      globals_->force_spdy_always.set(true);
    } else if (option == kDisablePing) {
      globals_->enable_spdy_ping_based_connection_checking.set(false);
    } else if (option == kExclude) {
      globals_->forced_spdy_exclusions.insert(
          net::HostPortPair::FromURL(GURL(value)));
    } else if (option == kDisableCompression) {
      globals_->enable_spdy_compression.set(false);
    } else if (option == kDisableAltProtocols) {
      globals_->use_alternate_protocols.set(false);
    } else if (option == kForceAltProtocols) {
      net::AlternateProtocolInfo pair(443, net::NPN_SPDY_3, 1);
      net::HttpServerPropertiesImpl::ForceAlternateProtocol(pair);
    } else if (option == kSingleDomain) {
      DVLOG(1) << "FORCING SINGLE DOMAIN";
      globals_->force_spdy_single_domain.set(true);
    } else if (option == kInitialMaxConcurrentStreams) {
      int streams;
      if (base::StringToInt(value, &streams))
        globals_->initial_max_spdy_concurrent_streams.set(streams);
    } else if (option.empty() && it == spdy_options.begin()) {
      continue;
    } else {
      LOG(DFATAL) << "Unrecognized spdy option: " << option;
    }
  }
}

// static
void IOThread::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kAuthSchemes,
                               "basic,digest,ntlm,negotiate,"
                               "spdyproxy");
  registry->RegisterBooleanPref(prefs::kDisableAuthNegotiateCnameLookup, false);
  registry->RegisterBooleanPref(prefs::kEnableAuthNegotiatePort, false);
  registry->RegisterStringPref(prefs::kAuthServerWhitelist, std::string());
  registry->RegisterStringPref(prefs::kAuthNegotiateDelegateWhitelist,
                               std::string());
  registry->RegisterStringPref(prefs::kGSSAPILibraryName, std::string());
  registry->RegisterStringPref(
      data_reduction_proxy::prefs::kDataReductionProxy, std::string());
  registry->RegisterBooleanPref(prefs::kEnableReferrers, true);
  data_reduction_proxy::RegisterPrefs(registry);
  registry->RegisterBooleanPref(prefs::kBuiltInDnsClientEnabled, true);
  registry->RegisterBooleanPref(prefs::kQuickCheckEnabled, true);
}

net::HttpAuthHandlerFactory* IOThread::CreateDefaultAuthHandlerFactory(
    net::HostResolver* resolver) {
  net::HttpAuthFilterWhitelist* auth_filter_default_credentials = NULL;
  if (!auth_server_whitelist_.empty()) {
    auth_filter_default_credentials =
        new net::HttpAuthFilterWhitelist(auth_server_whitelist_);
  }
  net::HttpAuthFilterWhitelist* auth_filter_delegate = NULL;
  if (!auth_delegate_whitelist_.empty()) {
    auth_filter_delegate =
        new net::HttpAuthFilterWhitelist(auth_delegate_whitelist_);
  }
  globals_->url_security_manager.reset(
      net::URLSecurityManager::Create(auth_filter_default_credentials,
                                      auth_filter_delegate));
  std::vector<std::string> supported_schemes;
  base::SplitString(auth_schemes_, ',', &supported_schemes);

  scoped_ptr<net::HttpAuthHandlerRegistryFactory> registry_factory(
      net::HttpAuthHandlerRegistryFactory::Create(
          supported_schemes, globals_->url_security_manager.get(),
          resolver, gssapi_library_name_, negotiate_disable_cname_lookup_,
          negotiate_enable_port_));
  return registry_factory.release();
}

void IOThread::ClearHostCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::HostCache* host_cache = globals_->host_resolver->GetHostCache();
  if (host_cache)
    host_cache->clear();
}

void IOThread::InitializeNetworkSessionParams(
    net::HttpNetworkSession::Params* params) {
  InitializeNetworkSessionParamsFromGlobals(*globals_, params);
}

// static
void IOThread::InitializeNetworkSessionParamsFromGlobals(
    const IOThread::Globals& globals,
    net::HttpNetworkSession::Params* params) {
  params->host_resolver = globals.host_resolver.get();
  params->cert_verifier = globals.cert_verifier.get();
  params->channel_id_service = globals.system_channel_id_service.get();
  params->transport_security_state = globals.transport_security_state.get();
  params->ssl_config_service = globals.ssl_config_service.get();
  params->http_auth_handler_factory = globals.http_auth_handler_factory.get();
  params->http_server_properties =
      globals.http_server_properties->GetWeakPtr();
  params->network_delegate = globals.system_network_delegate.get();
  params->host_mapping_rules = globals.host_mapping_rules.get();
  params->enable_ssl_connect_job_waiting =
      globals.enable_ssl_connect_job_waiting;
  params->ignore_certificate_errors = globals.ignore_certificate_errors;
  params->testing_fixed_http_port = globals.testing_fixed_http_port;
  params->testing_fixed_https_port = globals.testing_fixed_https_port;

  globals.initial_max_spdy_concurrent_streams.CopyToIfSet(
      &params->spdy_initial_max_concurrent_streams);
  globals.force_spdy_single_domain.CopyToIfSet(
      &params->force_spdy_single_domain);
  globals.enable_spdy_compression.CopyToIfSet(
      &params->enable_spdy_compression);
  globals.enable_spdy_ping_based_connection_checking.CopyToIfSet(
      &params->enable_spdy_ping_based_connection_checking);
  globals.spdy_default_protocol.CopyToIfSet(
      &params->spdy_default_protocol);
  params->next_protos = globals.next_protos;
  globals.trusted_spdy_proxy.CopyToIfSet(&params->trusted_spdy_proxy);
  globals.force_spdy_over_ssl.CopyToIfSet(&params->force_spdy_over_ssl);
  globals.force_spdy_always.CopyToIfSet(&params->force_spdy_always);
  params->forced_spdy_exclusions = globals.forced_spdy_exclusions;
  globals.use_alternate_protocols.CopyToIfSet(
      &params->use_alternate_protocols);
  globals.alternate_protocol_probability_threshold.CopyToIfSet(
      &params->alternate_protocol_probability_threshold);
  globals.enable_websocket_over_spdy.CopyToIfSet(
      &params->enable_websocket_over_spdy);

  globals.enable_quic.CopyToIfSet(&params->enable_quic);
  globals.enable_quic_pacing.CopyToIfSet(
      &params->enable_quic_pacing);
  globals.enable_quic_time_based_loss_detection.CopyToIfSet(
      &params->enable_quic_time_based_loss_detection);
  globals.enable_quic_port_selection.CopyToIfSet(
      &params->enable_quic_port_selection);
  globals.quic_max_packet_length.CopyToIfSet(&params->quic_max_packet_length);
  globals.quic_user_agent_id.CopyToIfSet(&params->quic_user_agent_id);
  globals.quic_supported_versions.CopyToIfSet(
      &params->quic_supported_versions);
  params->quic_connection_options = globals.quic_connection_options;

  globals.origin_to_force_quic_on.CopyToIfSet(
      &params->origin_to_force_quic_on);
  params->enable_user_alternate_protocol_ports =
      globals.enable_user_alternate_protocol_ports;
}

base::TimeTicks IOThread::creation_time() const {
  return creation_time_;
}

net::SSLConfigService* IOThread::GetSSLConfigService() {
  return ssl_config_service_manager_->Get();
}

void IOThread::ChangedToOnTheRecordOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Clear the host cache to avoid showing entries from the OTR session
  // in about:net-internals.
  ClearHostCache();
}

void IOThread::InitSystemRequestContext() {
  if (system_url_request_context_getter_.get())
    return;
  // If we're in unit_tests, IOThread may not be run.
  if (!BrowserThread::IsMessageLoopValid(BrowserThread::IO))
    return;
  system_proxy_config_service_.reset(
      ProxyServiceFactory::CreateProxyConfigService(
          pref_proxy_config_tracker_.get()));
  system_url_request_context_getter_ =
      new SystemURLRequestContextGetter(this);
  // Safe to post an unretained this pointer, since IOThread is
  // guaranteed to outlive the IO BrowserThread.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&IOThread::InitSystemRequestContextOnIOThread,
                 base::Unretained(this)));
}

void IOThread::InitSystemRequestContextOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!globals_->system_proxy_service.get());
  DCHECK(system_proxy_config_service_.get());

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  globals_->system_proxy_service.reset(
      ProxyServiceFactory::CreateProxyService(
          net_log_,
          globals_->proxy_script_fetcher_context.get(),
          globals_->system_network_delegate.get(),
          system_proxy_config_service_.release(),
          command_line,
          quick_check_enabled_.GetValue()));

  net::HttpNetworkSession::Params system_params;
  InitializeNetworkSessionParams(&system_params);
  system_params.net_log = net_log_;
  system_params.proxy_service = globals_->system_proxy_service.get();

  globals_->system_http_transaction_factory.reset(
      new net::HttpNetworkLayer(
          new net::HttpNetworkSession(system_params)));
  globals_->system_url_request_job_factory.reset(
      new net::URLRequestJobFactoryImpl());
  globals_->system_request_context.reset(
      ConstructSystemRequestContext(globals_, net_log_));
  globals_->system_request_context->set_ssl_config_service(
      globals_->ssl_config_service);
  globals_->system_request_context->set_http_server_properties(
      globals_->http_server_properties->GetWeakPtr());
}

void IOThread::UpdateDnsClientEnabled() {
  globals()->host_resolver->SetDnsClientEnabled(*dns_client_enabled_);
}

void IOThread::ConfigureQuic(const CommandLine& command_line) {
  // Always fetch the field trial group to ensure it is reported correctly.
  // The command line flags will be associated with a group that is reported
  // so long as trial is actually queried.
  std::string group =
      base::FieldTrialList::FindFullName(kQuicFieldTrialName);
  VariationParameters params;
  if (!variations::GetVariationParams(kQuicFieldTrialName, &params)) {
    params.clear();
  }

  ConfigureQuicGlobals(command_line, group, params, globals_);
}

// static
void IOThread::ConfigureQuicGlobals(
    const base::CommandLine& command_line,
    base::StringPiece quic_trial_group,
    const VariationParameters& quic_trial_params,
    IOThread::Globals* globals) {
  bool enable_quic = ShouldEnableQuic(command_line, quic_trial_group);
  globals->enable_quic.set(enable_quic);
  if (enable_quic) {
    globals->enable_quic_pacing.set(
        ShouldEnableQuicPacing(command_line, quic_trial_group,
                               quic_trial_params));
    globals->enable_quic_time_based_loss_detection.set(
        ShouldEnableQuicTimeBasedLossDetection(command_line, quic_trial_group,
                                               quic_trial_params));
    globals->enable_quic_port_selection.set(
        ShouldEnableQuicPortSelection(command_line));
    globals->quic_connection_options =
        GetQuicConnectionOptions(command_line, quic_trial_params);
  }

  size_t max_packet_length = GetQuicMaxPacketLength(command_line,
                                                    quic_trial_group,
                                                    quic_trial_params);
  if (max_packet_length != 0) {
    globals->quic_max_packet_length.set(max_packet_length);
  }

  std::string quic_user_agent_id =
      chrome::VersionInfo::GetVersionStringModifier();
  if (!quic_user_agent_id.empty())
    quic_user_agent_id.push_back(' ');
  chrome::VersionInfo version_info;
  quic_user_agent_id.append(version_info.ProductNameAndVersionForUserAgent());
  globals->quic_user_agent_id.set(quic_user_agent_id);

  net::QuicVersion version = GetQuicVersion(command_line, quic_trial_params);
  if (version != net::QUIC_VERSION_UNSUPPORTED) {
    net::QuicVersionVector supported_versions;
    supported_versions.push_back(version);
    globals->quic_supported_versions.set(supported_versions);
  }

  double threshold =
      GetAlternateProtocolProbabilityThreshold(command_line, quic_trial_params);
  if (threshold >=0 && threshold <= 1) {
    globals->alternate_protocol_probability_threshold.set(threshold);
    globals->http_server_properties->SetAlternateProtocolProbabilityThreshold(
        threshold);
  }

  if (command_line.HasSwitch(switches::kOriginToForceQuicOn)) {
    net::HostPortPair quic_origin =
        net::HostPortPair::FromString(
            command_line.GetSwitchValueASCII(switches::kOriginToForceQuicOn));
    if (!quic_origin.IsEmpty()) {
      globals->origin_to_force_quic_on.set(quic_origin);
    }
  }
}

bool IOThread::ShouldEnableQuic(const CommandLine& command_line,
                                base::StringPiece quic_trial_group) {
  if (command_line.HasSwitch(switches::kDisableQuic))
    return false;

  if (command_line.HasSwitch(switches::kEnableQuic))
    return true;

  return quic_trial_group.starts_with(kQuicFieldTrialEnabledGroupName) ||
      quic_trial_group.starts_with(kQuicFieldTrialHttpsEnabledGroupName);
}

bool IOThread::ShouldEnableQuicPortSelection(
      const CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kDisableQuicPortSelection))
    return false;

  if (command_line.HasSwitch(switches::kEnableQuicPortSelection))
    return true;

  return false;  // Default to disabling port selection on all channels.
}

bool IOThread::ShouldEnableQuicPacing(
    const CommandLine& command_line,
    base::StringPiece quic_trial_group,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kEnableQuicPacing))
    return true;

  if (command_line.HasSwitch(switches::kDisableQuicPacing))
    return false;

  if (base::LowerCaseEqualsASCII(
          GetVariationParam(quic_trial_params, "enable_pacing"),
          "true"))
    return true;

  return quic_trial_group.ends_with(kQuicFieldTrialPacingSuffix);
}

net::QuicTagVector IOThread::GetQuicConnectionOptions(
    const CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicConnectionOptions)) {
    return ParseQuicConnectionOptions(
        command_line.GetSwitchValueASCII(switches::kQuicConnectionOptions));
  }

  VariationParameters::const_iterator it =
      quic_trial_params.find("congestion_options");
  if (it == quic_trial_params.end())
    return net::QuicTagVector();

  return ParseQuicConnectionOptions(it->second);
}

// static
net::QuicTagVector IOThread::ParseQuicConnectionOptions(
    const std::string& connection_options) {
  net::QuicTagVector options;
  std::vector<std::string> tokens;
  base::SplitString(connection_options, ',', &tokens);
  // Tokens are expected to be no more than 4 characters long, but we
  // handle overflow gracefully.
  for (std::vector<std::string>::iterator token = tokens.begin();
       token != tokens.end(); ++token) {
    uint32 option = 0;
    for (size_t i = token->length() ; i > 0; --i) {
      option <<= 8;
      option |= static_cast<unsigned char>((*token)[i - 1]);
    }
    options.push_back(static_cast<net::QuicTag>(option));
  }
  return options;
}

// static
double IOThread::GetAlternateProtocolProbabilityThreshold(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  double value;
  if (command_line.HasSwitch(
          switches::kAlternateProtocolProbabilityThreshold)) {
    if (base::StringToDouble(
            command_line.GetSwitchValueASCII(
                switches::kAlternateProtocolProbabilityThreshold),
            &value)) {
      return value;
    }
  }
  if (base::StringToDouble(
          GetVariationParam(quic_trial_params,
                            "alternate_protocol_probability_threshold"),
          &value)) {
    return value;
  }
  return -1;
}

// static
bool IOThread::ShouldEnableQuicTimeBasedLossDetection(
    const CommandLine& command_line,
    base::StringPiece quic_trial_group,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kEnableQuicTimeBasedLossDetection))
    return true;

  if (command_line.HasSwitch(switches::kDisableQuicTimeBasedLossDetection))
    return false;

  if (base::LowerCaseEqualsASCII(
          GetVariationParam(quic_trial_params,
                            "enable_time_based_loss_detection"),
          "true"))
    return true;

  return quic_trial_group.ends_with(
      kQuicFieldTrialTimeBasedLossDetectionSuffix);
}

// static
size_t IOThread::GetQuicMaxPacketLength(
    const CommandLine& command_line,
    base::StringPiece quic_trial_group,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicMaxPacketLength)) {
    unsigned value;
    if (!base::StringToUint(
            command_line.GetSwitchValueASCII(switches::kQuicMaxPacketLength),
            &value)) {
      return 0;
    }
    return value;
  }

  unsigned value;
  if (base::StringToUint(GetVariationParam(quic_trial_params,
                                           "max_packet_length"),
                         &value)) {
    return value;
  }

  // Format of the packet length group names is:
  //   (Https)?Enabled<length>BytePackets.
  base::StringPiece length_str(quic_trial_group);
  if (length_str.starts_with(kQuicFieldTrialEnabledGroupName)) {
    length_str.remove_prefix(strlen(kQuicFieldTrialEnabledGroupName));
  } else if (length_str.starts_with(kQuicFieldTrialHttpsEnabledGroupName)) {
    length_str.remove_prefix(strlen(kQuicFieldTrialHttpsEnabledGroupName));
  } else {
    return 0;
  }
  if (!length_str.ends_with(kQuicFieldTrialPacketLengthSuffix)) {
    return 0;
  }
  length_str.remove_suffix(strlen(kQuicFieldTrialPacketLengthSuffix));
  if (!base::StringToUint(length_str, &value)) {
    return 0;
  }
  return value;
}

// static
net::QuicVersion IOThread::GetQuicVersion(
    const CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicVersion)) {
    return ParseQuicVersion(
        command_line.GetSwitchValueASCII(switches::kQuicVersion));
  }

  return ParseQuicVersion(GetVariationParam(quic_trial_params, "quic_version"));
}

// static
net::QuicVersion IOThread::ParseQuicVersion(const std::string& quic_version) {
  net::QuicVersionVector supported_versions = net::QuicSupportedVersions();
  for (size_t i = 0; i < supported_versions.size(); ++i) {
    net::QuicVersion version = supported_versions[i];
    if (net::QuicVersionToString(version) == quic_version) {
      return version;
    }
  }

  return net::QUIC_VERSION_UNSUPPORTED;
}
