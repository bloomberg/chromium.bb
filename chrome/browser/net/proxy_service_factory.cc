// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/proxy_service_factory.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/pref_proxy_config_service.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"
#include "net/base/net_log.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/url_request/url_request_context.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/libcros_service_library.h"
#include "chrome/browser/chromeos/proxy_config_service.h"
#endif  // defined(OS_CHROMEOS)

// static
net::ProxyConfigService* ProxyServiceFactory::CreateProxyConfigService(
    PrefProxyConfigTracker* proxy_config_tracker) {
  // The linux gconf-based proxy settings getter relies on being initialized
  // from the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Create a baseline service that provides proxy configuration in case nothing
  // is configured through prefs (Note: prefs include command line and
  // configuration policy).
  net::ProxyConfigService* base_service = NULL;

  // TODO(port): the IO and FILE message loops are only used by Linux.  Can
  // that code be moved to chrome/browser instead of being in net, so that it
  // can use BrowserThread instead of raw MessageLoop pointers? See bug 25354.
#if defined(OS_CHROMEOS)
  base_service = new chromeos::ProxyConfigService(
      g_browser_process->chromeos_proxy_config_service_impl());
#else
  base_service = net::ProxyService::CreateSystemProxyConfigService(
      g_browser_process->io_thread()->message_loop(),
      g_browser_process->file_thread()->message_loop());
#endif  // defined(OS_CHROMEOS)

  return new PrefProxyConfigService(proxy_config_tracker, base_service);
}

// static
net::ProxyService* ProxyServiceFactory::CreateProxyService(
    net::NetLog* net_log,
    net::URLRequestContext* context,
    net::ProxyConfigService* proxy_config_service,
    const CommandLine& command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool use_v8 = !command_line.HasSwitch(switches::kWinHttpProxyResolver);
  if (use_v8 && command_line.HasSwitch(switches::kSingleProcess)) {
    // See the note about V8 multithreading in net/proxy/proxy_resolver_v8.h
    // to understand why we have this limitation.
    LOG(ERROR) << "Cannot use V8 Proxy resolver in single process mode.";
    use_v8 = false;  // Fallback to non-v8 implementation.
  }

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

  net::ProxyService* proxy_service;
  if (use_v8) {
    proxy_service = net::ProxyService::CreateUsingV8ProxyResolver(
        proxy_config_service,
        num_pac_threads,
        new net::ProxyScriptFetcherImpl(context),
        context->host_resolver(),
        net_log);
  } else {
    proxy_service = net::ProxyService::CreateUsingSystemProxyResolver(
        proxy_config_service,
        num_pac_threads,
        net_log);
  }

#if defined(OS_CHROMEOS)
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    chromeos::CrosLibrary::Get()->GetLibCrosServiceLibrary()->StartService();
  }
#endif  // defined(OS_CHROMEOS)

  return proxy_service;
}
