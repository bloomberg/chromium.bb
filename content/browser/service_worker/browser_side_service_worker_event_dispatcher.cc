// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/browser_side_service_worker_event_dispatcher.h"

#include "content/browser/service_worker/service_worker_version.h"

namespace content {

BrowserSideServiceWorkerEventDispatcher::
    BrowserSideServiceWorkerEventDispatcher(
        ServiceWorkerVersion* receiver_version)
    : receiver_version_(receiver_version) {
  DCHECK(receiver_version_);
}

BrowserSideServiceWorkerEventDispatcher::
    ~BrowserSideServiceWorkerEventDispatcher() = default;

mojom::ServiceWorkerEventDispatcherPtrInfo
BrowserSideServiceWorkerEventDispatcher::CreateEventDispatcherPtrInfo() {
  mojom::ServiceWorkerEventDispatcherPtrInfo dispatcher_ptr_info;
  incoming_event_bindings_.AddBinding(this,
                                      mojo::MakeRequest(&dispatcher_ptr_info));
  return dispatcher_ptr_info;
}

void BrowserSideServiceWorkerEventDispatcher::DispatchInstallEvent(
    mojom::ServiceWorkerInstallEventMethodsAssociatedPtrInfo client,
    DispatchInstallEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchActivateEvent(
    DispatchActivateEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchBackgroundFetchAbortEvent(
    const std::string& tag,
    DispatchBackgroundFetchAbortEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchBackgroundFetchClickEvent(
    const std::string& tag,
    mojom::BackgroundFetchState state,
    DispatchBackgroundFetchClickEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchBackgroundFetchFailEvent(
    const std::string& tag,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    DispatchBackgroundFetchFailEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchBackgroundFetchedEvent(
    const std::string& tag,
    const std::vector<BackgroundFetchSettledFetch>& fetches,
    DispatchBackgroundFetchedEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchExtendableMessageEvent(
    mojom::ExtendableMessageEventPtr event,
    DispatchExtendableMessageEventCallback callback) {
  NOTIMPLEMENTED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchFetchEvent(
    int fetch_event_id,
    const ServiceWorkerFetchRequest& request,
    mojom::FetchEventPreloadHandlePtr preload_handle,
    mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    DispatchFetchEventCallback callback) {
  NOTIMPLEMENTED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchNotificationClickEvent(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    int action_index,
    const base::Optional<base::string16>& reply,
    DispatchNotificationClickEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchNotificationCloseEvent(
    const std::string& notification_id,
    const PlatformNotificationData& notification_data,
    DispatchNotificationCloseEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchPushEvent(
    const PushEventPayload& payload,
    DispatchPushEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchSyncEvent(
    const std::string& tag,
    blink::mojom::BackgroundSyncEventLastChance last_chance,
    DispatchSyncEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchAbortPaymentEvent(
    int payment_request_id,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchAbortPaymentEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchCanMakePaymentEvent(
    int payment_request_id,
    payments::mojom::CanMakePaymentEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchCanMakePaymentEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::DispatchPaymentRequestEvent(
    int payment_request_id,
    payments::mojom::PaymentRequestEventDataPtr event_data,
    payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
    DispatchPaymentRequestEventCallback callback) {
  NOTREACHED();
}

void BrowserSideServiceWorkerEventDispatcher::Ping(PingCallback callback) {
  NOTREACHED();
}

}  // namespace content
