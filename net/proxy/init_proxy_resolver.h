// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_INIT_PROXY_RESOLVER_H_
#define NET_PROXY_INIT_PROXY_RESOLVER_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"

namespace net {

class ProxyConfig;
class ProxyResolver;
class ProxyScriptFetcher;

// InitProxyResolver is a helper class used by ProxyService to
// initialize a ProxyResolver with the PAC script data specified
// by a particular ProxyConfig.
//
// This involves trying to use PAC scripts in this order:
//
//   (1) WPAD if auto-detect is on.
//   (2) Custom PAC script if a URL was given.
//
// If no PAC script was successfully downloaded + parsed, then it fails with
// a network error. Otherwise the proxy resolver is left initialized with
// the PAC script.
//
// Deleting InitProxyResolver while Init() is in progress, will
// cancel the request.
//
class InitProxyResolver {
 public:
  // |resolver|, |proxy_script_fetcher| and |net_log| must remain valid for
  // the lifespan of InitProxyResolver.
  InitProxyResolver(ProxyResolver* resolver,
                    ProxyScriptFetcher* proxy_script_fetcher,
                    NetLog* net_log);

  // Aborts any in-progress request.
  ~InitProxyResolver();

  // Applies the PAC settings of |config| to |resolver_|.
  // If |wait_delay| is positive, the initialization will pause for this
  // amount of time before getting started.
  // If |effective_config| is non-NULL, then on successful initialization of
  // |resolver_| the "effective" proxy settings we ended up using will be
  // written out to |*effective_config|. Note that this may differ from
  // |config| since we will have stripped any manual settings, and decided
  // whether to use auto-detect or the custom PAC URL. Finally, if auto-detect
  // was used we may now have resolved that to a specific script URL.
  int Init(const ProxyConfig& config,
           const base::TimeDelta wait_delay,
           ProxyConfig* effective_config,
           CompletionCallback* callback);

 private:
  struct PacURL {
    PacURL(bool auto_detect, const GURL& url)
        : auto_detect(auto_detect), url(url) {}
    bool auto_detect;
    GURL url;
  };

  typedef std::vector<PacURL> UrlList;

  enum State {
    STATE_NONE,
    STATE_WAIT,
    STATE_WAIT_COMPLETE,
    STATE_FETCH_PAC_SCRIPT,
    STATE_FETCH_PAC_SCRIPT_COMPLETE,
    STATE_SET_PAC_SCRIPT,
    STATE_SET_PAC_SCRIPT_COMPLETE,
  };

  // Returns ordered list of PAC urls to try for |config|.
  UrlList BuildPacUrlsFallbackList(const ProxyConfig& config) const;

  void OnIOCompletion(int result);
  int DoLoop(int result);
  void DoCallback(int result);

  int DoWait();
  int DoWaitComplete(int result);

  int DoFetchPacScript();
  int DoFetchPacScriptComplete(int result);

  int DoSetPacScript();
  int DoSetPacScriptComplete(int result);

  // Tries restarting using the next fallback PAC URL:
  // |pac_urls_[++current_pac_url_index]|.
  // Returns OK and rewinds the state machine when there
  // is something to try, otherwise returns |error|.
  int TryToFallbackPacUrl(int error);

  // Gets the initial state (we skip fetching when the
  // ProxyResolver doesn't |expect_pac_bytes()|.
  State GetStartState() const;

  // Returns the current PAC URL we are fetching/testing.
  const PacURL& current_pac_url() const;

  void OnWaitTimerFired();
  void DidCompleteInit();
  void Cancel();

  ProxyResolver* resolver_;
  ProxyScriptFetcher* proxy_script_fetcher_;

  CompletionCallbackImpl<InitProxyResolver> io_callback_;
  CompletionCallback* user_callback_;

  size_t current_pac_url_index_;

  // Filled when the PAC script fetch completes.
  string16 pac_script_;

  UrlList pac_urls_;
  State next_state_;

  BoundNetLog net_log_;

  base::TimeDelta wait_delay_;
  base::OneShotTimer<InitProxyResolver> wait_timer_;

  ProxyConfig* effective_config_;

  DISALLOW_COPY_AND_ASSIGN(InitProxyResolver);
};

}  // namespace net

#endif  // NET_PROXY_INIT_PROXY_RESOLVER_H_
