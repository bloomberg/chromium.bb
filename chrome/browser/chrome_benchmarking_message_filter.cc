// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_benchmarking_message_filter.h"

#include "base/command_line.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/benchmarking_messages.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_network_layer.h"

namespace {

class ClearCacheCompletion : public net::CompletionCallback {
 public:
  ClearCacheCompletion(ChromeBenchmarkingMessageFilter* filter,
                       IPC::Message* reply_msg)
      : filter_(filter),
        reply_msg_(reply_msg) {
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    ChromeViewHostMsg_ClearCache::WriteReplyParams(reply_msg_, params.a);
    filter_->Send(reply_msg_);
    delete this;
  }

 private:
  scoped_refptr<ChromeBenchmarkingMessageFilter> filter_;
  IPC::Message* reply_msg_;
};

// Class to assist with clearing out the cache when we want to preserve
// the sslhostinfo entries.  It's not very efficient, but its just for debug.
class DoomEntriesHelper {
 public:
  explicit DoomEntriesHelper(disk_cache::Backend* backend)
      : backend_(backend),
        entry_(NULL),
        iter_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(callback_(this,
            &DoomEntriesHelper::CacheCallback)),
        user_callback_(NULL) {
  }

  void ClearCache(ClearCacheCompletion* callback) {
    user_callback_ = callback;
    return CacheCallback(net::OK);  // Start clearing the cache.
  }

 private:
  void CacheCallback(int result) {
    do {
      if (result != net::OK) {
        user_callback_->RunWithParams(Tuple1<int>(result));
        delete this;
        return;
      }

      if (entry_) {
        // Doom all entries except those with snapstart information.
        std::string key = entry_->GetKey();
        if (key.find("sslhostinfo:") != 0) {
          entry_->Doom();
          backend_->EndEnumeration(&iter_);
          iter_ = NULL;  // We invalidated our iterator - start from the top!
        }
        entry_->Close();
        entry_ = NULL;
      }
      result = backend_->OpenNextEntry(&iter_, &entry_, &callback_);
    } while (result != net::ERR_IO_PENDING);
  }

  disk_cache::Backend* backend_;
  disk_cache::Entry* entry_;
  void* iter_;
  net::CompletionCallbackImpl<DoomEntriesHelper> callback_;
  ClearCacheCompletion* user_callback_;
};

}  // namespace

ChromeBenchmarkingMessageFilter::ChromeBenchmarkingMessageFilter(
    int render_process_id,
    Profile* profile,
    net::URLRequestContextGetter* request_context)
    : render_process_id_(render_process_id),
      profile_(profile),
      request_context_(request_context) {
}

ChromeBenchmarkingMessageFilter::~ChromeBenchmarkingMessageFilter() {
}

bool ChromeBenchmarkingMessageFilter::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ChromeBenchmarkingMessageFilter, message,
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

void ChromeBenchmarkingMessageFilter::OnClearCache(bool preserve_ssl_host_info,
                                                   IPC::Message* reply_msg) {
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
    ClearCacheCompletion* callback =
        new ClearCacheCompletion(this, reply_msg);
    if (preserve_ssl_host_info) {
      DoomEntriesHelper* helper = new DoomEntriesHelper(backend);
      helper->ClearCache(callback);  // Will self clean.
      return;
    } else {
      rv = backend->DoomAllEntries(callback);
      if (rv == net::ERR_IO_PENDING) {
        // The callback will send the reply.
        return;
      }
      // Completed synchronously, no need for the callback.
      delete callback;
    }
  }
  ChromeViewHostMsg_ClearCache::WriteReplyParams(reply_msg, rv);
  Send(reply_msg);
}

void ChromeBenchmarkingMessageFilter::OnClearHostResolverCache(int* result) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled()) {
    NOTREACHED() << "Received unexpected benchmarking IPC";
    return;
  }
  *result = -1;
  net::HostResolverImpl* host_resolver_impl =
      request_context_->GetURLRequestContext()->
      host_resolver()->GetAsHostResolverImpl();
  if (host_resolver_impl) {
    net::HostCache* cache = host_resolver_impl->cache();
    DCHECK(cache);
    cache->clear();
    *result = 0;
  }
}

// TODO(lzheng): This only enables spdy over ssl. Enable spdy for http
// when needed.
void ChromeBenchmarkingMessageFilter::OnEnableSpdy(bool enable) {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled()) {
    NOTREACHED() << "Received unexpected benchmarking IPC";
    return;
  }
  if (enable) {
    net::HttpNetworkLayer::EnableSpdy("npn,force-alt-protocols");
  } else {
    net::HttpNetworkLayer::EnableSpdy("npn-http");
  }
}

void ChromeBenchmarkingMessageFilter::OnCloseCurrentConnections() {
  // This function is disabled unless the user has enabled
  // benchmarking extensions.
  if (!CheckBenchmarkingEnabled()) {
    NOTREACHED() << "Received unexpected benchmarking IPC";
    return;
  }
  request_context_->GetURLRequestContext()->
      http_transaction_factory()->GetCache()->CloseAllConnections();
}

void ChromeBenchmarkingMessageFilter::OnSetCacheMode(bool enabled) {
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

void ChromeBenchmarkingMessageFilter::OnClearPredictorCache(int* result) {
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

bool ChromeBenchmarkingMessageFilter::CheckBenchmarkingEnabled() const {
  static bool checked = false;
  static bool result = false;
  if (!checked) {
    const CommandLine& command_line = *CommandLine::ForCurrentProcess();
    result = command_line.HasSwitch(switches::kEnableBenchmarking);
    checked = true;
  }
  return result;
}

