// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_INIT_PROXY_RESOLVER_H_
#define NET_PROXY_INIT_PROXY_RESOLVER_H_

#include <string>
#include <vector>

#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"

namespace net {

class LoadLog;
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
  // |resolver| and |proxy_script_fetcher| must remain valid for
  // the lifespan of InitProxyResolver.
  InitProxyResolver(ProxyResolver* resolver,
                    ProxyScriptFetcher* proxy_script_fetcher);

  // Aborts any in-progress request.
  ~InitProxyResolver();

  // Apply the PAC settings of |config| to |resolver_|.
  int Init(const ProxyConfig& config,
           CompletionCallback* callback,
           LoadLog* load_log);

 private:
  enum State {
    STATE_NONE,
    STATE_FETCH_PAC_SCRIPT,
    STATE_FETCH_PAC_SCRIPT_COMPLETE,
    STATE_SET_PAC_SCRIPT,
    STATE_SET_PAC_SCRIPT_COMPLETE,
  };
  typedef std::vector<GURL> UrlList;

  // Returns ordered list of PAC urls to try for |config|.
  UrlList BuildPacUrlsFallbackList(const ProxyConfig& config) const;

  void OnIOCompletion(int result);
  int DoLoop(int result);
  void DoCallback(int result);

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
  const GURL& current_pac_url() const;

  void DidCompleteInit();
  void Cancel();

  ProxyResolver* resolver_;
  ProxyScriptFetcher* proxy_script_fetcher_;

  CompletionCallbackImpl<InitProxyResolver> io_callback_;
  CompletionCallback* user_callback_;

  size_t current_pac_url_index_;

  // Filled when the PAC script fetch completes.
  std::string pac_bytes_;

  UrlList pac_urls_;
  State next_state_;

  scoped_refptr<LoadLog> load_log_;

  DISALLOW_COPY_AND_ASSIGN(InitProxyResolver);
};

}  // namespace net

#endif  // NET_PROXY_INIT_PROXY_RESOLVER_H_
