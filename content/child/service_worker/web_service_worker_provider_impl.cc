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
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebURL;

namespace content {

namespace {

// Must be unique in the child process.
int GetNextProviderId() {
  static base::StaticAtomicSequenceNumber sequence;
  return sequence.GetNext() + 1;  // We want to start at 1.
}

}  // namespace

WebServiceWorkerProviderImpl::WebServiceWorkerProviderImpl(
    ThreadSafeSender* thread_safe_sender,
    scoped_ptr<blink::WebServiceWorkerProviderClient> client)
    : provider_id_(GetNextProviderId()),
      thread_safe_sender_(thread_safe_sender),
      client_(client.Pass()) {
  thread_safe_sender_->Send(
      new ServiceWorkerHostMsg_ProviderCreated(provider_id_));
}

WebServiceWorkerProviderImpl::~WebServiceWorkerProviderImpl() {
  thread_safe_sender_->Send(
      new ServiceWorkerHostMsg_ProviderDestroyed(provider_id_));
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
