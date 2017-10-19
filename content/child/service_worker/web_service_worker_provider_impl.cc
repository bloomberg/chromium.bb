// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/service_worker/web_service_worker_provider_impl.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/child/service_worker/service_worker_dispatcher.h"
#include "content/child/service_worker/service_worker_handle_reference.h"
#include "content/child/service_worker/service_worker_provider_context.h"
#include "content/child/service_worker/web_service_worker_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

using blink::WebURL;

namespace content {

namespace {

const char kLostConnectionErrorMessage[] =
    "Lost connection to the service worker system.";

}  // anonymous namespace

WebServiceWorkerProviderImpl::WebServiceWorkerProviderImpl(
    ThreadSafeSender* thread_safe_sender,
    ServiceWorkerProviderContext* context)
    : thread_safe_sender_(thread_safe_sender),
      context_(context),
      weak_factory_(this) {
  DCHECK(context_);
  DCHECK(context_->provider_type() != SERVICE_WORKER_PROVIDER_FOR_WINDOW ||
         context_->container_host());
}

WebServiceWorkerProviderImpl::~WebServiceWorkerProviderImpl() {
  // Make sure the provider client is removed.
  RemoveProviderClient();
}

void WebServiceWorkerProviderImpl::SetClient(
    blink::WebServiceWorkerProviderClient* client) {
  if (!client) {
    RemoveProviderClient();
    return;
  }

  // TODO(kinuko): Here we could also register the current thread ID
  // on the provider context so that multiple WebServiceWorkerProviderImpl
  // (e.g. on document and on dedicated workers) can properly share
  // the single provider context across threads. (http://crbug.com/366538
  // for more context)
  GetDispatcher()->AddProviderClient(context_->provider_id(), client);

  if (!context_->controller())
    return;
  scoped_refptr<WebServiceWorkerImpl> controller =
      GetDispatcher()->GetOrCreateServiceWorker(
          ServiceWorkerHandleReference::Create(context_->controller()->info(),
                                               thread_safe_sender_.get()));

  // Sync the controllee's use counter with |context_|'s, which keeps
  // track of the controller's use counter.
  for (uint32_t feature : context_->used_features())
    client->CountFeature(feature);
  client->SetController(WebServiceWorkerImpl::CreateHandle(controller),
                        false /* shouldNotifyControllerChange */);
}

void WebServiceWorkerProviderImpl::RegisterServiceWorker(
    const WebURL& web_pattern,
    const WebURL& web_script_url,
    std::unique_ptr<WebServiceWorkerRegistrationCallbacks> callbacks) {
  DCHECK(callbacks);

  GURL pattern(web_pattern);
  GURL script_url(web_script_url);
  if (pattern.possibly_invalid_spec().size() > url::kMaxURLChars ||
      script_url.possibly_invalid_spec().size() > url::kMaxURLChars) {
    std::string error_message(kServiceWorkerRegisterErrorPrefix);
    error_message += "The provided scriptURL or scope is too long.";
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kSecurity,
        blink::WebString::FromASCII(error_message)));
    return;
  }

  if (!context_->container_host()) {
    std::string error_message(kServiceWorkerRegisterErrorPrefix);
    error_message += kLostConnectionErrorMessage;
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        blink::WebString::FromASCII(error_message)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN2(
      "ServiceWorker", "WebServiceWorkerProviderImpl::RegisterServiceWorker",
      this, "Scope", pattern.spec(), "Script URL", script_url.spec());
  auto options = blink::mojom::ServiceWorkerRegistrationOptions::New(pattern);
  context_->container_host()->Register(
      script_url, std::move(options),
      base::BindOnce(&WebServiceWorkerProviderImpl::OnRegistered,
                     weak_factory_.GetWeakPtr(), std::move(callbacks)));
}

void WebServiceWorkerProviderImpl::GetRegistration(
    const blink::WebURL& web_document_url,
    std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks> callbacks) {
  DCHECK(callbacks);
  GURL document_url(web_document_url);
  if (document_url.possibly_invalid_spec().size() > url::kMaxURLChars) {
    std::string error_message(kServiceWorkerGetRegistrationErrorPrefix);
    error_message += "The provided documentURL is too long.";
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kSecurity,
        blink::WebString::FromASCII(error_message)));
    return;
  }

  if (!context_->container_host()) {
    std::string error_message(kServiceWorkerGetRegistrationErrorPrefix);
    error_message += kLostConnectionErrorMessage;
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        blink::WebString::FromASCII(error_message)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN1("ServiceWorker",
                           "WebServiceWorkerProviderImpl::GetRegistration",
                           this, "Document URL", document_url.spec());
  context_->container_host()->GetRegistration(
      document_url,
      base::BindOnce(&WebServiceWorkerProviderImpl::OnDidGetRegistration,
                     weak_factory_.GetWeakPtr(), std::move(callbacks)));
}

void WebServiceWorkerProviderImpl::GetRegistrations(
    std::unique_ptr<WebServiceWorkerGetRegistrationsCallbacks> callbacks) {
  DCHECK(callbacks);
  if (!context_->container_host()) {
    std::string error_message(kServiceWorkerGetRegistrationsErrorPrefix);
    error_message += kLostConnectionErrorMessage;
    callbacks->OnError(blink::WebServiceWorkerError(
        blink::mojom::ServiceWorkerErrorType::kAbort,
        blink::WebString::FromASCII(error_message)));
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0(
      "ServiceWorker", "WebServiceWorkerProviderImpl::GetRegistrations", this);
  context_->container_host()->GetRegistrations(
      base::BindOnce(&WebServiceWorkerProviderImpl::OnDidGetRegistrations,
                     weak_factory_.GetWeakPtr(), std::move(callbacks)));
}

void WebServiceWorkerProviderImpl::GetRegistrationForReady(
    std::unique_ptr<WebServiceWorkerGetRegistrationForReadyCallbacks>
        callbacks) {
  if (!context_->container_host()) {
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0(
      "ServiceWorker", "WebServiceWorkerProviderImpl::GetRegistrationForReady",
      this);
  context_->container_host()->GetRegistrationForReady(base::BindOnce(
      &WebServiceWorkerProviderImpl::OnDidGetRegistrationForReady,
      weak_factory_.GetWeakPtr(), std::move(callbacks)));
}

bool WebServiceWorkerProviderImpl::ValidateScopeAndScriptURL(
    const blink::WebURL& scope,
    const blink::WebURL& script_url,
    blink::WebString* error_message) {
  std::string error;
  bool has_error = ServiceWorkerUtils::ContainsDisallowedCharacter(
      scope, script_url, &error);
  if (has_error)
    *error_message = blink::WebString::FromUTF8(error);
  return !has_error;
}

int WebServiceWorkerProviderImpl::provider_id() const {
  return context_->provider_id();
}

void WebServiceWorkerProviderImpl::RemoveProviderClient() {
  // Remove the provider client, but only if the dispatcher is still there.
  // (For cleanup path we don't need to bother creating a new dispatcher)
  ServiceWorkerDispatcher* dispatcher =
      ServiceWorkerDispatcher::GetThreadSpecificInstance();
  if (dispatcher)
    dispatcher->RemoveProviderClient(context_->provider_id());
}

ServiceWorkerDispatcher* WebServiceWorkerProviderImpl::GetDispatcher() {
  return ServiceWorkerDispatcher::GetThreadSpecificInstance();
}

void WebServiceWorkerProviderImpl::OnRegistered(
    std::unique_ptr<WebServiceWorkerRegistrationCallbacks> callbacks,
    blink::mojom::ServiceWorkerErrorType error,
    const base::Optional<std::string>& error_msg,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration,
    const base::Optional<ServiceWorkerVersionAttributes>& attributes) {
  TRACE_EVENT_ASYNC_END2(
      "ServiceWorker", "WebServiceWorkerProviderImpl::RegisterServiceWorker",
      this, "Error", ServiceWorkerUtils::ErrorTypeToString(error), "Message",
      error_msg ? *error_msg : "Success");
  if (error != blink::mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(error_msg);
    DCHECK(!registration);
    DCHECK(!attributes);
    callbacks->OnError(blink::WebServiceWorkerError(
        error, blink::WebString::FromASCII(*error_msg)));
    return;
  }

  DCHECK(!error_msg);
  DCHECK(registration);
  DCHECK(attributes);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationHandleId,
            registration->handle_id);
  callbacks->OnSuccess(WebServiceWorkerRegistrationImpl::CreateHandle(
      GetDispatcher()->GetOrCreateRegistrationForServiceWorkerClient(
          std::move(registration), *attributes)));
}

void WebServiceWorkerProviderImpl::OnDidGetRegistration(
    std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks> callbacks,
    blink::mojom::ServiceWorkerErrorType error,
    const base::Optional<std::string>& error_msg,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration,
    const base::Optional<ServiceWorkerVersionAttributes>& attributes) {
  TRACE_EVENT_ASYNC_END2("ServiceWorker",
                         "WebServiceWorkerProviderImpl::GetRegistration", this,
                         "Error", ServiceWorkerUtils::ErrorTypeToString(error),
                         "Message", error_msg ? *error_msg : "Success");
  if (error != blink::mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(error_msg);
    DCHECK(!registration);
    DCHECK(!attributes);
    callbacks->OnError(blink::WebServiceWorkerError(
        error, blink::WebString::FromASCII(*error_msg)));
    return;
  }

  DCHECK(!error_msg);
  DCHECK(registration);
  DCHECK(attributes);
  scoped_refptr<WebServiceWorkerRegistrationImpl> impl;
  // The handle id is invalid if no corresponding registration has been found
  // or the found one is uninstalling.
  if (registration->handle_id !=
      blink::mojom::kInvalidServiceWorkerRegistrationHandleId) {
    impl = GetDispatcher()->GetOrCreateRegistrationForServiceWorkerClient(
        std::move(registration), *attributes);
  }
  callbacks->OnSuccess(
      WebServiceWorkerRegistrationImpl::CreateHandle(std::move(impl)));
}

void WebServiceWorkerProviderImpl::OnDidGetRegistrations(
    std::unique_ptr<WebServiceWorkerGetRegistrationsCallbacks> callbacks,
    blink::mojom::ServiceWorkerErrorType error,
    const base::Optional<std::string>& error_msg,
    base::Optional<
        std::vector<blink::mojom::ServiceWorkerRegistrationObjectInfoPtr>>
        infos,
    const base::Optional<std::vector<ServiceWorkerVersionAttributes>>& attrs) {
  TRACE_EVENT_ASYNC_END2("ServiceWorker",
                         "WebServiceWorkerProviderImpl::GetRegistrations", this,
                         "Error", ServiceWorkerUtils::ErrorTypeToString(error),
                         "Message", error_msg ? *error_msg : "Success");
  if (error != blink::mojom::ServiceWorkerErrorType::kNone) {
    DCHECK(error_msg);
    DCHECK(!infos);
    DCHECK(!attrs);
    callbacks->OnError(blink::WebServiceWorkerError(
        error, blink::WebString::FromASCII(*error_msg)));
    return;
  }

  DCHECK(!error_msg);
  DCHECK(infos);
  DCHECK(attrs);
  using WebServiceWorkerRegistrationHandles =
      WebServiceWorkerProvider::WebServiceWorkerRegistrationHandles;
  std::unique_ptr<WebServiceWorkerRegistrationHandles> registrations =
      base::MakeUnique<WebServiceWorkerRegistrationHandles>(infos->size());
  for (size_t i = 0; i < infos->size(); ++i) {
    DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationHandleId,
              (*infos)[i]->handle_id);
    (*registrations)[i] = WebServiceWorkerRegistrationImpl::CreateHandle(
        GetDispatcher()->GetOrCreateRegistrationForServiceWorkerClient(
            std::move((*infos)[i]), (*attrs)[i]));
  }
  callbacks->OnSuccess(std::move(registrations));
}

void WebServiceWorkerProviderImpl::OnDidGetRegistrationForReady(
    std::unique_ptr<WebServiceWorkerGetRegistrationForReadyCallbacks> callbacks,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration,
    const base::Optional<ServiceWorkerVersionAttributes>& attributes) {
  TRACE_EVENT_ASYNC_END0(
      "ServiceWorker", "WebServiceWorkerProviderImpl::GetRegistrationForReady",
      this);
  // TODO(leonhsl): Currently the only reason that we allow nullable
  // |registration| and |attributes| is: impl of the mojo method
  // GetRegistrationForReady() needs to respond some non-sense params even if it
  // has found that the request is a bad message and has called
  // mojo::ReportBadMessage(), this is forced by Mojo, please see
  // content::ServiceWorkerProviderHost::GetRegistrationForReady(). We'll find a
  // better solution once the discussion at
  // https://groups.google.com/a/chromium.org/forum/#!topic/chromium-mojo/NNsogKNurlA
  // settled.
  CHECK(registration);
  CHECK(attributes);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerRegistrationHandleId,
            registration->handle_id);
  callbacks->OnSuccess(WebServiceWorkerRegistrationImpl::CreateHandle(
      GetDispatcher()->GetOrCreateRegistrationForServiceWorkerClient(
          std::move(registration), *attributes)));
}

}  // namespace content
