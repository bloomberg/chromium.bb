// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_IMPL_H_

#include <utility>
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {

class ServiceWorkerContextClient;

// S13nServiceWorker:
// An instance of this class is created on the service worker thread
// when ServiceWorkerContextClient's WorkerContextData is created.
// This implements blink::mojom::ControllerServiceWorker and its Mojo endpoint
// is connected by each controllee and also by the ServiceWorkerProviderHost
// in the browser process.
// Subresource requests made by the controllees are sent to this class as
// Fetch events via the Mojo endpoints.
//
// TODO(kinuko): Implement self-killing timer, that does something similar to
// what ServiceWorkerVersion::StopWorkerIfIdle does in the browser process in
// non-S13n code.
class ControllerServiceWorkerImpl
    : public blink::mojom::ControllerServiceWorker {
 public:
  // |context_client|'s weak pointer is the one that is bound to the worker
  // thread. (It should actually outlive this instance, but allow us to make
  // sure the thread safety)
  ControllerServiceWorkerImpl(
      blink::mojom::ControllerServiceWorkerRequest request,
      base::WeakPtr<ServiceWorkerContextClient> context_client,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~ControllerServiceWorkerImpl() override;

  // blink::mojom::ControllerServiceWorker:
  void DispatchFetchEvent(
      blink::mojom::DispatchFetchEventParamsPtr params,
      blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
      DispatchFetchEventCallback callback) override;
  void Clone(blink::mojom::ControllerServiceWorkerRequest request) override;

 private:
  // Connected by the ServiceWorkerProviderHost in the browser process
  // and by the controllees.
  mojo::BindingSet<blink::mojom::ControllerServiceWorker> bindings_;

  base::WeakPtr<ServiceWorkerContextClient> context_client_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ControllerServiceWorkerImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_IMPL_H_
