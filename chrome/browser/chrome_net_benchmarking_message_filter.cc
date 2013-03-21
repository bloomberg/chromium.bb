// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_net_benchmarking_message_filter.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/benchmarking_messages.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/host_cache.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_stream_factory.h"

namespace {

void ClearCacheCallback(ChromeNetBenchmarkingMessageFilter* filter,
                        IPC::Message* reply_msg,
                        int result) {
  ChromeViewHostMsg_ClearCache::WriteReplyParams(reply_msg, result);
  filter->Send(reply_msg);
}

}  // namespace

ChromeNetBenchmarkingMessageFilter::ChromeNetBenchmarkingMessageFilter(
    int render_process_id,
    Profile* profile,
    net::URLRequestContextGetter* request_context)
    : render_process_id_(render_process_id),
      profile_(profile),
      request_context_(request_context) {
}

ChromeNetBenchmarkingMessageFilter::~ChromeNetBenchmarkingMessageFilter() {
}

bool ChromeNetBenchmarkingMessageFilter::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ChromeNetBenchmarkingMessageFilter, message,
                           *message_was_ok)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_CloseCurrentConnections,
                        OnCloseCurrentConnections)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ChromeViewHostMsg_ClearCache, OnClearCache)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ClearHostResolverCache,
                        OnClearHostResolverCache)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_EnableSpdy, OnEnableSpdy)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ClearPredictorCache,
                        OnClearPredictorCache)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void ChromeNetBenchmarkingMessageFilter::OnClearCache(IPC::Message* reply_msg) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled()) {
    NOTREACHED() << "Received unexpected benchmarking IPC";
    return;
  }
  int rv = -1;

  disk_cache::Backend* backend = request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache()->GetCurrentBackend();
  if (backend) {
    net::CompletionCallback callback =
        base::Bind(&ClearCacheCallback, make_scoped_refptr(this), reply_msg);
    rv = backend->DoomAllEntries(callback);
    if (rv == net::ERR_IO_PENDING) {
      // The callback will send the reply.
      return;
    }
  }
  ChromeViewHostMsg_ClearCache::WriteReplyParams(reply_msg, rv);
  Send(reply_msg);
}

void ChromeNetBenchmarkingMessageFilter::OnClearHostResolverCache(int* result) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled()) {
    NOTREACHED() << "Received unexpected benchmarking IPC";
    return;
  }
  *result = -1;
  net::HostCache* cache =
      request_context_->GetURLRequestContext()->host_resolver()->GetHostCache();
  if (cache) {
    cache->clear();
    *result = 0;
  }
}

// TODO(lzheng): This only enables spdy over ssl. Enable spdy for http
// when needed.
void ChromeNetBenchmarkingMessageFilter::OnEnableSpdy(bool enable) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled()) {
    NOTREACHED() << "Received unexpected benchmarking IPC";
    return;
  }
  if (enable) {
    net::HttpStreamFactory::EnableNpnSpdy();
    net::HttpNetworkLayer::ForceAlternateProtocol();
  } else {
    net::HttpStreamFactory::EnableNpnHttpOnly();
  }
}

void ChromeNetBenchmarkingMessageFilter::OnCloseCurrentConnections() {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled()) {
    NOTREACHED() << "Received unexpected benchmarking IPC";
    return;
  }
  request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache()->CloseAllConnections();
}

void ChromeNetBenchmarkingMessageFilter::OnSetCacheMode(bool enabled) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled()) {
    NOTREACHED() << "Received unexpected benchmarking IPC";
    return;
  }
  net::HttpCache::Mode mode = enabled ?
      net::HttpCache::NORMAL : net::HttpCache::DISABLE;
  net::HttpCache* http_cache = request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache();
  http_cache->set_mode(mode);
}

void ChromeNetBenchmarkingMessageFilter::OnClearPredictorCache(int* result) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled()) {
    NOTREACHED() << "Received unexpected benchmarking IPC";
    return;
  }
  chrome_browser_net::Predictor* predictor = profile_->GetNetworkPredictor();
  if (predictor)
    predictor->DiscardAllResults();
  *result = 0;
}

bool ChromeNetBenchmarkingMessageFilter::CheckBenchmarkingEnabled() const {
  static bool checked = false;
  static bool result = false;
  if (!checked) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    result = command_line.HasSwitch(switches::kEnableNetBenchmarking);
    checked = true;
  }
  return result;
}

