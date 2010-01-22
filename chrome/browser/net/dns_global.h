// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the global interface for the dns prefetch services.  It centralizes
// initialization, along with all the callbacks etc. to connect to the browser
// process.  This allows the more standard DNS prefetching services, such as
// provided by DnsMaster to be left as more generally usable code, and possibly
// be shared across multiple client projects.

#ifndef CHROME_BROWSER_NET_DNS_GLOBAL_H_
#define CHROME_BROWSER_NET_DNS_GLOBAL_H_


#include <string>

#include "base/field_trial.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/net/dns_master.h"
#include "net/base/host_resolver.h"

class PrefService;

namespace chrome_browser_net {

// Deletes |referral_list| when done.
void FinalizeDnsPrefetchInitialization(
    DnsMaster* global_dns_master,
    net::HostResolver::Observer* global_prefetch_observer,
    const NameList& hostnames_to_prefetch,
    ListValue* referral_list);

// Free all resources allocated by FinalizeDnsPrefetchInitialization. After that
// you must not call any function from this file.
void FreeDnsPrefetchResources();

// Creates the HostResolver observer for the prefetching system.
net::HostResolver::Observer* CreatePrefetchObserver();

//------------------------------------------------------------------------------
// Global APIs relating to Prefetching in browser
void EnableDnsPrefetch(bool enable);
void RegisterPrefs(PrefService* local_state);
void RegisterUserPrefs(PrefService* user_prefs);
// Renderer bundles up list and sends to this browser API via IPC.
void DnsPrefetchList(const NameList& hostnames);
// This API is used by the autocomplete popup box (as user types).
void DnsPrefetchUrl(const GURL& url);
void DnsPrefetchGetHtmlInfo(std::string* output);

//------------------------------------------------------------------------------
void SaveDnsPrefetchStateForNextStartupAndTrim(PrefService* prefs);
// Helper class to handle global init and shutdown.
class DnsGlobalInit {
 public:
  // Too many concurrent lookups negate benefits of prefetching by trashing
  // the OS cache before all resource loading is complete.
  // This is the default.
  static const size_t kMaxPrefetchConcurrentLookups;

  // When prefetch requests are queued beyond some period of time, then the
  // system is congested, and we need to clear all queued requests to get out
  // of that state.  The following is the suggested default time limit.
  static const int kMaxPrefetchQueueingDelayMs;

  DnsGlobalInit(PrefService* user_prefs, PrefService* local_state);

 private:
  // Maintain a field trial instance when we do A/B testing.
  scoped_refptr<FieldTrial> trial_;

  DISALLOW_COPY_AND_ASSIGN(DnsGlobalInit);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_GLOBAL_H_
