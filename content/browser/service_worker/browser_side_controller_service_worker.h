// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_BROWSER_SIDE_CONTROLLER_SERVICE_WORKER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_BROWSER_SIDE_CONTROLLER_SERVICE_WORKER_H_

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/common/service_worker/controller_service_worker.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_event_status.mojom.h"

namespace content {

class ServiceWorkerVersion;

// S13nServiceWorker:
// BrowserSideControllerServiceWorker is the browser-side implementation of
// mojom::ControllerServiceWorker, that forwards Fetch events to a
// ServiceWorkerVersion, and is called by controllees from their renderer
// processes.
// This class should be soon migrated to the renderer-process where the
// controller service worker lives.
class BrowserSideControllerServiceWorker
    : public mojom::ControllerServiceWorker {
 public:
  explicit BrowserSideControllerServiceWorker(
      ServiceWorkerVersion* receiver_version);
  ~BrowserSideControllerServiceWorker() override;

  void AddBinding(mojom::ControllerServiceWorkerRequest request);

  // mojom::ControllerServiceWorker:
  void DispatchFetchEvent(
      const ServiceWorkerFetchRequest& request,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override;

 private:
  class ResponseCallback;
  using StatusCallback = base::OnceCallback<void(ServiceWorkerStatusCode)>;

  void DispatchFetchEventInternal(
      int internal_fetch_event_id,
      const ServiceWorkerFetchRequest& request,
      mojom::ServiceWorkerFetchResponseCallbackPtr response_callback);
  void DidFailToStartWorker(StatusCallback callback,
                            ServiceWorkerStatusCode status);
  void DidFailToDispatchFetch(int internal_fetch_event_id,
                              std::unique_ptr<ResponseCallback> callback,
                              ServiceWorkerStatusCode status);
  void DidDispatchFetchEvent(int internal_fetch_event_id,
                             int event_finish_id,
                             blink::mojom::ServiceWorkerEventStatus status,
                             base::Time dispatch_event_time);
  void CompleteDispatchFetchEvent(int internal_fetch_event_id,
                                  blink::mojom::ServiceWorkerEventStatus status,
                                  base::Time dispatch_event_time);

  // Connected by the controllees.
  mojo::BindingSet<mojom::ControllerServiceWorker> binding_;

  // Not owned; |this| should go away before the version goes away.
  ServiceWorkerVersion* receiver_version_;

  base::IDMap<std::unique_ptr<DispatchFetchEventCallback>>
      fetch_event_callbacks_;

  base::WeakPtrFactory<BrowserSideControllerServiceWorker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserSideControllerServiceWorker);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_BROWSER_SIDE_CONTROLLER_SERVICE_WORKER_H_
