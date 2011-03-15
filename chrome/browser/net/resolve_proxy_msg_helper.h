// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_RESOLVE_PROXY_MSG_HELPER_H_
#define CHROME_BROWSER_NET_RESOLVE_PROXY_MSG_HELPER_H_
#pragma once

#include <deque>
#include <string>

#include "base/ref_counted.h"
#include "content/browser/browser_message_filter.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/proxy/proxy_service.h"

// Responds to ChildProcessHostMsg_ResolveProxy, kicking off a ProxyResolve
// request on the IO thread using the specified proxy service.  Completion is
// notified through the delegate.  If multiple requests are started at the same
// time, they will run in FIFO order, with only 1 being outstanding at a time.
//
// When an instance of ResolveProxyMsgHelper is destroyed, it cancels any
// outstanding proxy resolve requests with the proxy service. It also deletes
// the stored IPC::Message pointers for pending requests.
//
// This object is expected to live on the IO thread.
class ResolveProxyMsgHelper : public BrowserMessageFilter {
 public:
  // If |proxy_service| is NULL, then the main profile's proxy service will
  // be used.
  explicit ResolveProxyMsgHelper(net::ProxyService* proxy_service);

  // Destruction cancels the current outstanding request, and clears the
  // pending queue.
  ~ResolveProxyMsgHelper();

  // BrowserMessageFilter implementation
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

  void OnResolveProxy(const GURL& url, IPC::Message* reply_msg);

 private:
  // Callback for the ProxyService (bound to |callback_|).
  void OnResolveProxyCompleted(int result);

  // Starts the first pending request.
  void StartPendingRequest();

  // Get the proxy service instance to use. On success returns true and
  // sets |*out|. Otherwise returns false.
  bool GetProxyService(scoped_refptr<net::ProxyService>* out) const;

  // A PendingRequest is a resolve request that is in progress, or queued.
  struct PendingRequest {
   public:
     PendingRequest(const GURL& url, IPC::Message* reply_msg) :
         url(url), reply_msg(reply_msg), pac_req(NULL) { }

     // The URL of the request.
     GURL url;

     // Data to pass back to the delegate on completion (we own it until then).
     IPC::Message* reply_msg;

     // Handle for cancelling the current request if it has started (else NULL).
     net::ProxyService::PacRequest* pac_req;
  };

  // Members for the current outstanding proxy request.
  scoped_refptr<net::ProxyService> proxy_service_;
  net::CompletionCallbackImpl<ResolveProxyMsgHelper> callback_;
  net::ProxyInfo proxy_info_;

  // FIFO queue of pending requests. The first entry is always the current one.
  typedef std::deque<PendingRequest> PendingRequestList;
  PendingRequestList pending_requests_;

  // Specified by unit-tests, to use this proxy service in place of the
  // global one.
  scoped_refptr<net::ProxyService> proxy_service_override_;
};

#endif  // CHROME_BROWSER_NET_RESOLVE_PROXY_MSG_HELPER_H_
