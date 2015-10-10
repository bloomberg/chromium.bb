// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/proxy_service_factory.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/chrome_switches.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "net/log/net_log.h"
#include "net/proxy/dhcp_proxy_script_fetcher_factory.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/proxy/proxy_service_v8.h"
#include "net/url_request/url_request_context.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chromeos/network/dhcp_proxy_script_fetcher_chromeos.h"
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_IOS)
#include "net/proxy/proxy_resolver_v8.h"
#endif

#if !defined(OS_IOS) && !defined(OS_ANDROID)
#include "chrome/browser/net/utility_process_mojo_proxy_resolver_factory.h"
#include "net/proxy/proxy_service_mojo.h"
#endif

using content::BrowserThread;

namespace {

#if !defined(OS_ANDROID)
bool EnableOutOfProcessV8Pac(const base::CommandLine& command_line) {
  const std::string group_name =
      base::FieldTrialList::FindFullName("OutOfProcessPac");

  if (command_line.HasSwitch(switches::kDisableOutOfProcessPac))
    return false;
  if (command_line.HasSwitch(switches::kV8PacMojoOutOfProcess))
    return true;
  return group_name == "Enabled";
}
#endif  // !defined(OS_ANDROID)

}  // namespace

// static
scoped_ptr<net::ProxyConfigService>
ProxyServiceFactory::CreateProxyConfigService(PrefProxyConfigTracker* tracker) {
  // The linux gconf-based proxy settings getter relies on being initialized
  // from the UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_ptr<net::ProxyConfigService> base_service;

#if !defined(OS_CHROMEOS)
  // On ChromeOS, base service is NULL; chromeos::ProxyConfigServiceImpl
  // determines the effective proxy config to take effect in the network layer,
  // be it from prefs or system (which is network shill on chromeos).

  // For other platforms, create a baseline service that provides proxy
  // configuration in case nothing is configured through prefs (Note: prefs
  // include command line and configuration policy).

  // TODO(port): the IO and FILE message loops are only used by Linux.  Can
  // that code be moved to chrome/browser instead of being in net, so that it
  // can use BrowserThread instead of raw MessageLoop pointers? See bug 25354.
  base_service = net::ProxyService::CreateSystemProxyConfigService(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
#endif  // !defined(OS_CHROMEOS)

  return tracker->CreateTrackingProxyConfigService(base_service.Pass());
}

// static
PrefProxyConfigTracker*
ProxyServiceFactory::CreatePrefProxyConfigTrackerOfProfile(
    PrefService* profile_prefs,
    PrefService* local_state_prefs) {
#if defined(OS_CHROMEOS)
  return new chromeos::ProxyConfigServiceImpl(profile_prefs, local_state_prefs);
#else
  return new PrefProxyConfigTrackerImpl(
      profile_prefs,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
#endif  // defined(OS_CHROMEOS)
}

// static
PrefProxyConfigTracker*
ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
    PrefService* local_state_prefs) {
#if defined(OS_CHROMEOS)
  return new chromeos::ProxyConfigServiceImpl(NULL, local_state_prefs);
#else
  return new PrefProxyConfigTrackerImpl(
      local_state_prefs,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
#endif  // defined(OS_CHROMEOS)
}

// static
scoped_ptr<net::ProxyService> ProxyServiceFactory::CreateProxyService(
    net::NetLog* net_log,
    net::URLRequestContext* context,
    net::NetworkDelegate* network_delegate,
    scoped_ptr<net::ProxyConfigService> proxy_config_service,
    const base::CommandLine& command_line,
    bool quick_check_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
#if defined(OS_IOS)
  bool use_v8 = false;
#else
  bool use_v8 = !command_line.HasSwitch(switches::kWinHttpProxyResolver);
  // TODO(eroman): Figure out why this doesn't work in single-process mode.
  // Should be possible now that a private isolate is used.
  // http://crbug.com/474654
  if (use_v8 && command_line.HasSwitch(switches::kSingleProcess)) {
    LOG(ERROR) << "Cannot use V8 Proxy resolver in single process mode.";
    use_v8 = false;  // Fallback to non-v8 implementation.
  }
#endif  // defined(OS_IOS)

  size_t num_pac_threads = 0u;  // Use default number of threads.

  // Check the command line for an override on the number of proxy resolver
  // threads to use.
  if (command_line.HasSwitch(switches::kNumPacThreads)) {
    std::string s = command_line.GetSwitchValueASCII(switches::kNumPacThreads);

    // Parse the switch (it should be a positive integer formatted as decimal).
    int n;
    if (base::StringToInt(s, &n) && n > 0) {
      num_pac_threads = static_cast<size_t>(n);
    } else {
      LOG(ERROR) << "Invalid switch for number of PAC threads: " << s;
    }
  }

  scoped_ptr<net::ProxyService> proxy_service;
  if (use_v8) {
#if defined(OS_IOS)
    NOTREACHED();
#else
    scoped_ptr<net::DhcpProxyScriptFetcher> dhcp_proxy_script_fetcher;
#if defined(OS_CHROMEOS)
    dhcp_proxy_script_fetcher.reset(
        new chromeos::DhcpProxyScriptFetcherChromeos(context));
#else
    net::DhcpProxyScriptFetcherFactory dhcp_factory;
    dhcp_proxy_script_fetcher = dhcp_factory.Create(context);
#endif

#if !defined(OS_ANDROID)
    // In-process Mojo PAC can only be set on the command line, so its presence
    // should override other options.
    if (command_line.HasSwitch(switches::kV8PacMojoInProcess)) {
      proxy_service = net::CreateProxyServiceUsingMojoInProcess(
          proxy_config_service.Pass(), new net::ProxyScriptFetcherImpl(context),
          dhcp_proxy_script_fetcher.Pass(), context->host_resolver(), net_log,
          network_delegate);
    } else if (EnableOutOfProcessV8Pac(command_line)) {
      proxy_service = net::CreateProxyServiceUsingMojoFactory(
          UtilityProcessMojoProxyResolverFactory::GetInstance(),
          proxy_config_service.Pass(), new net::ProxyScriptFetcherImpl(context),
          dhcp_proxy_script_fetcher.Pass(), context->host_resolver(), net_log,
          network_delegate);
    }
#endif  // !defined(OS_ANDROID)

    if (!proxy_service) {
      proxy_service = net::CreateProxyServiceUsingV8ProxyResolver(
          proxy_config_service.Pass(), new net::ProxyScriptFetcherImpl(context),
          dhcp_proxy_script_fetcher.Pass(), context->host_resolver(), net_log,
          network_delegate);
    }
#endif  // defined(OS_IOS)
  } else {
    proxy_service = net::ProxyService::CreateUsingSystemProxyResolver(
        proxy_config_service.Pass(), num_pac_threads, net_log);
  }

  proxy_service->set_quick_check_enabled(quick_check_enabled);

  return proxy_service;
}
