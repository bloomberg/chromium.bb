// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_provider_impl.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "content/child/child_thread.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/service_worker_registration_handle_reference.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "third_party/WebKit/public/platform/WebServiceWorkerProviderClient.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebURL;

namespace content {

WebServiceWorkerProviderImpl::WebServiceWorkerProviderImpl(
    ThreadSafeSender* thread_safe_sender,
    ServiceWorkerProviderContext* context)
    : thread_safe_sender_(thread_safe_sender),
      context_(context),
      provider_id_(context->provider_id()) {
}

WebServiceWorkerProviderImpl::~WebServiceWorkerProviderImpl() {
  // Make sure the script client is removed.
  RemoveScriptClient();
}

void WebServiceWorkerProviderImpl::setClient(
    blink::WebServiceWorkerProviderClient* client) {
  if (!client) {
    RemoveScriptClient();
    return;
  }

  // TODO(kinuko): Here we could also register the current thread ID
  // on the provider context so that multiple WebServiceWorkerProviderImpl
  // (e.g. on document and on dedicated workers) can properly share
  // the single provider context across threads. (http://crbug.com/366538
  // for more context)
  GetDispatcher()->AddScriptClient(provider_id_, client);

  if (!context_->registration()) {
    // This provider is not associated with any registration.
    return;
  }

  // Set .ready if the associated registration has the active service worker.
  if (context_->active_handle_id() != kInvalidServiceWorkerHandleId) {
    WebServiceWorkerRegistrationImpl* registration =
        GetDispatcher()->FindServiceWorkerRegistration(
            context_->registration()->info(), false);
    if (!registration) {
      registration = GetDispatcher()->CreateServiceWorkerRegistration(
          context_->registration()->info(), false);
      ServiceWorkerVersionAttributes attrs = context_->GetVersionAttributes();
      registration->SetInstalling(
          GetDispatcher()->GetServiceWorker(attrs.installing, false));
      registration->SetWaiting(
          GetDispatcher()->GetServiceWorker(attrs.waiting, false));
      registration->SetActive(
          GetDispatcher()->GetServiceWorker(attrs.active, false));
    }
    client->setReadyRegistration(registration);
  }

  if (context_->controller_handle_id() != kInvalidServiceWorkerHandleId) {
    client->setController(GetDispatcher()->GetServiceWorker(
        context_->controller()->info(), false));
  }
}

void WebServiceWorkerProviderImpl::registerServiceWorker(
    const WebURL& pattern,
    const WebURL& script_url,
    WebServiceWorkerRegistrationCallbacks* callbacks) {
  GetDispatcher()->RegisterServiceWorker(
      provider_id_, pattern, script_url, callbacks);
}

void WebServiceWorkerProviderImpl::unregisterServiceWorker(
    const WebURL& pattern,
    WebServiceWorkerUnregistrationCallbacks* callbacks) {
  GetDispatcher()->UnregisterServiceWorker(
      provider_id_, pattern, callbacks);
}

void WebServiceWorkerProviderImpl::getRegistration(
    const blink::WebURL& document_url,
    WebServiceWorkerRegistrationCallbacks* callbacks) {
  GetDispatcher()->GetRegistration(provider_id_, document_url, callbacks);
}

void WebServiceWorkerProviderImpl::RemoveScriptClient() {
  // Remove the script client, but only if the dispatcher is still there.
  // (For cleanup path we don't need to bother creating a new dispatcher)
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveScriptClient(provider_id_);
}

ServiceWorkerDispatcher* WebServiceWorkerProviderImpl::GetDispatcher() {
  return ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance(
      thread_safe_sender_.get());
}

}  // namespace content
