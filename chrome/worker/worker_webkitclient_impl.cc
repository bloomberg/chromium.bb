// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/worker/worker_webkitclient_impl.h"

#include "base/logging.h"
#include "chrome/common/webmessageportchannel_impl.h"
#include "chrome/worker/worker_thread.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"

WebKit::WebClipboard* WorkerWebKitClientImpl::clipboard() {
  NOTREACHED();
  return NULL;
}

WebKit::WebMimeRegistry* WorkerWebKitClientImpl::mimeRegistry() {
  NOTREACHED();
  return NULL;
}

WebKit::WebSandboxSupport* WorkerWebKitClientImpl::sandboxSupport() {
  NOTREACHED();
  return NULL;
}

bool WorkerWebKitClientImpl::sandboxEnabled() {
  // Always return true because WebKit should always act as though the Sandbox
  // is enabled for workers.  See the comment in WebKitClient for more info.
  return true;
}

unsigned long long WorkerWebKitClientImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool WorkerWebKitClientImpl::isLinkVisited(unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

WebKit::WebMessagePortChannel*
WorkerWebKitClientImpl::createMessagePortChannel() {
  return new WebMessagePortChannelImpl();
}

void WorkerWebKitClientImpl::setCookies(const WebKit::WebURL& url,
                                        const WebKit::WebURL& policy_url,
                                        const WebKit::WebString& value) {
  NOTREACHED();
}

WebKit::WebString WorkerWebKitClientImpl::cookies(
    const WebKit::WebURL& url, const WebKit::WebURL& policy_url) {
  NOTREACHED();
  return WebKit::WebString();
}

void WorkerWebKitClientImpl::prefetchHostName(const WebKit::WebString&) {
  NOTREACHED();
}

bool WorkerWebKitClientImpl::getFileSize(const WebKit::WebString& path,
                                         long long& result) {
  NOTREACHED();
  return false;
}

WebKit::WebString WorkerWebKitClientImpl::defaultLocale() {
  NOTREACHED();
  return WebKit::WebString();
}

WebKit::WebSharedWorkerRepository*
WorkerWebKitClientImpl::sharedWorkerRepository() {
    return 0;
}
