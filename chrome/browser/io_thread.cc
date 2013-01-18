// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_tracker.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/net/async_dns_field_trial.h"
#include "chrome/browser/net/basic_http_user_agent_settings.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/http_pipelining_compatibility_client.h"
#include "chrome/browser/net/load_time_stats.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/net/spdyproxy/http_auth_handler_spdyproxy.h"
#include "chrome/browser/policy/policy_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/cert_verifier.h"
#include "net/base/default_server_bound_cert_store.h"
#include "net/base/host_cache.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/host_resolver.h"
#include "net/base/mapped_host_resolver.h"
#include "net/base/net_util.h"
#include "net/base/sdch_manager.h"
#include "net/base/server_bound_cert_service.h"
#include "net/cookies/cookie_monster.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/spdy/spdy_session.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_throttler_manager.h"
#include "net/websockets/websocket_job.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "policy/policy_constants.h"
#endif

#if defined(USE_NSS) || defined(OS_IOS)
#include "net/ocsp/nss_ocsp.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;

class SafeBrowsingURLRequestContext;

// The IOThread object must outlive any tasks posted to the IO thread before the
// Quit task, so base::Bind() calls are not refcounted.

namespace {

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
#if defined(USE_NSS) || defined(OS_IOS)
    net::SetURLRequestContextForNSSHttpIO(NULL);
#endif
  }
};

scoped_ptr<net::HostResolver> CreateGlobalHostResolver(net::NetLog* net_log) {
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

  scoped_ptr<net::HostResolver> global_host_resolver(
      net::HostResolver::CreateSystemResolver(options, net_log));

  // Determine if we should disable IPv6 support.
  if (!command_line.HasSwitch(switches::kEnableIPv6)) {
    if (command_line.HasSwitch(switches::kDisableIPv6)) {
      global_host_resolver->SetDefaultAddressFamily(net::ADDRESS_FAMILY_IPV4);
    } else {
      global_host_resolver->ProbeIPv6Support();
    }
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
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->proxy_script_fetcher_proxy_service.get());
  context->set_http_transaction_factory(
      globals->proxy_script_fetcher_http_transaction_factory.get());
  context->set_ftp_transaction_factory(
      globals->proxy_script_fetcher_ftp_transaction_factory.get());
  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_server_bound_cert_service(
      globals->system_server_bound_cert_service.get());
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
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->system_proxy_service.get());
  context->set_http_transaction_factory(
      globals->system_http_transaction_factory.get());
  context->set_ftp_transaction_factory(
      globals->system_ftp_transaction_factory.get());
  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_server_bound_cert_service(
      globals->system_server_bound_cert_service.get());
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

  ~LoggingNetworkChangeObserver() {
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
    : ALLOW_THIS_IN_INITIALIZER_LIST(
        system_request_context_leak_checker(this)),
      ignore_certificate_errors(false),
      http_pipelining_enabled(false),
      testing_fixed_http_port(0),
      testing_fixed_https_port(0) {}

IOThread::Globals::~Globals() {}

// |local_state| is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOThread more flexible for testing.
IOThread::IOThread(
    PrefServiceSimple* local_state,
    policy::PolicyService* policy_service,
    ChromeNetLog* net_log,
    extensions::EventRouterForwarder* extension_event_router_forwarder)
    : net_log_(net_log),
      extension_event_router_forwarder_(extension_event_router_forwarder),
      globals_(NULL),
      sdch_manager_(NULL),
      is_spdy_disabled_by_policy_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  // We call RegisterPrefs() here (instead of inside browser_prefs.cc) to make
  // sure that everything is initialized in the right order.
  //
  // TODO(joi): See if we can fix so it does get registered from
  // browser_prefs::RegisterLocalState.
  RegisterPrefs(local_state);
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
      ProxyServiceFactory::CreatePrefProxyConfigTracker(local_state));
  ChromeNetworkDelegate::InitializePrefsOnUIThread(
      &system_enable_referrers_,
      NULL,
      NULL,
      local_state);
  ssl_config_service_manager_.reset(
      SSLConfigServiceManager::CreateDefaultManager(local_state, NULL));

  dns_client_enabled_.Init(prefs::kBuiltInDnsClientEnabled,
                           local_state,
                           base::Bind(&IOThread::UpdateDnsClientEnabled,
                                      base::Unretained(this)));
  dns_client_enabled_.MoveToThread(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));

#if defined(ENABLE_CONFIGURATION_POLICY)
  is_spdy_disabled_by_policy_ = policy_service->GetPolicies(
      policy::POLICY_DOMAIN_CHROME,
      std::string()).Get(policy::key::kDisableSpdy) != NULL;
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
  if (!system_url_request_context_getter_) {
    InitSystemRequestContext();
  }
  return system_url_request_context_getter_;
}

void IOThread::Init() {
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

  globals_->extension_event_router_forwarder =
      extension_event_router_forwarder_;
  ChromeNetworkDelegate* network_delegate =
      new ChromeNetworkDelegate(extension_event_router_forwarder_,
                                &system_enable_referrers_);
  if (command_line.HasSwitch(switches::kDisableExtensionsHttpThrottling))
    network_delegate->NeverThrottleRequests();
  globals_->system_network_delegate.reset(network_delegate);
  globals_->host_resolver = CreateGlobalHostResolver(net_log_);
  UpdateDnsClientEnabled();
  globals_->cert_verifier.reset(net::CertVerifier::CreateDefault());
  globals_->transport_security_state.reset(new net::TransportSecurityState());
  globals_->ssl_config_service = GetSSLConfigService();
  if (command_line.HasSwitch(switches::kSpdyProxyOrigin)) {
    spdyproxy_origin_ =
        command_line.GetSwitchValueASCII(switches::kSpdyProxyOrigin);
  }
  globals_->http_auth_handler_factory.reset(CreateDefaultAuthHandlerFactory(
      globals_->host_resolver.get()));
  globals_->http_server_properties.reset(new net::HttpServerPropertiesImpl);
  // For the ProxyScriptFetcher, we use a direct ProxyService.
  globals_->proxy_script_fetcher_proxy_service.reset(
      net::ProxyService::CreateDirectWithNetLog(net_log_));
  // In-memory cookie store.
  globals_->system_cookie_store = new net::CookieMonster(NULL, NULL);
  // In-memory server bound cert store.
  globals_->system_server_bound_cert_service.reset(
      new net::ServerBoundCertService(
          new net::DefaultServerBoundCertStore(NULL),
          base::WorkerPool::GetTaskRunner(true)));
  globals_->dns_probe_service.reset(new chrome_browser_net::DnsProbeService());
  globals_->load_time_stats.reset(new chrome_browser_net::LoadTimeStats());
  globals_->host_mapping_rules.reset(new net::HostMappingRules());
  globals_->http_user_agent_settings.reset(
      new BasicHttpUserAgentSettings(EmptyString(), EmptyString()));
  if (command_line.HasSwitch(switches::kHostRules)) {
    globals_->host_mapping_rules->SetRulesFromString(
        command_line.GetSwitchValueASCII(switches::kHostRules));
  }
  if (command_line.HasSwitch(switches::kIgnoreCertificateErrors))
    globals_->ignore_certificate_errors = true;
  if (command_line.HasSwitch(switches::kEnableHttpPipelining))
    globals_->http_pipelining_enabled = true;
  if (command_line.HasSwitch(switches::kTestingFixedHttpPort)) {
    globals_->testing_fixed_http_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpPort);
  }
  if (command_line.HasSwitch(switches::kTestingFixedHttpsPort)) {
    globals_->testing_fixed_https_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpsPort);
  }
  if (command_line.HasSwitch(switches::kOriginPortToForceQuicOn)) {
    globals_->origin_port_to_force_quic_on.set(
        GetSwitchValueAsInt(command_line,
                            switches::kOriginPortToForceQuicOn));
  }

  InitializeNetworkOptions(command_line);

  net::HttpNetworkSession::Params session_params;
  InitializeNetworkSessionParams(&session_params);
  session_params.net_log = net_log_;
  session_params.proxy_service =
      globals_->proxy_script_fetcher_proxy_service.get();

  scoped_refptr<net::HttpNetworkSession> network_session(
      new net::HttpNetworkSession(session_params));
  globals_->proxy_script_fetcher_http_transaction_factory.reset(
      new net::HttpNetworkLayer(network_session));
  globals_->proxy_script_fetcher_ftp_transaction_factory.reset(
      new net::FtpNetworkLayer(globals_->host_resolver.get()));

  globals_->throttler_manager.reset(new net::URLRequestThrottlerManager());
  globals_->throttler_manager->set_net_log(net_log_);
  // Always done in production, disabled only for unit tests.
  globals_->throttler_manager->set_enable_thread_checks(true);

  globals_->proxy_script_fetcher_context.reset(
      ConstructProxyScriptFetcherContext(globals_, net_log_));

  sdch_manager_ = new net::SdchManager();

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

  // We constructed the weak pointer on the IO thread but it will be
  // used on the UI thread.  Call this to avoid a thread checker
  // error.
  weak_factory_.DetachFromThread();
}

void IOThread::CleanUp() {
  base::debug::LeakTracker<SafeBrowsingURLRequestContext>::CheckForLeaks();

  delete sdch_manager_;
  sdch_manager_ = NULL;

#if defined(USE_NSS) || defined(OS_IOS)
  net::ShutdownNSSHttpIO();
#endif

  system_url_request_context_getter_ = NULL;

  // Release objects that the net::URLRequestContext could have been pointing
  // to.

  // This must be reset before the ChromeNetLog is destroyed.
  network_change_observer_.reset();

  system_proxy_config_service_.reset();

  delete globals_;
  globals_ = NULL;

  base::debug::LeakTracker<SystemURLRequestContextGetter>::CheckForLeaks();
}

void IOThread::InitializeNetworkOptions(const CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kEnableFileCookies)) {
    // Enable cookie storage for file:// URLs.  Must do this before the first
    // Profile (and therefore the first CookieMonster) is created.
    net::CookieMonster::EnableFileScheme();
  }

  // If "spdy.disabled" preference is controlled via policy, then skip use-spdy
  // command line flags.
  if (is_spdy_disabled_by_policy_)
    return;

  if (command_line.HasSwitch(switches::kEnableIPPooling))
    globals_->enable_spdy_ip_pooling.set(true);

  if (command_line.HasSwitch(switches::kDisableIPPooling))
    globals_->enable_spdy_ip_pooling.set(false);

  if (command_line.HasSwitch(switches::kEnableSpdyCredentialFrames))
    globals_->enable_spdy_credential_frames.set(true);

  if (command_line.HasSwitch(switches::kEnableWebSocketOverSpdy)) {
    // Enable WebSocket over SPDY.
    net::WebSocketJob::set_websocket_over_spdy_enabled(true);
  }
  if (command_line.HasSwitch(switches::kMaxSpdySessionsPerDomain)) {
    globals_->max_spdy_sessions_per_domain.set(
        GetSwitchValueAsInt(command_line, switches::kMaxSpdySessionsPerDomain));
  }
  if (command_line.HasSwitch(switches::kMaxSpdyConcurrentStreams)) {
    globals_->max_spdy_concurrent_streams_limit.set(
        GetSwitchValueAsInt(command_line, switches::kMaxSpdyConcurrentStreams));
  }

  bool used_spdy_switch = false;
  if (command_line.HasSwitch(switches::kUseSpdy)) {
    std::string spdy_mode =
        command_line.GetSwitchValueASCII(switches::kUseSpdy);
    EnableSpdy(spdy_mode);
    used_spdy_switch = true;
  }
  if (command_line.HasSwitch(switches::kEnableSpdy3)) {
    net::HttpStreamFactory::EnableNpnSpdy3();
    used_spdy_switch = true;
  } else if (command_line.HasSwitch(switches::kEnableNpn)) {
    net::HttpStreamFactory::EnableNpnSpdy();
    used_spdy_switch = true;
  } else if (command_line.HasSwitch(switches::kEnableNpnHttpOnly)) {
    net::HttpStreamFactory::EnableNpnHttpOnly();
    used_spdy_switch = true;
  }
  if (!used_spdy_switch) {
    net::HttpStreamFactory::EnableNpnSpdy3();
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
    const std::string& option = name_value.size() > 0 ? name_value[0] : "";
    const std::string value = name_value.size() > 1 ? name_value[1] : "";

    if (option == kOff) {
      net::HttpStreamFactory::set_spdy_enabled(false);
    } else if (option == kDisableSSL) {
      globals_->spdy_default_protocol.set(net::kProtoSPDY2);
      net::HttpStreamFactory::set_force_spdy_over_ssl(false);
      net::HttpStreamFactory::set_force_spdy_always(true);
    } else if (option == kSSL) {
      globals_->spdy_default_protocol.set(net::kProtoSPDY2);
      net::HttpStreamFactory::set_force_spdy_over_ssl(true);
      net::HttpStreamFactory::set_force_spdy_always(true);
    } else if (option == kDisablePing) {
      globals_->enable_spdy_ping_based_connection_checking.set(false);
    } else if (option == kExclude) {
      net::HttpStreamFactory::add_forced_spdy_exclusion(value);
    } else if (option == kDisableCompression) {
      globals_->enable_spdy_compression.set(false);
    } else if (option == kDisableAltProtocols) {
      net::HttpStreamFactory::set_use_alternate_protocols(false);
    } else if (option == kForceAltProtocols) {
      net::PortAlternateProtocolPair pair;
      pair.port = 443;
      pair.protocol = net::NPN_SPDY_2;
      net::HttpServerPropertiesImpl::ForceAlternateProtocol(pair);
    } else if (option == kSingleDomain) {
      DLOG(INFO) << "FORCING SINGLE DOMAIN";
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
void IOThread::RegisterPrefs(PrefServiceSimple* local_state) {
  local_state->RegisterStringPref(prefs::kAuthSchemes,
                                  "basic,digest,ntlm,negotiate,"
                                  "spdyproxy");
  local_state->RegisterBooleanPref(prefs::kDisableAuthNegotiateCnameLookup,
                                   false);
  local_state->RegisterBooleanPref(prefs::kEnableAuthNegotiatePort, false);
  local_state->RegisterStringPref(prefs::kAuthServerWhitelist, "");
  local_state->RegisterStringPref(prefs::kAuthNegotiateDelegateWhitelist, "");
  local_state->RegisterStringPref(prefs::kGSSAPILibraryName, "");
  local_state->RegisterStringPref(prefs::kSpdyProxyOrigin, "");
  local_state->RegisterBooleanPref(prefs::kEnableReferrers, true);
  local_state->RegisterInt64Pref(prefs::kHttpReceivedContentLength, 0);
  local_state->RegisterInt64Pref(prefs::kHttpOriginalContentLength, 0);
  local_state->RegisterBooleanPref(
      prefs::kBuiltInDnsClientEnabled,
      chrome_browser_net::ConfigureAsyncDnsFieldTrial());
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

  if (!spdyproxy_origin_.empty()) {
    GURL origin_url(spdyproxy_origin_);
    if (origin_url.is_valid()) {
      registry_factory->RegisterSchemeFactory(
          "spdyproxy",
          new spdyproxy::HttpAuthHandlerSpdyProxy::Factory(origin_url));
    } else {
      LOG(WARNING) << "Skipping creation of SpdyProxy auth handler since "
                   << "authorized origin is invalid: "
                   << spdyproxy_origin_;
    }
  }

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
  params->host_resolver = globals_->host_resolver.get();
  params->cert_verifier = globals_->cert_verifier.get();
  params->server_bound_cert_service =
      globals_->system_server_bound_cert_service.get();
  params->transport_security_state = globals_->transport_security_state.get();
  params->ssl_config_service = globals_->ssl_config_service.get();
  params->http_auth_handler_factory = globals_->http_auth_handler_factory.get();
  params->http_server_properties = globals_->http_server_properties.get();
  params->network_delegate = globals_->system_network_delegate.get();
  params->host_mapping_rules = globals_->host_mapping_rules.get();
  params->ignore_certificate_errors = globals_->ignore_certificate_errors;
  params->http_pipelining_enabled = globals_->http_pipelining_enabled;
  params->testing_fixed_http_port = globals_->testing_fixed_http_port;
  params->testing_fixed_https_port = globals_->testing_fixed_https_port;

  globals_->max_spdy_sessions_per_domain.CopyToIfSet(
      &params->max_spdy_sessions_per_domain);
  globals_->initial_max_spdy_concurrent_streams.CopyToIfSet(
      &params->spdy_initial_max_concurrent_streams);
  globals_->max_spdy_concurrent_streams_limit.CopyToIfSet(
      &params->spdy_max_concurrent_streams_limit);
  globals_->force_spdy_single_domain.CopyToIfSet(
      &params->force_spdy_single_domain);
  globals_->enable_spdy_ip_pooling.CopyToIfSet(
      &params->enable_spdy_ip_pooling);
  globals_->enable_spdy_credential_frames.CopyToIfSet(
      &params->enable_spdy_credential_frames);
  globals_->enable_spdy_compression.CopyToIfSet(
      &params->enable_spdy_compression);
  globals_->enable_spdy_ping_based_connection_checking.CopyToIfSet(
      &params->enable_spdy_ping_based_connection_checking);
  globals_->spdy_default_protocol.CopyToIfSet(
      &params->spdy_default_protocol);
  globals_->origin_port_to_force_quic_on.CopyToIfSet(
      &params->origin_port_to_force_quic_on);
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
  if (system_url_request_context_getter_)
    return;
  // If we're in unit_tests, IOThread may not be run.
  if (!BrowserThread::IsMessageLoopValid(BrowserThread::IO))
    return;
  ChromeProxyConfigService* proxy_config_service =
      ProxyServiceFactory::CreateProxyConfigService();
  system_proxy_config_service_.reset(proxy_config_service);
  pref_proxy_config_tracker_->SetChromeProxyConfigService(proxy_config_service);
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
          system_proxy_config_service_.release(),
          command_line));

  net::HttpNetworkSession::Params system_params;
  InitializeNetworkSessionParams(&system_params);
  system_params.net_log = net_log_;
  system_params.proxy_service = globals_->system_proxy_service.get();

  globals_->system_http_transaction_factory.reset(
      new net::HttpNetworkLayer(
          new net::HttpNetworkSession(system_params)));
  globals_->system_ftp_transaction_factory.reset(
      new net::FtpNetworkLayer(globals_->host_resolver.get()));
  globals_->system_request_context.reset(
      ConstructSystemRequestContext(globals_, net_log_));

  sdch_manager_->set_sdch_fetcher(
      new SdchDictionaryFetcher(system_url_request_context_getter_.get()));
}

void IOThread::UpdateDnsClientEnabled() {
  globals()->host_resolver->SetDnsClientEnabled(*dns_client_enabled_);
}
