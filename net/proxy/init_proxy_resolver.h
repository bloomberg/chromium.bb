// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_INIT_PROXY_RESOLVER_H_
#define NET_PROXY_INIT_PROXY_RESOLVER_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log.h"

namespace net {

class DhcpProxyScriptFetcher;
class NetLogParameter;
class ProxyConfig;
class ProxyResolver;
class ProxyScriptFetcher;
class URLRequestContext;

// InitProxyResolver is a helper class used by ProxyService to
// initialize a ProxyResolver with the PAC script data specified
// by a particular ProxyConfig.
//
// This involves trying to use PAC scripts in this order:
//
//   (1) WPAD (DNS) if auto-detect is on.
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
  // |resolver|, |proxy_script_fetcher|, |dhcp_proxy_script_fetcher| and
  // |net_log| must remain valid for the lifespan of InitProxyResolver.
  InitProxyResolver(ProxyResolver* resolver,
                    ProxyScriptFetcher* proxy_script_fetcher,
                    DhcpProxyScriptFetcher* dhcp_proxy_script_fetcher,
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
  // Represents the sources from which we can get PAC files; two types of
  // auto-detect or a custom URL.
  struct PacSource {
    enum Type {
      WPAD_DHCP,
      WPAD_DNS,
      CUSTOM
    };

    PacSource(Type type, const GURL& url)
        : type(type), url(url) {}

    Type type;
    GURL url;  // Empty unless |type == PAC_SOURCE_CUSTOM|.
  };

  typedef std::vector<PacSource> PacSourceList;

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
  PacSourceList BuildPacSourcesFallbackList(const ProxyConfig& config) const;

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
  // |pac_sources_[++current_pac_source_index]|.
  // Returns OK and rewinds the state machine when there
  // is something to try, otherwise returns |error|.
  int TryToFallbackPacSource(int error);

  // Gets the initial state (we skip fetching when the
  // ProxyResolver doesn't |expect_pac_bytes()|.
  State GetStartState() const;

  NetLogStringParameter* CreateNetLogParameterAndDetermineURL(
      const PacSource& pac_source, GURL* effective_pac_url);

  // Returns the current PAC URL we are fetching/testing.
  const PacSource& current_pac_source() const;

  void OnWaitTimerFired();
  void DidCompleteInit();
  void Cancel();

  ProxyResolver* resolver_;
  ProxyScriptFetcher* proxy_script_fetcher_;
  DhcpProxyScriptFetcher* dhcp_proxy_script_fetcher_;

  CompletionCallbackImpl<InitProxyResolver> io_callback_;
  CompletionCallback* user_callback_;

  size_t current_pac_source_index_;

  // Filled when the PAC script fetch completes.
  string16 pac_script_;

  // Flag indicating whether the caller requested a mandatory pac script
  // (i.e. fallback to direct connections are prohibited).
  bool pac_mandatory_;

  PacSourceList pac_sources_;
  State next_state_;

  BoundNetLog net_log_;

  base::TimeDelta wait_delay_;
  base::OneShotTimer<InitProxyResolver> wait_timer_;

  ProxyConfig* effective_config_;

  DISALLOW_COPY_AND_ASSIGN(InitProxyResolver);
};

}  // namespace net

#endif  // NET_PROXY_INIT_PROXY_RESOLVER_H_
