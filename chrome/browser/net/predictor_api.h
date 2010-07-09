// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the global interface for the dns prefetch services.  It centralizes
// initialization, along with all the callbacks etc. to connect to the browser
// process.  This allows the more standard DNS prefetching services, such as
// provided by Predictor to be left as more generally usable code, and possibly
// be shared across multiple client projects.

#ifndef CHROME_BROWSER_NET_PREDICTOR_API_H_
#define CHROME_BROWSER_NET_PREDICTOR_API_H_


#include <string>
#include <vector>

#include "base/field_trial.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/net/predictor.h"
#include "net/base/host_resolver.h"

class PrefService;

namespace chrome_browser_net {

// Deletes |referral_list| when done.
void FinalizePredictorInitialization(
    Predictor* global_predictor,
    net::HostResolver::Observer* global_prefetch_observer,
    const std::vector<GURL>& urls_to_prefetch,
    ListValue* referral_list);

// Free all resources allocated by FinalizePredictorInitialization. After that
// you must not call any function from this file.
void FreePredictorResources();

// Creates the HostResolver observer for the prefetching system.
net::HostResolver::Observer* CreateResolverObserver();

//------------------------------------------------------------------------------
// Global APIs relating to predictions in browser.
void EnablePredictor(bool enable);
void RegisterPrefs(PrefService* local_state);
void RegisterUserPrefs(PrefService* user_prefs);

// Renderer bundles up list and sends to this browser API via IPC.
// TODO(jar): Use UrlList instead to include port and scheme.
void DnsPrefetchList(const NameList& hostnames);

// This API is used by the autocomplete popup box (as user types).
// This will either preresolve the domain name, or possibly preconnect creating
// an open TCP/IP connection to the host.
void AnticipateUrl(const GURL& url, bool preconnectable);

// When displaying info in about:dns, the following API is called.
void PredictorGetHtmlInfo(std::string* output);

//------------------------------------------------------------------------------
// When we navigate, we may know in advance some other URLs that will need to
// be resolved.  This function initiates those side effects.
void PredictSubresources(const GURL& url);

// When we navigate to a frame that may contain embedded resources, we may know
// in advance some other URLs that will need to be connected to (via TCP and
// sometimes SSL).  This function initiates those connections
void PredictFrameSubresources(const GURL& url);

// Call when we should learn from a navigation about a relationship to a
// subresource target, and its containing frame, which was loaded as a referring
// URL.
void LearnFromNavigation(const GURL& referring_url, const GURL& target_url);

//------------------------------------------------------------------------------
void SavePredictorStateForNextStartupAndTrim(PrefService* prefs);
// Helper class to handle global init and shutdown.
class PredictorInit {
 public:
  // Too many concurrent lookups negate benefits of prefetching by trashing
  // the OS cache before all resource loading is complete.
  // This is the default.
  static const size_t kMaxPrefetchConcurrentLookups;

  // When prefetch requests are queued beyond some period of time, then the
  // system is congested, and we need to clear all queued requests to get out
  // of that state.  The following is the suggested default time limit.
  static const int kMaxPrefetchQueueingDelayMs;

  PredictorInit(PrefService* user_prefs, PrefService* local_state,
                bool preconnect_enabled, bool preconnect_despite_proxy);

 private:
  // Maintain a field trial instance when we do A/B testing.
  scoped_refptr<FieldTrial> trial_;

  DISALLOW_COPY_AND_ASSIGN(PredictorInit);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_PREDICTOR_API_H_
