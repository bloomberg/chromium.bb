// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_provider_impl.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "content/child/child_thread.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/thread_safe_sender.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebURL;

namespace content {

WebServiceWorkerProviderImpl::WebServiceWorkerProviderImpl(
    ThreadSafeSender* thread_safe_sender,
    int provider_id)
    : thread_safe_sender_(thread_safe_sender),
      provider_id_(provider_id) {
}

WebServiceWorkerProviderImpl::~WebServiceWorkerProviderImpl() {
  // Make sure the script client is removed.
  GetDispatcher()->RemoveScriptClient(provider_id_);
}

void WebServiceWorkerProviderImpl::setClient(
    blink::WebServiceWorkerProviderClient* client) {
  if (client)
    GetDispatcher()->AddScriptClient(provider_id_, client);
  else
    GetDispatcher()->RemoveScriptClient(provider_id_);
}

void WebServiceWorkerProviderImpl::registerServiceWorker(
    const WebURL& pattern,
    const WebURL& script_url,
    WebServiceWorkerCallbacks* callbacks) {
  GetDispatcher()->RegisterServiceWorker(
      provider_id_, pattern, script_url, callbacks);
}

void WebServiceWorkerProviderImpl::unregisterServiceWorker(
    const WebURL& pattern,
    WebServiceWorkerCallbacks* callbacks) {
  GetDispatcher()->UnregisterServiceWorker(
      provider_id_, pattern, callbacks);
}

ServiceWorkerDispatcher* WebServiceWorkerProviderImpl::GetDispatcher() {
  return ServiceWorkerDispatcher::ThreadSpecificInstance(thread_safe_sender_);
}

}  // namespace content
