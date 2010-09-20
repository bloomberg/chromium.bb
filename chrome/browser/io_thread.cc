// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"

#include "base/command_line.h"
#include "base/leak_tracker.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
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
#if defined(USE_NSS)
#include "net/ocsp/nss_ocsp.h"
#endif  // defined(USE_NSS)

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
  }

  net::HostResolver* global_host_resolver =
      net::CreateSystemHostResolver(parallelism, net_log);

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
    LOG(INFO) << "Observed a change to the network IP addresses";

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

}  // namespace

// The IOThread object must outlive any tasks posted to the IO thread before the
// Quit task.
DISABLE_RUNNABLE_METHOD_REFCOUNT(IOThread);

IOThread::IOThread()
    : BrowserProcessSubThread(ChromeThread::IO),
      globals_(NULL),
      speculative_interceptor_(NULL),
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

  DCHECK_EQ(MessageLoop::TYPE_IO, message_loop()->type());

#if defined(USE_NSS)
  net::SetMessageLoopForOCSP();
#endif // defined(USE_NSS)

  DCHECK(!globals_);
  globals_ = new Globals;

  globals_->net_log.reset(new ChromeNetLog());

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(
      new LoggingNetworkChangeObserver(globals_->net_log.get()));

  globals_->host_resolver = CreateGlobalHostResolver(globals_->net_log.get());
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

  // TODO(eroman): hack for http://crbug.com/15513
  if (globals_->host_resolver->GetAsHostResolverImpl()) {
    globals_->host_resolver.get()->GetAsHostResolverImpl()->Shutdown();
  }

  // We will delete the NetLog as part of CleanUpAfterMessageLoopDestruction()
  // in case any of the message loop destruction observers try to access it.
  deferred_net_log_to_delete_.reset(globals_->net_log.release());

  delete globals_;
  globals_ = NULL;

  BrowserProcessSubThread::CleanUp();
}

void IOThread::CleanUpAfterMessageLoopDestruction() {
  // TODO(eroman): get rid of this special case for 39723. If we could instead
  // have a method that runs after the message loop destruction obsevers have
  // run, but before the message loop itself is destroyed, we could safely
  // combine the two cleanups.
  deferred_net_log_to_delete_.reset();
  BrowserProcessSubThread::CleanUpAfterMessageLoopDestruction();

  // URLRequest instances must NOT outlive the IO thread.
  //
  // To allow for URLRequests to be deleted from
  // MessageLoop::DestructionObserver this check has to happen after CleanUp
  // (which runs before DestructionObservers).
  base::LeakTracker<URLRequest>::CheckForLeaks();
}

net::HttpAuthHandlerFactory* IOThread::CreateDefaultAuthHandlerFactory(
    net::HostResolver* resolver) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Get the whitelist information from the command line, create an
  // HttpAuthFilterWhitelist, and attach it to the HttpAuthHandlerFactory.
  net::HttpAuthFilterWhitelist* auth_filter_default_credentials = NULL;
  if (command_line.HasSwitch(switches::kAuthServerWhitelist)) {
    auth_filter_default_credentials = new net::HttpAuthFilterWhitelist(
        command_line.GetSwitchValueASCII(switches::kAuthServerWhitelist));
  }
  net::HttpAuthFilterWhitelist* auth_filter_delegate = NULL;
  if (command_line.HasSwitch(switches::kAuthNegotiateDelegateWhitelist)) {
    auth_filter_delegate = new net::HttpAuthFilterWhitelist(
        command_line.GetSwitchValueASCII(
            switches::kAuthNegotiateDelegateWhitelist));
  }
  globals_->url_security_manager.reset(
      net::URLSecurityManager::Create(auth_filter_default_credentials,
                                      auth_filter_delegate));

  // Determine which schemes are supported.
  std::string csv_auth_schemes = "basic,digest,ntlm,negotiate";
  if (command_line.HasSwitch(switches::kAuthSchemes))
    csv_auth_schemes = StringToLowerASCII(
        command_line.GetSwitchValueASCII(switches::kAuthSchemes));
  std::vector<std::string> supported_schemes;
  SplitString(csv_auth_schemes, ',', &supported_schemes);

  return net::HttpAuthHandlerRegistryFactory::Create(
      supported_schemes,
      globals_->url_security_manager.get(),
      resolver,
      command_line.HasSwitch(switches::kDisableAuthNegotiateCnameLookup),
      command_line.HasSwitch(switches::kEnableAuthNegotiatePort));
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

  // Speculative_interceptor_ is used to predict subresource usage.
  DCHECK(!speculative_interceptor_);
  speculative_interceptor_ = new chrome_browser_net::ConnectInterceptor;

  FinalizePredictorInitialization(predictor_, startup_urls, referral_list);
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
