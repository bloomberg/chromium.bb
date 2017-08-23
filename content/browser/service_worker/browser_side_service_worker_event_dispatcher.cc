// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/browser_side_service_worker_event_dispatcher.h"

#include "base/callback.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

// BrowserSideServiceWorkerEventDispatcher::ResponseCallback ----------------

// ResponseCallback is owned by the callback that is passed to
// ServiceWorkerVersion::StartRequest*(), and held in pending_requests_
// until FinishRequest() is called.
//
// This forwards callbacks to the controllee-side's ResponseCallback
// (|forwarding_callback|, implemented by ServiceWorkerSubresourceLoader),
// which is the original initiator of the FetchEvent for subresource fetch:
//
// SubresourceLoader <-> BrowserSideEventDispatcher <-> EventDispatcher
//   (controllee)            [browser-process]            (controller)
//
// (Eventually SubresourceLoader should directly dispatch events to
// the controller's EventDispatcher)
class BrowserSideServiceWorkerEventDispatcher::ResponseCallback
    : public mojom::ServiceWorkerFetchResponseCallback {
 public:
  // |forwarding_callback| is implemented by the controllee's SubresourceLoader.
  ResponseCallback(
      mojom::ServiceWorkerFetchResponseCallbackRequest request,
      mojom::ServiceWorkerFetchResponseCallbackPtr forwarding_callback,
      ServiceWorkerVersion* version)
      : binding_(this, std::move(request)),
        forwarding_callback_(std::move(forwarding_callback)),
        version_(version) {}
  ~ResponseCallback() override { DCHECK(fetch_event_id_.has_value()); }

  void set_fetch_event_id(int id) {
    DCHECK(!fetch_event_id_);
    fetch_event_id_ = id;
  }

  // Implements mojom::ServiceWorkerFetchResponseCallback.
  void OnResponse(const ServiceWorkerResponse& response,
                  base::Time dispatch_event_time) override {
    // Calling FinishRequest will destroy |this|, so save the callback now.
    auto callback = std::move(forwarding_callback_);
    if (!version_->FinishRequest(fetch_event_id_.value(),
                                 true /* was_handled */, dispatch_event_time)) {
      NOTREACHED() << "Should only receive one reply per event";
    }
    callback->OnResponse(response, dispatch_event_time);
  }
  void OnResponseBlob(const ServiceWorkerResponse& response,
                      storage::mojom::BlobPtr body_as_blob,
                      base::Time dispatch_event_time) override {
    // Calling FinishRequest will destroy |this|, so save the callback now.
    auto callback = std::move(forwarding_callback_);
    if (!version_->FinishRequest(fetch_event_id_.value(),
                                 true /* was_handled */, dispatch_event_time)) {
      NOTREACHED() << "Should only receive one reply per event";
    }
    callback->OnResponseBlob(response, std::move(body_as_blob),
                             dispatch_event_time);
  }
  void OnResponseStream(
      const ServiceWorkerResponse& response,
      blink::mojom::ServiceWorkerStreamHandlePtr body_as_stream,
      base::Time dispatch_event_time) override {
    // Calling FinishRequest will destroy |this|, so save the callback now.
    auto callback = std::move(forwarding_callback_);
    if (!version_->FinishRequest(fetch_event_id_.value(),
                                 true /* was_handled */, dispatch_event_time)) {
      NOTREACHED() << "Should only receive one reply per event";
    }
    callback->OnResponseStream(response, std::move(body_as_stream),
                               dispatch_event_time);
  }
  void OnFallback(base::Time dispatch_event_time) override {
    // Calling FinishRequest will destroy |this|, so save the callback now.
    auto callback = std::move(forwarding_callback_);
    if (!version_->FinishRequest(fetch_event_id_.value(),
                                 false /* was_handled */,
                                 dispatch_event_time)) {
      NOTREACHED() << "Should only receive one reply per event";
    }
    callback->OnFallback(dispatch_event_time);
  }

 private:
  mojo::Binding<mojom::ServiceWorkerFetchResponseCallback> binding_;
  mojom::ServiceWorkerFetchResponseCallbackPtr forwarding_callback_;
  ServiceWorkerVersion* version_;  // Owns |this| via pending_requests_.
  // Must be set to a non-nullopt value before the corresponding mojo
  // handle is passed to the other end (i.e. before any of OnResponse*
  // is called).
  base::Optional<int> fetch_event_id_;
  DISALLOW_COPY_AND_ASSIGN(ResponseCallback);
};

// BrowserSideServiceWorkerEventDispatcher ----------------------------------

BrowserSideServiceWorkerEventDispatcher::
    BrowserSideServiceWorkerEventDispatcher(
        ServiceWorkerVersion* receiver_version)
    : receiver_version_(receiver_version), weak_factory_(this) {
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
    int incoming_fetch_event_id,
    const ServiceWorkerFetchRequest& request,
    mojom::FetchEventPreloadHandlePtr preload_handle,
    mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    DispatchFetchEventCallback callback) {
  DVLOG(1) << "FetchEvent [" << incoming_fetch_event_id << "] "
           << request.url.spec();
  DCHECK(fetch_event_callbacks_.find(incoming_fetch_event_id) ==
         fetch_event_callbacks_.end());
  fetch_event_callbacks_[incoming_fetch_event_id] = std::move(callback);

  if (receiver_version_->running_status() == EmbeddedWorkerStatus::RUNNING) {
    DispatchFetchEventInternal(incoming_fetch_event_id, request,
                               std::move(preload_handle),
                               std::move(response_callback));
    return;
  }

  // TODO(kinuko): Support ForeignFetch if it turns out that we need it.
  DCHECK_NE(ServiceWorkerFetchType::FOREIGN_FETCH, request.fetch_type);
  // Only subresource requests should come here (as the other end of pipe for
  // this event dispatcher is passed to the renderer's SubresourceLoader).
  DCHECK_EQ(REQUEST_CONTEXT_FRAME_TYPE_NONE, request.frame_type);

  receiver_version_->RunAfterStartWorker(
      ServiceWorkerMetrics::EventType::FETCH_SUB_RESOURCE,
      base::Bind(
          &BrowserSideServiceWorkerEventDispatcher::DispatchFetchEventInternal,
          weak_factory_.GetWeakPtr(), incoming_fetch_event_id, request,
          base::Passed(&preload_handle), base::Passed(&response_callback)),
      base::Bind(
          &BrowserSideServiceWorkerEventDispatcher::DidFailToStartWorker,
          weak_factory_.GetWeakPtr(),
          base::Bind(
              &BrowserSideServiceWorkerEventDispatcher::DidFailToDispatchFetch,
              weak_factory_.GetWeakPtr(), incoming_fetch_event_id, nullptr)));
}

void BrowserSideServiceWorkerEventDispatcher::DispatchFetchEventInternal(
    int incoming_fetch_event_id,
    const ServiceWorkerFetchRequest& request,
    mojom::FetchEventPreloadHandlePtr preload_handle,
    mojom::ServiceWorkerFetchResponseCallbackPtr incoming_response_callback) {
  mojom::ServiceWorkerFetchResponseCallbackPtr mojo_response_callback_ptr;
  auto response_callback = base::MakeUnique<ResponseCallback>(
      mojo::MakeRequest(&mojo_response_callback_ptr),
      std::move(incoming_response_callback), receiver_version_);
  ResponseCallback* response_callback_rawptr = response_callback.get();

  // TODO(kinuko): No timeout support for now; support timeout.
  int fetch_event_id = receiver_version_->StartRequest(
      ServiceWorkerMetrics::EventType::FETCH_SUB_RESOURCE,
      base::Bind(
          &BrowserSideServiceWorkerEventDispatcher::DidFailToDispatchFetch,
          weak_factory_.GetWeakPtr(), incoming_fetch_event_id,
          base::Passed(&response_callback)));
  int event_finish_id = receiver_version_->StartRequest(
      ServiceWorkerMetrics::EventType::FETCH_WAITUNTIL,
      // NoOp as the same error callback should be handled by the other
      // StartRequest above.
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));

  response_callback_rawptr->set_fetch_event_id(fetch_event_id);

  receiver_version_->event_dispatcher()->DispatchFetchEvent(
      fetch_event_id, request, std::move(preload_handle),
      std::move(mojo_response_callback_ptr),
      base::BindOnce(
          &BrowserSideServiceWorkerEventDispatcher::DidDispatchFetchEvent,
          weak_factory_.GetWeakPtr(), incoming_fetch_event_id,
          event_finish_id));
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

void BrowserSideServiceWorkerEventDispatcher::DidFailToStartWorker(
    const StatusCallback& callback,
    ServiceWorkerStatusCode status) {
  // TODO(kinuko): Should log the failures.
  DCHECK_NE(SERVICE_WORKER_OK, status);
  std::move(callback).Run(status);
}

void BrowserSideServiceWorkerEventDispatcher::DidFailToDispatchFetch(
    int incoming_fetch_event_id,
    std::unique_ptr<ResponseCallback> response_callback,
    ServiceWorkerStatusCode status) {
  // TODO(kinuko): Should log the failures.
  DCHECK_NE(SERVICE_WORKER_OK, status);
  CompleteDispatchFetchEvent(incoming_fetch_event_id, status, base::Time());
}

void BrowserSideServiceWorkerEventDispatcher::DidDispatchFetchEvent(
    int incoming_fetch_event_id,
    int event_finish_id,
    ServiceWorkerStatusCode status,
    base::Time dispatch_event_time) {
  receiver_version_->FinishRequest(event_finish_id,
                                   status != SERVICE_WORKER_ERROR_ABORT,
                                   dispatch_event_time);
  CompleteDispatchFetchEvent(incoming_fetch_event_id, status,
                             dispatch_event_time);
}

void BrowserSideServiceWorkerEventDispatcher::CompleteDispatchFetchEvent(
    int incoming_fetch_event_id,
    ServiceWorkerStatusCode status,
    base::Time dispatch_event_time) {
  DVLOG(1) << "CompleteFetch [" << incoming_fetch_event_id
           << "] status:" << status;
  auto found = fetch_event_callbacks_.find(incoming_fetch_event_id);
  DCHECK(found != fetch_event_callbacks_.end());
  std::move(found->second).Run(status, dispatch_event_time);
  fetch_event_callbacks_.erase(found);
}

}  // namespace content
