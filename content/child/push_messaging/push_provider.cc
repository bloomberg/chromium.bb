// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/push_messaging/push_provider.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_local.h"
#include "content/child/child_thread_impl.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/push_subscription_options.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushSubscription.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushSubscriptionOptions.h"

namespace content {
namespace {

int CurrentWorkerId() {
  return WorkerThread::GetCurrentId();
}

// Returns the id of the given |service_worker_registration|, which
// is only available on the implementation of the interface.
int64_t GetServiceWorkerRegistrationId(
    blink::WebServiceWorkerRegistration* service_worker_registration) {
  return static_cast<WebServiceWorkerRegistrationImpl*>(
             service_worker_registration)
      ->RegistrationId();
}

}  // namespace

blink::WebPushError PushRegistrationStatusToWebPushError(
    PushRegistrationStatus status) {
  blink::WebPushError::ErrorType error_type =
      blink::WebPushError::kErrorTypeAbort;
  switch (status) {
    case PUSH_REGISTRATION_STATUS_PERMISSION_DENIED:
      error_type = blink::WebPushError::kErrorTypeNotAllowed;
      break;
    case PUSH_REGISTRATION_STATUS_SENDER_ID_MISMATCH:
      error_type = blink::WebPushError::kErrorTypeInvalidState;
      break;
    case PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE:
    case PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER:
    case PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE:
    case PUSH_REGISTRATION_STATUS_LIMIT_REACHED:
    case PUSH_REGISTRATION_STATUS_SERVICE_ERROR:
    case PUSH_REGISTRATION_STATUS_NO_SENDER_ID:
    case PUSH_REGISTRATION_STATUS_STORAGE_ERROR:
    case PUSH_REGISTRATION_STATUS_SUCCESS_FROM_CACHE:
    case PUSH_REGISTRATION_STATUS_NETWORK_ERROR:
    case PUSH_REGISTRATION_STATUS_INCOGNITO_PERMISSION_DENIED:
    case PUSH_REGISTRATION_STATUS_PUBLIC_KEY_UNAVAILABLE:
    case PUSH_REGISTRATION_STATUS_MANIFEST_EMPTY_OR_MISSING:
    case PUSH_REGISTRATION_STATUS_STORAGE_CORRUPT:
    case PUSH_REGISTRATION_STATUS_RENDERER_SHUTDOWN:
      error_type = blink::WebPushError::kErrorTypeAbort;
      break;
  }
  return blink::WebPushError(
      error_type,
      blink::WebString::FromUTF8(PushRegistrationStatusToString(status)));
}

static base::LazyInstance<base::ThreadLocalPointer<PushProvider>>::Leaky
    g_push_provider_tls = LAZY_INSTANCE_INITIALIZER;

PushProvider::PushProvider(const scoped_refptr<base::SingleThreadTaskRunner>&
                               main_thread_task_runner) {
  DCHECK(main_thread_task_runner);

  auto request = mojo::MakeRequest(&push_messaging_manager_);
  if (!main_thread_task_runner->BelongsToCurrentThread()) {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&PushProvider::GetInterface, base::Passed(&request)));
  } else {
    GetInterface(std::move(request));
  }
  g_push_provider_tls.Pointer()->Set(this);
}

PushProvider::~PushProvider() {
  g_push_provider_tls.Pointer()->Set(nullptr);
}

PushProvider* PushProvider::ThreadSpecificInstance(
    const scoped_refptr<base::SingleThreadTaskRunner>&
        main_thread_task_runner) {
  if (g_push_provider_tls.Pointer()->Get())
    return g_push_provider_tls.Pointer()->Get();

  PushProvider* provider = new PushProvider(main_thread_task_runner);
  if (CurrentWorkerId())
    WorkerThread::AddObserver(provider);
  return provider;
}

// static
void PushProvider::GetInterface(mojom::PushMessagingRequest request) {
  if (ChildThreadImpl::current()) {
    ChildThreadImpl::current()->GetConnector()->BindInterface(
        mojom::kBrowserServiceName, std::move(request));
  }
}

void PushProvider::WillStopCurrentWorkerThread() {
  delete this;
}

void PushProvider::Subscribe(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    const blink::WebPushSubscriptionOptions& options,
    bool user_gesture,
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);

  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  PushSubscriptionOptions content_options;
  content_options.user_visible_only = options.user_visible_only;

  // Just treat the server key as a string of bytes and pass it to the push
  // service.
  content_options.sender_info = options.application_server_key.Latin1();

  push_messaging_manager_->Subscribe(
      ChildProcessHost::kInvalidUniqueID, service_worker_registration_id,
      content_options, user_gesture,
      // Safe to use base::Unretained because |push_messaging_manager_ |is owned
      // by |this|.
      base::Bind(&PushProvider::DidSubscribe, base::Unretained(this),
                 base::Passed(&callbacks)));
}

void PushProvider::DidSubscribe(
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
    content::PushRegistrationStatus status,
    const base::Optional<GURL>& endpoint,
    const base::Optional<content::PushSubscriptionOptions>& options,
    const base::Optional<std::vector<uint8_t>>& p256dh,
    const base::Optional<std::vector<uint8_t>>& auth) {
  DCHECK(callbacks);

  if (status == PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE ||
      status == PUSH_REGISTRATION_STATUS_SUCCESS_FROM_CACHE) {
    DCHECK(endpoint);
    DCHECK(options);
    DCHECK(p256dh);
    DCHECK(auth);

    callbacks->OnSuccess(base::MakeUnique<blink::WebPushSubscription>(
        endpoint.value(), options.value().user_visible_only,
        blink::WebString::FromLatin1(options.value().sender_info),
        p256dh.value(), auth.value()));
  } else {
    callbacks->OnError(PushRegistrationStatusToWebPushError(status));
  }
}

void PushProvider::Unsubscribe(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    std::unique_ptr<blink::WebPushUnsubscribeCallbacks> callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);

  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);

  push_messaging_manager_->Unsubscribe(
      service_worker_registration_id,
      // Safe to use base::Unretained because |push_messaging_manager_ |is owned
      // by |this|.
      base::Bind(&PushProvider::DidUnsubscribe, base::Unretained(this),
                 base::Passed(&callbacks)));
}

void PushProvider::DidUnsubscribe(
    std::unique_ptr<blink::WebPushUnsubscribeCallbacks> callbacks,
    blink::WebPushError::ErrorType error_type,
    bool did_unsubscribe,
    const base::Optional<std::string>& error_message) {
  DCHECK(callbacks);

  // ErrorTypeNone indicates success.
  if (error_type == blink::WebPushError::kErrorTypeNone) {
    callbacks->OnSuccess(did_unsubscribe);
  } else {
    callbacks->OnError(blink::WebPushError(
        error_type, blink::WebString::FromUTF8(error_message->c_str())));
  }
}

void PushProvider::GetSubscription(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);

  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);

  push_messaging_manager_->GetSubscription(
      service_worker_registration_id,
      // Safe to use base::Unretained because |push_messaging_manager_ |is owned
      // by |this|.
      base::Bind(&PushProvider::DidGetSubscription, base::Unretained(this),
                 base::Passed(&callbacks)));
}

void PushProvider::DidGetSubscription(
    std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
    content::PushGetRegistrationStatus status,
    const base::Optional<GURL>& endpoint,
    const base::Optional<content::PushSubscriptionOptions>& options,
    const base::Optional<std::vector<uint8_t>>& p256dh,
    const base::Optional<std::vector<uint8_t>>& auth) {
  DCHECK(callbacks);

  if (status == PUSH_GETREGISTRATION_STATUS_SUCCESS) {
    DCHECK(endpoint);
    DCHECK(options);
    DCHECK(p256dh);
    DCHECK(auth);

    callbacks->OnSuccess(base::MakeUnique<blink::WebPushSubscription>(
        endpoint.value(), options.value().user_visible_only,
        blink::WebString::FromLatin1(options.value().sender_info),
        p256dh.value(), auth.value()));
  } else {
    // We are only expecting an error if we can't find a registration.
    callbacks->OnSuccess(nullptr);
  }
}

void PushProvider::GetPermissionStatus(
    blink::WebServiceWorkerRegistration* service_worker_registration,
    const blink::WebPushSubscriptionOptions& options,
    std::unique_ptr<blink::WebPushPermissionStatusCallbacks> callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);

  int64_t service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);

  push_messaging_manager_->GetPermissionStatus(
      service_worker_registration_id, options.user_visible_only,
      // Safe to use base::Unretained because |push_messaging_manager_ |is owned
      // by |this|.
      base::Bind(&PushProvider::DidGetPermissionStatus, base::Unretained(this),
                 base::Passed(&callbacks)));
}

void PushProvider::DidGetPermissionStatus(
    std::unique_ptr<blink::WebPushPermissionStatusCallbacks> callbacks,
    blink::WebPushError::ErrorType error_type,
    blink::WebPushPermissionStatus status) {
  DCHECK(callbacks);
  // ErrorTypeNone indicates success.
  if (error_type == blink::WebPushError::kErrorTypeNone) {
    callbacks->OnSuccess(status);
  } else {
    std::string error_message;
    if (error_type == blink::WebPushError::kErrorTypeNotSupported) {
      error_message =
          "Push subscriptions that don't enable userVisibleOnly are not "
          "supported.";
    }
    callbacks->OnError(blink::WebPushError(
        error_type, blink::WebString::FromUTF8(error_message)));
  }
}

}  // namespace content
