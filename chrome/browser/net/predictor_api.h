// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the global interface for the dns prefetch services.  It centralizes
// initialization, along with all the callbacks etc. to connect to the browser
// process.  This allows the more standard DNS prefetching services, such as
// provided by Predictor to be left as more generally usable code, and possibly
// be shared across multiple client projects.

#ifndef CHROME_BROWSER_NET_PREDICTOR_API_H_
#define CHROME_BROWSER_NET_PREDICTOR_API_H_
#pragma once


#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/net/predictor.h"

namespace base {
class FieldTrial;
}

class PrefService;

namespace chrome_browser_net {

// Deletes |referral_list| when done.
void FinalizePredictorInitialization(
    Predictor* global_predictor,
    const std::vector<GURL>& urls_to_prefetch,
    ListValue* referral_list);

// Free all resources allocated by FinalizePredictorInitialization. After that
// you must not call any function from this file.
void FreePredictorResources();

//------------------------------------------------------------------------------
// Global APIs relating to predictions in browser.
void EnablePredictor(bool enable);
void DiscardInitialNavigationHistory();
void RegisterUserPrefs(PrefService* user_prefs);

// Renderer bundles up list and sends to this browser API via IPC.
// TODO(jar): Use UrlList instead to include port and scheme.
void DnsPrefetchList(const NameList& hostnames);

// This API is used by the autocomplete popup box (as user types).
// This will either preresolve the domain name, or possibly preconnect creating
// an open TCP/IP connection to the host.
void AnticipateOmniboxUrl(const GURL& url, bool preconnectable);

// This API should only be called when we're absolutely certain that we will
// be connecting to the URL.  It will preconnect the url and it's associated
// subresource domains immediately.
void PreconnectUrlAndSubresources(const GURL& url);

// When displaying info in about:dns, the following API is called.
void PredictorGetHtmlInfo(std::string* output);

// Destroy the predictor's internal state: referrers and work queue.
void ClearPredictorCache();

//------------------------------------------------------------------------------
// When we navigate to a frame that may contain embedded resources, we may know
// in advance some other URLs that will need to be connected to (via TCP and
// sometimes SSL).  This function initiates those connections
void PredictFrameSubresources(const GURL& url);

// During startup, we learn what the first N urls visited are, and then resolve
// the associated hosts ASAP during our next startup.
void LearnAboutInitialNavigation(const GURL& url);

// Call when we should learn from a navigation about a relationship to a
// subresource target, and its containing frame, which was loaded as a referring
// URL.
void LearnFromNavigation(const GURL& referring_url, const GURL& target_url);

//------------------------------------------------------------------------------
void SavePredictorStateForNextStartupAndTrim(PrefService* prefs);
// Helper class to handle global init and shutdown.
class PredictorInit {
 public:
  // Too many concurrent lookups performed in parallel may overload a resolver,
  // or may cause problems for a local router.  The following limits that count.
  static const size_t kMaxSpeculativeParallelResolves;

  // When prefetch requests are queued beyond some period of time, then the
  // system is congested, and we need to clear all queued requests to get out
  // of that state.  The following is the suggested default time limit.
  static const int kMaxSpeculativeResolveQueueDelayMs;

  PredictorInit(PrefService* user_prefs, PrefService* local_state,
                bool preconnect_enabled);
  ~PredictorInit();

 private:
  // Maintain a field trial instance when we do A/B testing.
  scoped_refptr<base::FieldTrial> trial_;

  DISALLOW_COPY_AND_ASSIGN(PredictorInit);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_PREDICTOR_API_H_
