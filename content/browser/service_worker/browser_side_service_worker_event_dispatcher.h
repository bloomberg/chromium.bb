// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_BROWSER_SIDE_SERVICE_WORKER_EVENT_DISPATCHER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_BROWSER_SIDE_SERVICE_WORKER_EVENT_DISPATCHER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace content {

class ServiceWorkerVersion;

// Temporary class until we start to have ServiceWorkerVersion-like object
// in the renderer.
//
// BrowserSideServiceWorkerEventDispatcher forward events to a
// ServiceWorkerVersion. It's the browser-side implementation of
// mojom::ServiceWorkerEventDispatcher, used by the renderer to dispatch
// events initiated by a controllee to its controlling service worker.
// The short-term plan is for it to be used to dispatch fetch events directly
// to the service worker instead of relying on ServiceWorkerURLRequestJob.
// The (slightly) longer term plan is to have this in the renderer and
// dispatch events directly from the page to the ServiceWorker without going
// through the browser process.
//
// TODO(kinuko): this should probably eventually replace ServiceWorkerHandle
// and get state changes.
class BrowserSideServiceWorkerEventDispatcher
    : public mojom::ServiceWorkerEventDispatcher {
 public:
  explicit BrowserSideServiceWorkerEventDispatcher(
      ServiceWorkerVersion* receiver_version);
  ~BrowserSideServiceWorkerEventDispatcher() override;

  mojom::ServiceWorkerEventDispatcherPtrInfo CreateEventDispatcherPtrInfo();

  // mojom::ServiceWorkerEventDispatcher:
  void DispatchInstallEvent(
      mojom::ServiceWorkerInstallEventMethodsAssociatedPtrInfo client,
      DispatchInstallEventCallback callback) override;
  void DispatchActivateEvent(DispatchActivateEventCallback callback) override;
  void DispatchBackgroundFetchAbortEvent(
      const std::string& tag,
      DispatchBackgroundFetchAbortEventCallback callback) override;
  void DispatchBackgroundFetchClickEvent(
      const std::string& tag,
      mojom::BackgroundFetchState state,
      DispatchBackgroundFetchClickEventCallback callback) override;
  void DispatchBackgroundFetchFailEvent(
      const std::string& tag,
      const std::vector<BackgroundFetchSettledFetch>& fetches,
      DispatchBackgroundFetchFailEventCallback callback) override;
  void DispatchBackgroundFetchedEvent(
      const std::string& tag,
      const std::vector<BackgroundFetchSettledFetch>& fetches,
      DispatchBackgroundFetchedEventCallback callback) override;
  void DispatchExtendableMessageEvent(
      mojom::ExtendableMessageEventPtr event,
      DispatchExtendableMessageEventCallback callback) override;
  void DispatchFetchEvent(
      int fetch_event_id,
      const ServiceWorkerFetchRequest& request,
      mojom::FetchEventPreloadHandlePtr preload_handle,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override;
  void DispatchNotificationClickEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      int action_index,
      const base::Optional<base::string16>& reply,
      DispatchNotificationClickEventCallback callback) override;
  void DispatchNotificationCloseEvent(
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      DispatchNotificationCloseEventCallback callback) override;
  void DispatchPushEvent(const PushEventPayload& payload,
                         DispatchPushEventCallback callback) override;
  void DispatchSyncEvent(
      const std::string& tag,
      blink::mojom::BackgroundSyncEventLastChance last_chance,
      DispatchSyncEventCallback callback) override;
  void DispatchAbortPaymentEvent(
      int payment_request_id,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchAbortPaymentEventCallback callback) override;
  void DispatchCanMakePaymentEvent(
      int payment_request_id,
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchCanMakePaymentEventCallback callback) override;
  void DispatchPaymentRequestEvent(
      int payment_request_id,
      payments::mojom::PaymentRequestEventDataPtr event_data,
      payments::mojom::PaymentHandlerResponseCallbackPtr response_callback,
      DispatchPaymentRequestEventCallback callback) override;
  void Ping(PingCallback callback) override;

 private:
  // Connected to ServiceWorkerNetworkProvider's of the controllees (and of the
  // browser, when this object starts to live in the renderer process).
  mojo::BindingSet<mojom::ServiceWorkerEventDispatcher>
      incoming_event_bindings_;

  // Not owned; this proxy should go away before the version goes away.
  ServiceWorkerVersion* receiver_version_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSideServiceWorkerEventDispatcher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_BROWSER_SIDE_SERVICE_WORKER_EVENT_DISPATCHER_H_
