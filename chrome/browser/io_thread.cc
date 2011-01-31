// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"

#include <vector>

#include "base/command_line.h"
#include "base/debug/leak_tracker.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/gpu_process_host.h"
#include "chrome/browser/in_process_webkit/indexed_db_key_utility_client.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/net/passive_log_collector.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/raw_host_resolver_proc.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/pref_names.h"
#include "net/base/cert_verifier.h"
#include "net/base/dnsrr_resolver.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/mapped_host_resolver.h"
#include "net/base/net_util.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#if defined(USE_NSS)
#include "net/ocsp/nss_ocsp.h"
#endif  // defined(USE_NSS)
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/socket/client_socket_factory.h"
#include "net/spdy/spdy_session_pool.h"

namespace {

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

  // Use the specified DNS server for doing raw resolutions if requested
  // from the command-line.
  scoped_refptr<net::HostResolverProc> resolver_proc;
  if (command_line.HasSwitch(switches::kDnsServer)) {
    std::string dns_ip_string =
        command_line.GetSwitchValueASCII(switches::kDnsServer);
    net::IPAddressNumber dns_ip_number;
    if (net::ParseIPLiteralToNumber(dns_ip_string, &dns_ip_number)) {
      resolver_proc =
          new chrome_common_net::RawHostResolverProc(dns_ip_number, NULL);
    } else {
      LOG(ERROR) << "Invalid IP address specified for --dns-server: "
                 << dns_ip_string;
    }
  }

  net::HostResolver* global_host_resolver =
      net::CreateSystemHostResolver(parallelism, resolver_proc.get(), net_log);

  // Determine if we should disable IPv6 support.
  if (!command_line.HasSwitch(switches::kEnableIPv6)) {
    if (command_line.HasSwitch(switches::kDisableIPv6)) {
      global_host_resolver->SetDefaultAddressFamily(net::ADDRESS_FAMILY_IPV4);
    } else {
      net::HostResolverImpl* host_resolver_impl =
          global_host_resolver->GetAsHostResolverImpl();
      if (host_resolver_impl != NULL) {
        // Use probe to decide if support is warranted.
        host_resolver_impl->ProbeIPv6Support();
      }
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
    : public net::NetworkChangeNotifier::Observer {
 public:
  // |net_log| must remain valid throughout our lifetime.
  explicit LoggingNetworkChangeObserver(net::NetLog* net_log)
      : net_log_(net_log) {
    net::NetworkChangeNotifier::AddObserver(this);
  }

  ~LoggingNetworkChangeObserver() {
    net::NetworkChangeNotifier::RemoveObserver(this);
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

scoped_refptr<net::URLRequestContext>
ConstructProxyScriptFetcherContext(IOThread::Globals* globals,
                                   net::NetLog* net_log) {
  scoped_refptr<net::URLRequestContext> context(new net::URLRequestContext);
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_dnsrr_resolver(globals->dnsrr_resolver.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->proxy_script_fetcher_proxy_service.get());
  context->set_http_transaction_factory(
      globals->proxy_script_fetcher_http_transaction_factory.get());
  // In-memory cookie store.
  context->set_cookie_store(new net::CookieMonster(NULL, NULL));
  // TODO(mpcomplete): give it a SystemNetworkDelegate.
  return context;
}

}  // namespace

// The IOThread object must outlive any tasks posted to the IO thread before the
// Quit task.
DISABLE_RUNNABLE_METHOD_REFCOUNT(IOThread);

IOThread::Globals::Globals() {}

IOThread::Globals::~Globals() {}

// |local_state| is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOThread more flexible for testing.
IOThread::IOThread(PrefService* local_state, ChromeNetLog* net_log)
    : BrowserProcessSubThread(BrowserThread::IO),
      net_log_(net_log),
      globals_(NULL),
      speculative_interceptor_(NULL),
      predictor_(NULL) {
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
}

IOThread::~IOThread() {
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

void IOThread::InitNetworkPredictor(
    bool prefetching_enabled,
    base::TimeDelta max_dns_queue_delay,
    size_t max_speculative_parallel_resolves,
    const chrome_common_net::UrlList& startup_urls,
    ListValue* referral_list,
    bool preconnect_enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &IOThread::InitNetworkPredictorOnIOThread,
          prefetching_enabled, max_dns_queue_delay,
          max_speculative_parallel_resolves,
          startup_urls, referral_list, preconnect_enabled));
}

void IOThread::RegisterURLRequestContextGetter(
    ChromeURLRequestContextGetter* url_request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::list<ChromeURLRequestContextGetter*>::const_iterator it =
      std::find(url_request_context_getters_.begin(),
                url_request_context_getters_.end(),
                url_request_context_getter);
  DCHECK(it == url_request_context_getters_.end());
  url_request_context_getters_.push_back(url_request_context_getter);
}

void IOThread::UnregisterURLRequestContextGetter(
    ChromeURLRequestContextGetter* url_request_context_getter) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::list<ChromeURLRequestContextGetter*>::iterator it =
      std::find(url_request_context_getters_.begin(),
                url_request_context_getters_.end(),
                url_request_context_getter);
  DCHECK(it != url_request_context_getters_.end());
  // This does not scale, but we shouldn't have many URLRequestContextGetters in
  // the first place, so this should be fine.
  url_request_context_getters_.erase(it);
}

void IOThread::ChangedToOnTheRecord() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &IOThread::ChangedToOnTheRecordOnIOThread));
}

void IOThread::Init() {
#if !defined(OS_CHROMEOS)
  // TODO(evan): test and enable this on all platforms.
  // Though this thread is called the "IO" thread, it actually just routes
  // messages around; it shouldn't be allowed to perform any blocking disk I/O.
  base::ThreadRestrictions::SetIOAllowed(false);
#endif

  BrowserProcessSubThread::Init();

  DCHECK_EQ(MessageLoop::TYPE_IO, message_loop()->type());

#if defined(USE_NSS)
  net::SetMessageLoopForOCSP();
#endif  // defined(USE_NSS)

  DCHECK(!globals_);
  globals_ = new Globals;

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(
      new LoggingNetworkChangeObserver(net_log_));

  globals_->client_socket_factory =
      net::ClientSocketFactory::GetDefaultFactory();
  globals_->host_resolver.reset(
      CreateGlobalHostResolver(net_log_));
  globals_->cert_verifier.reset(new net::CertVerifier);
  globals_->dnsrr_resolver.reset(new net::DnsRRResolver);
  // TODO(willchan): Use the real SSLConfigService.
  globals_->ssl_config_service =
      net::SSLConfigService::CreateSystemSSLConfigService();
  globals_->http_auth_handler_factory.reset(CreateDefaultAuthHandlerFactory(
      globals_->host_resolver.get()));
  // For the ProxyScriptFetcher, we use a direct ProxyService.
  globals_->proxy_script_fetcher_proxy_service =
      net::ProxyService::CreateDirectWithNetLog(net_log_);
  scoped_refptr<net::HttpNetworkSession> network_session(
      new net::HttpNetworkSession(
          globals_->host_resolver.get(),
          globals_->cert_verifier.get(),
          globals_->dnsrr_resolver.get(),
          NULL /* dns_cert_checker */,
          NULL /* ssl_host_info_factory */,
          globals_->proxy_script_fetcher_proxy_service.get(),
          globals_->client_socket_factory,
          globals_->ssl_config_service.get(),
          new net::SpdySessionPool(globals_->ssl_config_service.get()),
          globals_->http_auth_handler_factory.get(),
          &globals_->network_delegate,
          net_log_));
  globals_->proxy_script_fetcher_http_transaction_factory.reset(
      new net::HttpNetworkLayer(network_session));

  scoped_refptr<net::URLRequestContext> proxy_script_fetcher_context =
      ConstructProxyScriptFetcherContext(globals_, net_log_);
  globals_->proxy_script_fetcher_context = proxy_script_fetcher_context;
}

void IOThread::CleanUp() {
  // Step 1: Kill all things that might be holding onto
  // net::URLRequest/net::URLRequestContexts.

#if defined(USE_NSS)
  net::ShutdownOCSP();
#endif  // defined(USE_NSS)

  // Destroy all URLRequests started by URLFetchers.
  URLFetcher::CancelAll();

  IndexedDBKeyUtilityClient::Shutdown();

  // If any child processes are still running, terminate them and
  // and delete the BrowserChildProcessHost instances to release whatever
  // IO thread only resources they are referencing.
  BrowserChildProcessHost::TerminateAll();

  std::list<ChromeURLRequestContextGetter*> url_request_context_getters;
  url_request_context_getters.swap(url_request_context_getters_);
  for (std::list<ChromeURLRequestContextGetter*>::iterator it =
       url_request_context_getters.begin();
       it != url_request_context_getters.end(); ++it) {
    ChromeURLRequestContextGetter* getter = *it;
    // Stop all pending certificate provenance check uploads
    net::DnsCertProvenanceChecker* checker =
        getter->GetURLRequestContext()->dns_cert_checker();
    if (checker)
      checker->Shutdown();
    getter->ReleaseURLRequestContext();
  }

  // Step 2: Release objects that the net::URLRequestContext could have been
  // pointing to.

  // This must be reset before the ChromeNetLog is destroyed.
  network_change_observer_.reset();

  // Not initialized in Init().  May not be initialized.
  if (predictor_) {
    predictor_->Shutdown();

    // TODO(willchan): Stop reference counting Predictor.  It's owned by
    // IOThread now.
    predictor_->Release();
    predictor_ = NULL;
    chrome_browser_net::FreePredictorResources();
  }

  // Deletion will unregister this interceptor.
  delete speculative_interceptor_;
  speculative_interceptor_ = NULL;

  // TODO(eroman): hack for http://crbug.com/15513
  if (globals_->host_resolver->GetAsHostResolverImpl()) {
    globals_->host_resolver.get()->GetAsHostResolverImpl()->Shutdown();
  }

  delete globals_;
  globals_ = NULL;

  BrowserProcessSubThread::CleanUp();
}

void IOThread::CleanUpAfterMessageLoopDestruction() {
  // This will delete the |notification_service_|.  Make sure it's done after
  // anything else can reference it.
  BrowserProcessSubThread::CleanUpAfterMessageLoopDestruction();

  // net::URLRequest instances must NOT outlive the IO thread.
  //
  // To allow for URLRequests to be deleted from
  // MessageLoop::DestructionObserver this check has to happen after CleanUp
  // (which runs before DestructionObservers).
  base::debug::LeakTracker<net::URLRequest>::CheckForLeaks();
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

void IOThread::InitNetworkPredictorOnIOThread(
    bool prefetching_enabled,
    base::TimeDelta max_dns_queue_delay,
    size_t max_speculative_parallel_resolves,
    const chrome_common_net::UrlList& startup_urls,
    ListValue* referral_list,
    bool preconnect_enabled) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  CHECK(!predictor_);

  chrome_browser_net::EnablePredictor(prefetching_enabled);

  predictor_ = new chrome_browser_net::Predictor(
      globals_->host_resolver.get(),
      max_dns_queue_delay,
      max_speculative_parallel_resolves,
      preconnect_enabled);
  predictor_->AddRef();

  // Speculative_interceptor_ is used to predict subresource usage.
  DCHECK(!speculative_interceptor_);
  speculative_interceptor_ = new chrome_browser_net::ConnectInterceptor;

  FinalizePredictorInitialization(predictor_, startup_urls, referral_list);
}

void IOThread::ChangedToOnTheRecordOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (predictor_) {
    // Destroy all evidence of our OTR session.
    predictor_->Predictor::DiscardAllResults();
  }

  // Clear the host cache to avoid showing entries from the OTR session
  // in about:net-internals.
  if (globals_->host_resolver->GetAsHostResolverImpl()) {
    net::HostCache* host_cache =
        globals_->host_resolver.get()->GetAsHostResolverImpl()->cache();
    if (host_cache)
      host_cache->clear();
  }
  // Clear all of the passively logged data.
  // TODO(eroman): this is a bit heavy handed, really all we need to do is
  //               clear the data pertaining to off the record context.
  net_log_->ClearAllPassivelyCapturedEvents();
}
