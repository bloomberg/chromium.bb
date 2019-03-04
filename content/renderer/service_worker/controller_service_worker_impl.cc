// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/controller_service_worker_impl.h"

#include "base/sequenced_task_runner.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"

namespace content {

ControllerServiceWorkerImpl::ControllerServiceWorkerImpl(
    blink::mojom::ControllerServiceWorkerRequest request,
    base::WeakPtr<ServiceWorkerContextClient> context_client,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : context_client_(std::move(context_client)),
      task_runner_(std::move(task_runner)) {
  CHECK(blink::ServiceWorkerUtils::IsServicificationEnabled());
  bindings_.AddBinding(this, std::move(request), task_runner_);
}

ControllerServiceWorkerImpl::~ControllerServiceWorkerImpl() = default;

void ControllerServiceWorkerImpl::Clone(
    blink::mojom::ControllerServiceWorkerRequest request) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  bindings_.AddBinding(this, std::move(request), task_runner_);
}

void ControllerServiceWorkerImpl::DispatchFetchEvent(
    blink::mojom::DispatchFetchEventParamsPtr params,
    blink::mojom::ServiceWorkerFetchResponseCallbackPtr response_callback,
    DispatchFetchEventCallback callback) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(context_client_);
  context_client_->DispatchOrQueueFetchEvent(
      std::move(params), std::move(response_callback), std::move(callback));
}

}  // namespace content
