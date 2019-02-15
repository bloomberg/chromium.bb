// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/fake_service_worker.h"

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"

namespace content {

FakeServiceWorker::FakeServiceWorker(EmbeddedWorkerTestHelper* helper)
    : helper_(helper), binding_(this) {}

FakeServiceWorker::~FakeServiceWorker() = default;

void FakeServiceWorker::Bind(blink::mojom::ServiceWorkerRequest request) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(base::BindOnce(
      &FakeServiceWorker::OnConnectionError, base::Unretained(this)));
}

void FakeServiceWorker::RunUntilInitializeGlobalScope() {
  if (host_)
    return;
  base::RunLoop loop;
  quit_closure_for_initialize_global_scope_ = loop.QuitClosure();
  loop.Run();
}

void FakeServiceWorker::InitializeGlobalScope(
    blink::mojom::ServiceWorkerHostAssociatedPtrInfo service_worker_host,
    blink::mojom::ServiceWorkerRegistrationObjectInfoPtr registration_info) {
  host_.Bind(std::move(service_worker_host));

  // Enable callers to use these endpoints without us actually binding them
  // to an implementation.
  mojo::AssociateWithDisconnectedPipe(registration_info->request.PassHandle());
  if (registration_info->installing) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->installing->request.PassHandle());
  }
  if (registration_info->waiting) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->waiting->request.PassHandle());
  }
  if (registration_info->active) {
    mojo::AssociateWithDisconnectedPipe(
        registration_info->active->request.PassHandle());
  }

  registration_info_ = std::move(registration_info);
  if (quit_closure_for_initialize_global_scope_)
    std::move(quit_closure_for_initialize_global_scope_).Run();
}

void FakeServiceWorker::DispatchInstallEvent(
    DispatchInstallEventCallback callback) {
  helper_->OnInstallEventStub(std::move(callback));
}

void FakeServiceWorker::DispatchActivateEvent(
    DispatchActivateEventCallback callback) {
  helper_->OnActivateEventStub(std::move(callback));
}

void FakeServiceWorker::DispatchBackgroundFetchAbortEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchAbortEventCallback callback) {
  helper_->OnBackgroundFetchAbortEventStub(std::move(registration),
                                           std::move(callback));
}

void FakeServiceWorker::DispatchBackgroundFetchClickEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchClickEventCallback callback) {
  helper_->OnBackgroundFetchClickEventStub(std::move(registration),
                                           std::move(callback));
}

void FakeServiceWorker::DispatchBackgroundFetchFailEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchFailEventCallback callback) {
  helper_->OnBackgroundFetchFailEventStub(std::move(registration),
                                          std::move(callback));
}

void FakeServiceWorker::DispatchBackgroundFetchSuccessEvent(
    blink::mojom::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchSuccessEventCallback callback) {
  helper_->OnBackgroundFetchSuccessEventStub(std::move(registration),
                                             std::move(callback));
}

void FakeServiceWorker::DispatchCookieChangeEvent(
    const net::CanonicalCookie& cookie,
    ::network::mojom::CookieChangeCause cause,
    DispatchCookieChangeEventCallback callback) {
  helper_->OnCookieChangeEventStub(cookie, cause, std::move(callback));
}

void FakeServiceWorker::DispatchFetchEvent(
    blink::mojom::DispatchFetchEventParamsPtr params,
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    DispatchFetchEventCallback callback) {
  helper_->OnFetchEventStub(0 /* embedded_worker_id_ */,
                            std::move(params->request),
                            std::move(params->preload_handle),
                            std::move(response_callback), std::move(callback));
}

void FakeServiceWorker::DispatchNotificationClickEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    DispatchNotificationClickEventCallback callback) {
  helper_->OnNotificationClickEventStub(notification_id, notification_data,
                                        action_index, reply,
                                        std::move(callback));
}

void FakeServiceWorker::DispatchNotificationCloseEvent(
    const std::string& notification_id,
    const blink::PlatformNotificationData& notification_data,
    DispatchNotificationCloseEventCallback callback) {
  helper_->OnNotificationCloseEventStub(notification_id, notification_data,
                                        std::move(callback));
}

void FakeServiceWorker::DispatchPushEvent(
    const base::Optional<std::string>& payload,
    DispatchPushEventCallback callback) {
  helper_->OnPushEventStub(payload, std::move(callback));
}

void FakeServiceWorker::DispatchSyncEvent(const std::string& tag,
                                          bool last_chance,
                                          base::TimeDelta timeout,
                                          DispatchSyncEventCallback callback) {
  NOTIMPLEMENTED();
}

void FakeServiceWorker::DispatchAbortPaymentEvent(
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchAbortPaymentEventCallback callback) {
  helper_->OnAbortPaymentEventStub(std::move(response_callback),
                                   std::move(callback));
}

void FakeServiceWorker::DispatchCanMakePaymentEvent(
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchCanMakePaymentEventCallback callback) {
  helper_->OnCanMakePaymentEventStub(
      std::move(event_data), std::move(response_callback), std::move(callback));
}

void FakeServiceWorker::DispatchPaymentRequestEvent(
    payments::mojom::PaymentRequestEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchPaymentRequestEventCallback callback) {
  helper_->OnPaymentRequestEventStub(
      std::move(event_data), std::move(response_callback), std::move(callback));
}

void FakeServiceWorker::DispatchExtendableMessageEvent(
    blink::mojom::ExtendableMessageEventPtr event,
    DispatchExtendableMessageEventCallback callback) {
  helper_->OnExtendableMessageEventStub(std::move(event), std::move(callback));
}

void FakeServiceWorker::DispatchExtendableMessageEventWithCustomTimeout(
    blink::mojom::ExtendableMessageEventPtr event,
    base::TimeDelta timeout,
    DispatchExtendableMessageEventWithCustomTimeoutCallback callback) {
  helper_->OnExtendableMessageEventStub(std::move(event), std::move(callback));
}

void FakeServiceWorker::Ping(PingCallback callback) {
  std::move(callback).Run();
}

void FakeServiceWorker::SetIdleTimerDelayToZero() {
  helper_->OnSetIdleTimerDelayToZero(0 /* embedded_worker_id_ */);
}

void FakeServiceWorker::OnConnectionError() {
  // Destroys |this|.
  helper_->RemoveServiceWorker(this);
}

}  // namespace content
