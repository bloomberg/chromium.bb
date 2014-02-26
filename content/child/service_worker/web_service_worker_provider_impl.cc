// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_provider_impl.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "content/child/child_thread.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerProviderClient.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebURL;

namespace content {

WebServiceWorkerProviderImpl::WebServiceWorkerProviderImpl(
    ThreadSafeSender* thread_safe_sender)
    : thread_safe_sender_(thread_safe_sender),
      client_(NULL) {
}

WebServiceWorkerProviderImpl::~WebServiceWorkerProviderImpl() {
}

void WebServiceWorkerProviderImpl::setClient(
    blink::WebServiceWorkerProviderClient* client) {
  // TODO(kinuko): We should register the client with provider_id
  // so that the client can start listening events for the provider.
  client_ = client;
}

void WebServiceWorkerProviderImpl::registerServiceWorker(
    const WebURL& pattern,
    const WebURL& script_url,
    WebServiceWorkerCallbacks* callbacks) {
  ServiceWorkerDispatcher::ThreadSpecificInstance(thread_safe_sender_)
      ->RegisterServiceWorker(pattern, script_url, callbacks);
}

void WebServiceWorkerProviderImpl::unregisterServiceWorker(
    const WebURL& pattern,
    WebServiceWorkerCallbacks* callbacks) {
  ServiceWorkerDispatcher::ThreadSpecificInstance(thread_safe_sender_)
      ->UnregisterServiceWorker(pattern, callbacks);
}

}  // namespace content
