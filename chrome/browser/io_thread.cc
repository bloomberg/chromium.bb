// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"
#include "base/command_line.h"
#include "base/leak_tracker.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/chrome_net_log.h"
#include "chrome/browser/net/dns_global.h"
#include "chrome/browser/net/passive_log_collector.h"
#include "chrome/browser/net/url_fetcher.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/mapped_host_resolver.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/url_request/url_request.h"

namespace {

net::HostResolver* CreateGlobalHostResolver(
    net::NetworkChangeNotifier* network_change_notifier) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  net::HostResolver* global_host_resolver =
      net::CreateSystemHostResolver(network_change_notifier);

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

}  // namespace

// The IOThread object must outlive any tasks posted to the IO thread before the
// Quit task.
template <>
struct RunnableMethodTraits<IOThread> {
  void RetainCallee(IOThread* /* io_thread */) {}
  void ReleaseCallee(IOThread* /* io_thread */) {}
};

IOThread::IOThread()
    : BrowserProcessSubThread(ChromeThread::IO),
      globals_(NULL),
      prefetch_observer_(NULL),
      dns_master_(NULL) {}

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

void IOThread::InitDnsMaster(
    bool prefetching_enabled,
    base::TimeDelta max_queue_delay,
    size_t max_concurrent,
    const chrome_common_net::NameList& hostnames_to_prefetch,
    ListValue* referral_list) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &IOThread::InitDnsMasterOnIOThread,
          prefetching_enabled, max_queue_delay, max_concurrent,
          hostnames_to_prefetch, referral_list));
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
  globals_->network_change_notifier.reset(
      net::NetworkChangeNotifier::CreateDefaultNetworkChangeNotifier());
  globals_->host_resolver =
      CreateGlobalHostResolver(globals_->network_change_notifier.get());
  globals_->http_auth_handler_factory.reset(CreateDefaultAuthHandlerFactory());
}

void IOThread::CleanUp() {
  // Not initialized in Init().  May not be initialized.
  if (dns_master_) {
    DCHECK(prefetch_observer_);

    dns_master_->Shutdown();

    // TODO(willchan): Stop reference counting DnsMaster.  It's owned by
    // IOThread now.
    dns_master_->Release();
    dns_master_ = NULL;
    chrome_browser_net::FreeDnsPrefetchResources();
  }

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

  delete globals_;
  globals_ = NULL;

  // URLFetcher and URLRequest instances must NOT outlive the IO thread.
  //
  // Strictly speaking, URLFetcher's CheckForLeaks() should be done on the
  // UI thread. However, since there _shouldn't_ be any instances left
  // at this point, it shouldn't be a race.
  //
  // We check URLFetcher first, since if it has leaked then an associated
  // URLRequest will also have leaked. However it is more useful to
  // crash showing the callstack of URLFetcher's allocation than its
  // URLRequest member.
  base::LeakTracker<URLFetcher>::CheckForLeaks();
  base::LeakTracker<URLRequest>::CheckForLeaks();

  BrowserProcessSubThread::CleanUp();
}

net::HttpAuthHandlerFactory* IOThread::CreateDefaultAuthHandlerFactory() {
  net::HttpAuthHandlerRegistryFactory* registry_factory =
      net::HttpAuthHandlerFactory::CreateDefault();

  // Get the whitelist information from the command line, create an
  // HttpAuthFilterWhitelist, and attach it to the HttpAuthHandlerFactory.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string auth_server_whitelist;

  // Get whitelist information from the command line.
  if (command_line.HasSwitch(switches::kAuthServerWhitelist)) {
    auth_server_whitelist =
        command_line.GetSwitchValueASCII(switches::kAuthServerWhitelist);
  }

  // Set the NTLM and Negotiate filters (from the same whitelist).
  net::HttpAuthFilterWhitelist* ntlm_filter =
    new net::HttpAuthFilterWhitelist();
  net::HttpAuthFilterWhitelist* negotiate_filter =
    new net::HttpAuthFilterWhitelist();

  ntlm_filter->SetWhitelist(auth_server_whitelist);
  negotiate_filter->SetWhitelist(auth_server_whitelist);
  registry_factory->SetFilter("ntlm", ntlm_filter);
  registry_factory->SetFilter("negotiate", negotiate_filter);

  return registry_factory;
}

void IOThread::InitDnsMasterOnIOThread(
    bool prefetching_enabled,
    base::TimeDelta max_queue_delay,
    size_t max_concurrent,
    chrome_common_net::NameList hostnames_to_prefetch,
    ListValue* referral_list) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  CHECK(!dns_master_);

  chrome_browser_net::EnableDnsPrefetch(prefetching_enabled);

  dns_master_ = new chrome_browser_net::DnsMaster(
      globals_->host_resolver, max_queue_delay, max_concurrent);
  dns_master_->AddRef();

  DCHECK(!prefetch_observer_);
  prefetch_observer_ = chrome_browser_net::CreatePrefetchObserver();
  globals_->host_resolver->AddObserver(prefetch_observer_);

  FinalizeDnsPrefetchInitialization(
      dns_master_, prefetch_observer_, hostnames_to_prefetch, referral_list);
}

void IOThread::ChangedToOnTheRecordOnIOThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  if (dns_master_) {
    // Destroy all evidence of our OTR session.
    dns_master_->DnsMaster::DiscardAllResults();
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
