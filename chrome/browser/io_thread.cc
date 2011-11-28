// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"

#include <vector>

#include "base/command_line.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/debug/leak_tracker.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_event_router_forwarder.h"
#include "chrome/browser/media/media_internals.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/net/passive_log_collector.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/browser/net/sdch_dictionary_fetcher.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/in_process_webkit/indexed_db_key_utility_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_fetcher.h"
#include "net/base/cert_verifier.h"
#include "net/base/cookie_monster.h"
#include "net/base/default_origin_bound_cert_store.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/mapped_host_resolver.h"
#include "net/base/net_util.h"
#include "net/base/origin_bound_cert_service.h"
#include "net/base/sdch_manager.h"
#include "net/dns/async_host_resolver.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/dns_cert_provenance_checker.h"

#if defined(USE_NSS)
#include "net/ocsp/nss_ocsp.h"
#endif  // defined(USE_NSS)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;

// The IOThread object must outlive any tasks posted to the IO thread before the
// Quit task, so base::Bind() calls are not refcounted.

namespace {

// Custom URLRequestContext used by requests which aren't associated with a
// particular profile. We need to use a subclass of URLRequestContext in order
// to provide the correct User-Agent.
class URLRequestContextWithUserAgent : public net::URLRequestContext {
 public:
  virtual const std::string& GetUserAgent(
      const GURL& url) const OVERRIDE {
    return content::GetUserAgent(url);
  }
};

// Used for the "system" URLRequestContext. If this grows more complicated, then
// consider inheriting directly from URLRequestContext rather than using
// implementation inheritance.
class SystemURLRequestContext : public URLRequestContextWithUserAgent {
 public:
  SystemURLRequestContext() {
#if defined(USE_NSS)
    net::SetURLRequestContextForOCSP(this);
#endif  // defined(USE_NSS)
  }

 private:
  virtual ~SystemURLRequestContext() {
#if defined(USE_NSS)
    net::SetURLRequestContextForOCSP(NULL);
#endif  // defined(USE_NSS)
  }
};

net::HostResolver* CreateGlobalHostResolver(net::NetLog* net_log) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  size_t parallelism = net::HostResolver::kDefaultParallelism;

  // Use the concurrency override from the command-line, if any.
  if (command_line.HasSwitch(switches::kHostResolverParallelism)) {
    std::string s =
        command_line.GetSwitchValueASCII(switches::kHostResolverParallelism);

    // Parse the switch (it should be a positive integer formatted as decimal).
    int n;
    if (base::StringToInt(s, &n) && n > 0) {
      parallelism = static_cast<size_t>(n);
    } else {
      LOG(ERROR) << "Invalid switch for host resolver parallelism: " << s;
    }
  } else {
    // Set up a field trial to see what impact the total number of concurrent
    // resolutions have on DNS resolutions.
    base::FieldTrial::Probability kDivisor = 1000;
    // For each option (i.e., non-default), we have a fixed probability.
    base::FieldTrial::Probability kProbabilityPerGroup = 100;  // 10%.

    // After June 30, 2011 builds, it will always be in default group
    // (parallel_default).
    scoped_refptr<base::FieldTrial> trial(
        new base::FieldTrial(
            "DnsParallelism", kDivisor, "parallel_default", 2011, 6, 30));

    // List options with different counts.
    // Firefox limits total to 8 in parallel, and default is currently 50.
    int parallel_6 = trial->AppendGroup("parallel_6", kProbabilityPerGroup);
    int parallel_7 = trial->AppendGroup("parallel_7", kProbabilityPerGroup);
    int parallel_8 = trial->AppendGroup("parallel_8", kProbabilityPerGroup);
    int parallel_9 = trial->AppendGroup("parallel_9", kProbabilityPerGroup);
    int parallel_10 = trial->AppendGroup("parallel_10", kProbabilityPerGroup);
    int parallel_14 = trial->AppendGroup("parallel_14", kProbabilityPerGroup);
    int parallel_20 = trial->AppendGroup("parallel_20", kProbabilityPerGroup);

    if (trial->group() == parallel_6)
      parallelism = 6;
    else if (trial->group() == parallel_7)
      parallelism = 7;
    else if (trial->group() == parallel_8)
      parallelism = 8;
    else if (trial->group() == parallel_9)
      parallelism = 9;
    else if (trial->group() == parallel_10)
      parallelism = 10;
    else if (trial->group() == parallel_14)
      parallelism = 14;
    else if (trial->group() == parallel_20)
      parallelism = 20;
  }

  size_t retry_attempts = net::HostResolver::kDefaultRetryAttempts;

  // Use the retry attempts override from the command-line, if any.
  if (command_line.HasSwitch(switches::kHostResolverRetryAttempts)) {
    std::string s =
        command_line.GetSwitchValueASCII(switches::kHostResolverRetryAttempts);
    // Parse the switch (it should be a non-negative integer).
    int n;
    if (base::StringToInt(s, &n) && n >= 0) {
      retry_attempts = static_cast<size_t>(n);
    } else {
      LOG(ERROR) << "Invalid switch for host resolver retry attempts: " << s;
    }
  }

  net::HostResolver* global_host_resolver = NULL;
  if (command_line.HasSwitch(switches::kDnsServer)) {
    std::string dns_ip_string =
      command_line.GetSwitchValueASCII(switches::kDnsServer);
    net::IPAddressNumber dns_ip_number;
    if (net::ParseIPLiteralToNumber(dns_ip_string, &dns_ip_number)) {
      global_host_resolver =
        net::CreateAsyncHostResolver(parallelism, dns_ip_number, net_log);
    } else {
      LOG(ERROR) << "Invalid IP address specified for --dns-server: "
                 << dns_ip_string;
    }
  }

  if (!global_host_resolver) {
    global_host_resolver =
      net::CreateSystemHostResolver(parallelism, retry_attempts, net_log);
  }

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
    return global_host_resolver;

  net::MappedHostResolver* remapped_resolver =
      new net::MappedHostResolver(global_host_resolver);
  remapped_resolver->SetRulesFromString(
      command_line.GetSwitchValueASCII(switches::kHostResolverRules));
  return remapped_resolver;
}

class LoggingNetworkChangeObserver
    : public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  // |net_log| must remain valid throughout our lifetime.
  explicit LoggingNetworkChangeObserver(net::NetLog* net_log)
      : net_log_(net_log) {
    net::NetworkChangeNotifier::AddIPAddressObserver(this);
  }

  ~LoggingNetworkChangeObserver() {
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
  }

  virtual void OnIPAddressChanged() {
    VLOG(1) << "Observed a change to the network IP addresses";

    net_log_->AddEntry(net::NetLog::TYPE_NETWORK_IP_ADDRESSES_CHANGED,
                       base::TimeTicks::Now(),
                       net::NetLog::Source(),
                       net::NetLog::PHASE_NONE,
                       NULL);
  }

 private:
  net::NetLog* net_log_;
  DISALLOW_COPY_AND_ASSIGN(LoggingNetworkChangeObserver);
};

// Create a separate request context for PAC fetches to avoid reference cycles.
// See IOThread::Globals for details.
scoped_refptr<net::URLRequestContext>
ConstructProxyScriptFetcherContext(IOThread::Globals* globals,
                                   net::NetLog* net_log) {
  scoped_refptr<net::URLRequestContext> context(
      new URLRequestContextWithUserAgent);
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_dnsrr_resolver(globals->dnsrr_resolver.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->proxy_script_fetcher_proxy_service.get());
  context->set_http_transaction_factory(
      globals->proxy_script_fetcher_http_transaction_factory.get());
  context->set_ftp_transaction_factory(
      globals->proxy_script_fetcher_ftp_transaction_factory.get());
  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_origin_bound_cert_service(
      globals->system_origin_bound_cert_service.get());
  context->set_network_delegate(globals->system_network_delegate.get());
  // TODO(rtenneti): We should probably use HttpServerPropertiesManager for the
  // system URLRequestContext too. There's no reason this should be tied to a
  // profile.
  return context;
}

scoped_refptr<net::URLRequestContext>
ConstructSystemRequestContext(IOThread::Globals* globals,
                              net::NetLog* net_log) {
  scoped_refptr<net::URLRequestContext> context(
      new SystemURLRequestContext);
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_dnsrr_resolver(globals->dnsrr_resolver.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->system_proxy_service.get());
  context->set_http_transaction_factory(
      globals->system_http_transaction_factory.get());
  context->set_ftp_transaction_factory(
      globals->system_ftp_transaction_factory.get());
  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_origin_bound_cert_service(
      globals->system_origin_bound_cert_service.get());
  return context;
}

}  // namespace

class SystemURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit SystemURLRequestContextGetter(IOThread* io_thread);
  virtual ~SystemURLRequestContextGetter();

  // Implementation for net::UrlRequestContextGetter.
  virtual net::URLRequestContext* GetURLRequestContext();
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const;

 private:
  IOThread* const io_thread_;  // Weak pointer, owned by BrowserProcess.
  scoped_refptr<base::MessageLoopProxy> io_message_loop_proxy_;

  base::debug::LeakTracker<SystemURLRequestContextGetter> leak_tracker_;
};

SystemURLRequestContextGetter::SystemURLRequestContextGetter(
    IOThread* io_thread)
    : io_thread_(io_thread),
      io_message_loop_proxy_(io_thread->message_loop_proxy()) {
}

SystemURLRequestContextGetter::~SystemURLRequestContextGetter() {}

net::URLRequestContext* SystemURLRequestContextGetter::GetURLRequestContext() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(io_thread_->globals()->system_request_context);

  return io_thread_->globals()->system_request_context;
}

scoped_refptr<base::MessageLoopProxy>
SystemURLRequestContextGetter::GetIOMessageLoopProxy() const {
  return io_message_loop_proxy_;
}

IOThread::Globals::Globals() {}

IOThread::Globals::~Globals() {}

IOThread::Globals::MediaGlobals::MediaGlobals() {}

IOThread::Globals::MediaGlobals::~MediaGlobals() {}

// |local_state| is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOThread more flexible for testing.
IOThread::IOThread(
    PrefService* local_state,
    ChromeNetLog* net_log,
    ExtensionEventRouterForwarder* extension_event_router_forwarder)
    : content::BrowserProcessSubThread(BrowserThread::IO),
      net_log_(net_log),
      extension_event_router_forwarder_(extension_event_router_forwarder),
      globals_(NULL),
      sdch_manager_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  // We call RegisterPrefs() here (instead of inside browser_prefs.cc) to make
  // sure that everything is initialized in the right order.
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
  ChromeNetworkDelegate::InitializeReferrersEnabled(&system_enable_referrers_,
                                                    local_state);
  ssl_config_service_manager_.reset(
      SSLConfigServiceManager::CreateDefaultManager(local_state));
  MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&IOThread::InitSystemRequestContext,
                            weak_factory_.GetWeakPtr()));
}

IOThread::~IOThread() {
  if (pref_proxy_config_tracker_.get())
    pref_proxy_config_tracker_->DetachFromPrefService();
  // We cannot rely on our base class to stop the thread since we want our
  // CleanUp function to run.
  Stop();
  DCHECK(!globals_);
}

IOThread::Globals* IOThread::globals() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return globals_;
}

ChromeNetLog* IOThread::net_log() {
  return net_log_;
}

net::URLRequestContextGetter* IOThread::system_url_request_context_getter() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!system_url_request_context_getter_) {
    InitSystemRequestContext();
  }
  return system_url_request_context_getter_;
}

void IOThread::Init() {
  // Though this thread is called the "IO" thread, it actually just routes
  // messages around; it shouldn't be allowed to perform any blocking disk I/O.
  base::ThreadRestrictions::SetIOAllowed(false);

  content::BrowserProcessSubThread::Init();

  DCHECK_EQ(MessageLoop::TYPE_IO, message_loop()->type());

#if defined(USE_NSS)
  net::SetMessageLoopForOCSP();
#endif  // defined(USE_NSS)

  DCHECK(!globals_);
  globals_ = new Globals;

  globals_->media.media_internals.reset(new MediaInternals());

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(
      new LoggingNetworkChangeObserver(net_log_));

  globals_->extension_event_router_forwarder =
      extension_event_router_forwarder_;
  globals_->system_network_delegate.reset(new ChromeNetworkDelegate(
      extension_event_router_forwarder_,
      NULL,
      NULL,
      NULL,
      &system_enable_referrers_));
  globals_->host_resolver.reset(
      CreateGlobalHostResolver(net_log_));
  globals_->cert_verifier.reset(new net::CertVerifier);
  globals_->dnsrr_resolver.reset(new net::DnsRRResolver);
  globals_->ssl_config_service = GetSSLConfigService();
  globals_->http_auth_handler_factory.reset(CreateDefaultAuthHandlerFactory(
      globals_->host_resolver.get()));
  globals_->http_server_properties.reset(new net::HttpServerPropertiesImpl);
  // For the ProxyScriptFetcher, we use a direct ProxyService.
  globals_->proxy_script_fetcher_proxy_service.reset(
      net::ProxyService::CreateDirectWithNetLog(net_log_));
  // In-memory cookie store.
  globals_->system_cookie_store = new net::CookieMonster(NULL, NULL);
  // In-memory origin-bound cert store.
  globals_->system_origin_bound_cert_service.reset(
      new net::OriginBoundCertService(
          new net::DefaultOriginBoundCertStore(NULL)));
  net::HttpNetworkSession::Params session_params;
  session_params.host_resolver = globals_->host_resolver.get();
  session_params.cert_verifier = globals_->cert_verifier.get();
  session_params.origin_bound_cert_service =
      globals_->system_origin_bound_cert_service.get();
  session_params.proxy_service =
      globals_->proxy_script_fetcher_proxy_service.get();
  session_params.http_auth_handler_factory =
      globals_->http_auth_handler_factory.get();
  session_params.network_delegate = globals_->system_network_delegate.get();
  // TODO(rtenneti): We should probably use HttpServerPropertiesManager for the
  // system URLRequestContext too. There's no reason this should be tied to a
  // profile.
  session_params.http_server_properties =
      globals_->http_server_properties.get();
  session_params.net_log = net_log_;
  session_params.ssl_config_service = globals_->ssl_config_service;
  scoped_refptr<net::HttpNetworkSession> network_session(
      new net::HttpNetworkSession(session_params));
  globals_->proxy_script_fetcher_http_transaction_factory.reset(
      new net::HttpNetworkLayer(network_session));
  globals_->proxy_script_fetcher_ftp_transaction_factory.reset(
      new net::FtpNetworkLayer(globals_->host_resolver.get()));

  globals_->proxy_script_fetcher_context =
      ConstructProxyScriptFetcherContext(globals_, net_log_);

  sdch_manager_ = new net::SdchManager();
  sdch_manager_->set_sdch_fetcher(new SdchDictionaryFetcher);
}

void IOThread::CleanUp() {
  delete sdch_manager_;
  sdch_manager_ = NULL;

  // Step 1: Kill all things that might be holding onto
  // net::URLRequest/net::URLRequestContexts.

#if defined(USE_NSS)
  net::ShutdownOCSP();
#endif  // defined(USE_NSS)

  // Destroy all URLRequests started by URLFetchers.
  content::URLFetcher::CancelAll();

  IndexedDBKeyUtilityClient::Shutdown();

  // If any child processes are still running, terminate them and
  // and delete the BrowserChildProcessHost instances to release whatever
  // IO thread only resources they are referencing.
  BrowserChildProcessHost::TerminateAll();

  system_url_request_context_getter_ = NULL;

  // Step 2: Release objects that the net::URLRequestContext could have been
  // pointing to.

  // This must be reset before the ChromeNetLog is destroyed.
  network_change_observer_.reset();

  system_proxy_config_service_.reset();

  delete globals_;
  globals_ = NULL;

  // net::URLRequest instances must NOT outlive the IO thread.
  base::debug::LeakTracker<net::URLRequest>::CheckForLeaks();

  base::debug::LeakTracker<SystemURLRequestContextGetter>::CheckForLeaks();

  // This will delete the |notification_service_|.  Make sure it's done after
  // anything else can reference it.
  content::BrowserProcessSubThread::CleanUp();
}

// static
void IOThread::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterStringPref(prefs::kAuthSchemes,
                                  "basic,digest,ntlm,negotiate");
  local_state->RegisterBooleanPref(prefs::kDisableAuthNegotiateCnameLookup,
                                   false);
  local_state->RegisterBooleanPref(prefs::kEnableAuthNegotiatePort, false);
  local_state->RegisterStringPref(prefs::kAuthServerWhitelist, "");
  local_state->RegisterStringPref(prefs::kAuthNegotiateDelegateWhitelist, "");
  local_state->RegisterStringPref(prefs::kGSSAPILibraryName, "");
  local_state->RegisterBooleanPref(prefs::kEnableReferrers, true);
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

  return net::HttpAuthHandlerRegistryFactory::Create(
      supported_schemes,
      globals_->url_security_manager.get(),
      resolver,
      gssapi_library_name_,
      negotiate_disable_cname_lookup_,
      negotiate_enable_port_);
}

void IOThread::ClearHostCache() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  net::HostCache* host_cache = globals_->host_resolver->GetHostCache();
  if (host_cache)
    host_cache->clear();
}

net::SSLConfigService* IOThread::GetSSLConfigService() {
  return ssl_config_service_manager_->Get();
}

void IOThread::InitSystemRequestContext() {
  if (system_url_request_context_getter_)
    return;
  // If we're in unit_tests, IOThread may not be run.
  if (!message_loop())
    return;
  ChromeProxyConfigService* proxy_config_service =
      ProxyServiceFactory::CreateProxyConfigService();
  system_proxy_config_service_.reset(proxy_config_service);
  if (pref_proxy_config_tracker_.get()) {
    pref_proxy_config_tracker_->SetChromeProxyConfigService(
        proxy_config_service);
  }
  system_url_request_context_getter_ =
      new SystemURLRequestContextGetter(this);
  message_loop()->PostTask(
      FROM_HERE, base::Bind(&IOThread::InitSystemRequestContextOnIOThread,
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
          globals_->proxy_script_fetcher_context,
          system_proxy_config_service_.release(),
          command_line));
  net::HttpNetworkSession::Params system_params;
  system_params.host_resolver = globals_->host_resolver.get();
  system_params.cert_verifier = globals_->cert_verifier.get();
  system_params.origin_bound_cert_service =
      globals_->system_origin_bound_cert_service.get();
  system_params.dnsrr_resolver = globals_->dnsrr_resolver.get();
  system_params.dns_cert_checker = NULL;
  system_params.ssl_host_info_factory = NULL;
  system_params.proxy_service = globals_->system_proxy_service.get();
  system_params.ssl_config_service = globals_->ssl_config_service.get();
  system_params.http_auth_handler_factory =
      globals_->http_auth_handler_factory.get();
  system_params.http_server_properties = globals_->http_server_properties.get();
  system_params.network_delegate = globals_->system_network_delegate.get();
  system_params.net_log = net_log_;
  globals_->system_http_transaction_factory.reset(
      new net::HttpNetworkLayer(
          new net::HttpNetworkSession(system_params)));
  globals_->system_ftp_transaction_factory.reset(
      new net::FtpNetworkLayer(globals_->host_resolver.get()));
  globals_->system_request_context =
      ConstructSystemRequestContext(globals_, net_log_);
}
