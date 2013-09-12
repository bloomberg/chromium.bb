// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/dhcp_proxy_script_fetcher_chromeos.h"

#include "base/task_runner_util.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "net/proxy/proxy_script_fetcher.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/url_request/url_request_context.h"

namespace chromeos {

namespace {

// Runs on NetworkHandler::Get()->message_loop().
std::string GetPacUrlFromDefaultNetwork() {
  if (!NetworkHandler::IsInitialized())
    return std::string();
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (default_network)
    return default_network->web_proxy_auto_discovery_url().spec();
  return std::string();
}

}  // namespace

DhcpProxyScriptFetcherChromeos::DhcpProxyScriptFetcherChromeos(
    net::URLRequestContext* url_request_context)
    : url_request_context_(url_request_context),
      weak_ptr_factory_(this) {
  DCHECK(url_request_context_);
  proxy_script_fetcher_.reset(
      new net::ProxyScriptFetcherImpl(url_request_context_));
  if (NetworkHandler::IsInitialized())
    network_handler_message_loop_ = NetworkHandler::Get()->message_loop();
}

DhcpProxyScriptFetcherChromeos::~DhcpProxyScriptFetcherChromeos() {
}

int DhcpProxyScriptFetcherChromeos::Fetch(
    base::string16* utf16_text,
    const net::CompletionCallback& callback) {
  if (!network_handler_message_loop_.get())
    return net::ERR_PAC_NOT_IN_DHCP;
  CHECK(!callback.is_null());
  base::PostTaskAndReplyWithResult(
      network_handler_message_loop_.get(),
      FROM_HERE,
      base::Bind(&GetPacUrlFromDefaultNetwork),
      base::Bind(&DhcpProxyScriptFetcherChromeos::ContinueFetch,
                 weak_ptr_factory_.GetWeakPtr(), utf16_text, callback));
  return net::ERR_IO_PENDING;
}

void DhcpProxyScriptFetcherChromeos::Cancel() {
  proxy_script_fetcher_->Cancel();
  // Invalidate any pending callbacks (i.e. calls to ContinueFetch).
  weak_ptr_factory_.InvalidateWeakPtrs();
}

const GURL& DhcpProxyScriptFetcherChromeos::GetPacURL() const {
  return pac_url_;
}

std::string DhcpProxyScriptFetcherChromeos::GetFetcherName() const {
  return "chromeos";
}

void DhcpProxyScriptFetcherChromeos::ContinueFetch(
    base::string16* utf16_text,
    net::CompletionCallback callback,
    std::string pac_url) {
  NET_LOG_EVENT("DhcpProxyScriptFetcher", pac_url);
  pac_url_ = GURL(pac_url);
  if (pac_url_.is_empty()) {
    callback.Run(net::ERR_PAC_NOT_IN_DHCP);
    return;
  }
  int res = proxy_script_fetcher_->Fetch(pac_url_, utf16_text, callback);
  if (res != net::ERR_IO_PENDING)
    callback.Run(res);
}

}  // namespace chromeos
