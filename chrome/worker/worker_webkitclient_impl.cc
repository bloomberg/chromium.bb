// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/worker/worker_webkitclient_impl.h"

#include "base/logging.h"
#include "chrome/common/webmessageportchannel_impl.h"
#include "chrome/worker/worker_thread.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"

using WebKit::WebClipboard;
using WebKit::WebMessagePortChannel;
using WebKit::WebMimeRegistry;
using WebKit::WebSandboxSupport;
using WebKit::WebSharedWorkerRepository;
using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebURL;

WebClipboard* WorkerWebKitClientImpl::clipboard() {
  NOTREACHED();
  return NULL;
}

WebMimeRegistry* WorkerWebKitClientImpl::mimeRegistry() {
  return this;
}

WebSandboxSupport* WorkerWebKitClientImpl::sandboxSupport() {
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

WebMessagePortChannel*
WorkerWebKitClientImpl::createMessagePortChannel() {
  return new WebMessagePortChannelImpl();
}

void WorkerWebKitClientImpl::setCookies(const WebURL& url,
                                        const WebURL& policy_url,
                                        const WebString& value) {
  NOTREACHED();
}

WebString WorkerWebKitClientImpl::cookies(
    const WebURL& url, const WebURL& policy_url) {
  NOTREACHED();
  return WebString();
}

void WorkerWebKitClientImpl::prefetchHostName(const WebString&) {
  NOTREACHED();
}

bool WorkerWebKitClientImpl::getFileSize(const WebString& path,
                                         long long& result) {
  NOTREACHED();
  return false;
}

WebString WorkerWebKitClientImpl::defaultLocale() {
  NOTREACHED();
  return WebString();
}

WebStorageNamespace* WorkerWebKitClientImpl::createLocalStorageNamespace(
    const WebString& path, unsigned quota) {
  NOTREACHED();
  return 0;
}

WebStorageNamespace* WorkerWebKitClientImpl::createSessionStorageNamespace() {
  NOTREACHED();
  return 0;
}

void WorkerWebKitClientImpl::dispatchStorageEvent(
    const WebString& key, const WebString& old_value,
    const WebString& new_value, const WebString& origin,
    bool is_local_storage) {
  NOTREACHED();
}

WebSharedWorkerRepository* WorkerWebKitClientImpl::sharedWorkerRepository() {
    return 0;
}

WebMimeRegistry::SupportsType WorkerWebKitClientImpl::supportsMIMEType(
    const WebString&) {
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType WorkerWebKitClientImpl::supportsImageMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType WorkerWebKitClientImpl::supportsJavaScriptMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType WorkerWebKitClientImpl::supportsMediaMIMEType(
    const WebString&, const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType WorkerWebKitClientImpl::supportsNonImageMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebString WorkerWebKitClientImpl::mimeTypeForExtension(const WebString&) {
  NOTREACHED();
  return WebString();
}

WebString WorkerWebKitClientImpl::mimeTypeFromFile(const WebString&) {
  NOTREACHED();
  return WebString();
}

WebString WorkerWebKitClientImpl::preferredExtensionForMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebString();
}
