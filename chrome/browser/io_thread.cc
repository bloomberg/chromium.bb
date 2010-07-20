// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"
#include "base/command_line.h"
#include "base/leak_tracker.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/gpu_process_host.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/net/passive_log_collector.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_fetcher.h"
#include "net/base/mapped_host_resolver.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/net_util.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_handler_negotiate.h"

namespace {

net::HostResolver* CreateGlobalHostResolver() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  size_t parallelism = net::HostResolver::kDefaultParallelism;

  // Use the concurrency override from the command-line, if any.
  if (command_line.HasSwitch(switches::kHostResolverParallelism)) {
    std::string s =
        command_line.GetSwitchValueASCII(switches::kHostResolverParallelism);

    // Parse the switch (it should be a positive integer formatted as decimal).
    int n;
    if (StringToInt(s, &n) && n > 0) {
      parallelism = static_cast<size_t>(n);
    } else {
      LOG(ERROR) << "Invalid switch for host resolver parallelism: " << s;
    }
  }

  net::HostResolver* global_host_resolver =
      net::CreateSystemHostResolver(parallelism);

  // Determine if we should disable IPv6 support.
  if (!command_line.HasSwitch(switches::kEnableIPv6)) {
    if (command_line.HasSwitch(switches::kDisableIPv6)) {
      global_host_resolver->SetDefaultAddressFamily(net::ADDRESS_FAMILY_IPV4);
    } else {
      net::HostResolverImpl* host_resolver_impl =
          global_host_resolver->GetAsHostResolverImpl();
      if (host_resolver_impl != NULL) {
        // (optionally) Use probe to decide if support is warranted.
        bool use_ipv6_probe = true;

#if defined(OS_WIN)
        // Measure impact of probing to allow IPv6.
        // Some users report confused OS handling of IPv6, leading to large
        // latency.  If we can show that IPv6 is not supported, then disabliing
        // it will work around such problems. This is the test of the probe.
        const FieldTrial::Probability kDivisor = 100;
        const FieldTrial::Probability kProbability = 50;  // 50% probability.
        FieldTrial* trial = new FieldTrial("IPv6_Probe", kDivisor);
        int skip_group = trial->AppendGroup("_IPv6_probe_skipped",
                                            kProbability);
        trial->AppendGroup("_IPv6_probe_done",
                           FieldTrial::kAllRemainingProbability);
        use_ipv6_probe = (trial->group() != skip_group);
#endif

        if (use_ipv6_probe)
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
    LOG(INFO) << "Observed a change to the network IP addresses";

    net::NetLog::Source global_source;

    // TODO(eroman): We shouldn't need to assign an ID to this source, since
    //               conceptually it is the "global event stream". However
    //               currently the javascript does a grouping on source id, so
    //               the display will look weird if we don't give it one.
    global_source.id = net_log_->NextID();

    net_log_->AddEntry(net::NetLog::TYPE_NETWORK_IP_ADDRESSSES_CHANGED,
                       base::TimeTicks::Now(),
                       global_source,
                       net::NetLog::PHASE_NONE,
                       NULL);
  }

 private:
  net::NetLog* net_log_;
  DISALLOW_COPY_AND_ASSIGN(LoggingNetworkChangeObserver);
};

}  // namespace

// The IOThread object must outlive any tasks posted to the IO thread before the
// Quit task.
DISABLE_RUNNABLE_METHOD_REFCOUNT(IOThread);

IOThread::IOThread()
    : BrowserProcessSubThread(ChromeThread::IO),
      globals_(NULL),
      speculative_interceptor_(NULL),
      prefetch_observer_(NULL),
      predictor_(NULL) {}

IOThread::~IOThread() {
  // We cannot rely on our base class to stop the thread since we want our
  // CleanUp function to run.
  Stop();
  DCHECK(!globals_);
}

IOThread::Globals* IOThread::globals() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return globals_;
}

void IOThread::InitNetworkPredictor(
    bool prefetching_enabled,
    base::TimeDelta max_dns_queue_delay,
    size_t max_concurrent,
    const chrome_common_net::UrlList& startup_urls,
    ListValue* referral_list,
    bool preconnect_enabled) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &IOThread::InitNetworkPredictorOnIOThread,
          prefetching_enabled, max_dns_queue_delay, max_concurrent,
          startup_urls, referral_list, preconnect_enabled));
}

void IOThread::ChangedToOnTheRecord() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &IOThread::ChangedToOnTheRecordOnIOThread));
}

void IOThread::Init() {
  BrowserProcessSubThread::Init();

  DCHECK(!globals_);
  globals_ = new Globals;

  globals_->net_log.reset(new ChromeNetLog());

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(
      new LoggingNetworkChangeObserver(globals_->net_log.get()));

  globals_->host_resolver = CreateGlobalHostResolver();
  globals_->http_auth_handler_factory.reset(CreateDefaultAuthHandlerFactory(
      globals_->host_resolver));
}

void IOThread::CleanUp() {
  // This must be reset before the ChromeNetLog is destroyed.
  network_change_observer_.reset();

  // If any child processes are still running, terminate them and
  // and delete the BrowserChildProcessHost instances to release whatever
  // IO thread only resources they are referencing.
  BrowserChildProcessHost::TerminateAll();

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

  // Not initialized in Init().  May not be initialized.
  if (prefetch_observer_) {
    globals_->host_resolver->RemoveObserver(prefetch_observer_);
    delete prefetch_observer_;
    prefetch_observer_ = NULL;
  }

  // TODO(eroman): hack for http://crbug.com/15513
  if (globals_->host_resolver->GetAsHostResolverImpl()) {
    globals_->host_resolver.get()->GetAsHostResolverImpl()->Shutdown();
  }

  // We will delete the NetLog as part of CleanUpAfterMessageLoopDestruction()
  // in case any of the message loop destruction observers try to access it.
  deferred_net_log_to_delete_.reset(globals_->net_log.release());

  delete globals_;
  globals_ = NULL;

  // URLRequest instances must NOT outlive the IO thread.
  base::LeakTracker<URLRequest>::CheckForLeaks();

  BrowserProcessSubThread::CleanUp();
}

void IOThread::CleanUpAfterMessageLoopDestruction() {
  // TODO(eroman): get rid of this special case for 39723. If we could instead
  // have a method that runs after the message loop destruction obsevers have
  // run, but before the message loop itself is destroyed, we could safely
  // combine the two cleanups.
  deferred_net_log_to_delete_.reset();
  BrowserProcessSubThread::CleanUpAfterMessageLoopDestruction();
}

net::HttpAuthHandlerFactory* IOThread::CreateDefaultAuthHandlerFactory(
    net::HostResolver* resolver) {
  net::HttpAuthFilterWhitelist* auth_filter = NULL;

  // Get the whitelist information from the command line, create an
  // HttpAuthFilterWhitelist, and attach it to the HttpAuthHandlerFactory.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kAuthServerWhitelist)) {
    std::string auth_server_whitelist =
        command_line.GetSwitchValueASCII(switches::kAuthServerWhitelist);

    // Create a whitelist filter.
    auth_filter = new net::HttpAuthFilterWhitelist();
    auth_filter->SetWhitelist(auth_server_whitelist);
  }

  // Set the flag that enables or disables the Negotiate auth handler.
  static const bool kNegotiateAuthEnabledDefault = true;

  bool negotiate_auth_enabled = kNegotiateAuthEnabledDefault;
  if (command_line.HasSwitch(switches::kExperimentalEnableNegotiateAuth)) {
    std::string enable_negotiate_auth = command_line.GetSwitchValueASCII(
        switches::kExperimentalEnableNegotiateAuth);
    // Enabled if no value, or value is 'true'.  Disabled otherwise.
    negotiate_auth_enabled =
        enable_negotiate_auth.empty() ||
        (StringToLowerASCII(enable_negotiate_auth) == "true");
  }

  net::HttpAuthHandlerRegistryFactory* registry_factory =
      net::HttpAuthHandlerFactory::CreateDefault();

  globals_->url_security_manager.reset(
      net::URLSecurityManager::Create(auth_filter));

  // Add the security manager to the auth factories that need it.
  registry_factory->SetURLSecurityManager("ntlm",
                                          globals_->url_security_manager.get());
  registry_factory->SetURLSecurityManager("negotiate",
                                          globals_->url_security_manager.get());
  if (negotiate_auth_enabled) {
    // Configure the Negotiate settings for the Kerberos SPN.
    // TODO(cbentzel): Read the related IE registry settings on Windows builds.
    // TODO(cbentzel): Ugly use of static_cast here.
    net::HttpAuthHandlerNegotiate::Factory* negotiate_factory =
        static_cast<net::HttpAuthHandlerNegotiate::Factory*>(
            registry_factory->GetSchemeFactory("negotiate"));
    DCHECK(negotiate_factory);
    negotiate_factory->set_host_resolver(resolver);
    if (command_line.HasSwitch(switches::kDisableAuthNegotiateCnameLookup))
      negotiate_factory->set_disable_cname_lookup(true);
    if (command_line.HasSwitch(switches::kEnableAuthNegotiatePort))
      negotiate_factory->set_use_port(true);
  } else {
    // Disable the Negotiate authentication handler.
    registry_factory->RegisterSchemeFactory("negotiate", NULL);
  }
  return registry_factory;
}

void IOThread::InitNetworkPredictorOnIOThread(
    bool prefetching_enabled,
    base::TimeDelta max_dns_queue_delay,
    size_t max_concurrent,
    const chrome_common_net::UrlList& startup_urls,
    ListValue* referral_list,
    bool preconnect_enabled) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  CHECK(!predictor_);

  chrome_browser_net::EnablePredictor(prefetching_enabled);

  predictor_ = new chrome_browser_net::Predictor(
      globals_->host_resolver,
      max_dns_queue_delay,
      max_concurrent,
      preconnect_enabled);
  predictor_->AddRef();

  // TODO(jar): Until connection notification and DNS observation handling are
  // properly combined into a learning model, we'll only use one observation
  // mechanism or the other.
  if (preconnect_enabled) {
    DCHECK(!speculative_interceptor_);
    speculative_interceptor_ = new chrome_browser_net::ConnectInterceptor;
  } else {
    DCHECK(!prefetch_observer_);
    prefetch_observer_ = chrome_browser_net::CreateResolverObserver();
    globals_->host_resolver->AddObserver(prefetch_observer_);
  }

  FinalizePredictorInitialization(
      predictor_, prefetch_observer_, startup_urls, referral_list);
}

void IOThread::ChangedToOnTheRecordOnIOThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

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
  globals_->net_log->passive_collector()->Clear();
}
