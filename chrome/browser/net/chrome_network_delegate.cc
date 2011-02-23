// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_network_delegate.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_proxy_api.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"

namespace {

// Get the event router from the request context. Currently only
// ChromeURLRequestContexts use a network delegate, so the cast is guaranteed to
// work.
const ExtensionIOEventRouter* GetIOEventRouter(
    net::URLRequestContext* context) {
  return static_cast<ChromeURLRequestContext*>(context)->
      extension_io_event_router();
}

// If the |request| failed due to problems with a proxy, forward the error to
// the proxy extension API.
void ForwardProxyErrors(net::URLRequest* request) {
  if (request->status().status() == net::URLRequestStatus::FAILED) {
    switch (request->status().os_error()) {
      case net::ERR_PROXY_AUTH_UNSUPPORTED:
      case net::ERR_PROXY_CONNECTION_FAILED:
      case net::ERR_TUNNEL_CONNECTION_FAILED:
        ExtensionProxyEventRouter::GetInstance()->OnProxyError(
            GetIOEventRouter(request->context()),
            request->status().os_error());
    }
  }
}

}  // namespace

ChromeNetworkDelegate::ChromeNetworkDelegate() {}
ChromeNetworkDelegate::~ChromeNetworkDelegate() {}

void ChromeNetworkDelegate::OnBeforeURLRequest(net::URLRequest* request) {
  ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      GetIOEventRouter(request->context()), request->url(), request->method());
}

void ChromeNetworkDelegate::OnSendHttpRequest(
    net::HttpRequestHeaders* headers) {
  DCHECK(headers);

  // TODO(willchan): Add Chrome-side hooks to listen / mutate requests here.
}

void ChromeNetworkDelegate::OnResponseStarted(net::URLRequest* request) {
  ForwardProxyErrors(request);
}

void ChromeNetworkDelegate::OnReadCompleted(net::URLRequest* request,
                                            int bytes_read) {
  ForwardProxyErrors(request);
}
