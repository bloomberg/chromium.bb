// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_dispatcher.h"

#include <stddef.h>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/content_constants.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/web_service_worker_impl.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/modules/serviceworker/web_service_worker_provider_client.h"
#include "third_party/blink/public/platform/web_string.h"
#include "url/url_constants.h"

using blink::WebServiceWorkerError;
using blink::WebServiceWorkerProvider;
using base::ThreadLocalPointer;

namespace content {

namespace {

base::LazyInstance<ThreadLocalPointer<void>>::Leaky g_dispatcher_tls =
    LAZY_INSTANCE_INITIALIZER;

void* const kDeletedServiceWorkerDispatcherMarker =
    reinterpret_cast<void*>(0x1);

}  // namespace

ServiceWorkerDispatcher::ServiceWorkerDispatcher() {
  g_dispatcher_tls.Pointer()->Set(static_cast<void*>(this));
}

ServiceWorkerDispatcher::~ServiceWorkerDispatcher() {
  if (allow_reinstantiation_) {
    g_dispatcher_tls.Pointer()->Set(nullptr);
    return;
  }
  g_dispatcher_tls.Pointer()->Set(kDeletedServiceWorkerDispatcherMarker);
}

ServiceWorkerDispatcher*
ServiceWorkerDispatcher::GetOrCreateThreadSpecificInstance() {
  if (g_dispatcher_tls.Pointer()->Get() ==
      kDeletedServiceWorkerDispatcherMarker) {
    NOTREACHED() << "Re-instantiating TLS ServiceWorkerDispatcher.";
    g_dispatcher_tls.Pointer()->Set(nullptr);
  }
  if (g_dispatcher_tls.Pointer()->Get())
    return static_cast<ServiceWorkerDispatcher*>(
        g_dispatcher_tls.Pointer()->Get());

  ServiceWorkerDispatcher* dispatcher = new ServiceWorkerDispatcher();
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(dispatcher);
  return dispatcher;
}

ServiceWorkerDispatcher* ServiceWorkerDispatcher::GetThreadSpecificInstance() {
  if (g_dispatcher_tls.Pointer()->Get() ==
      kDeletedServiceWorkerDispatcherMarker)
    return nullptr;
  return static_cast<ServiceWorkerDispatcher*>(
      g_dispatcher_tls.Pointer()->Get());
}

void ServiceWorkerDispatcher::AllowReinstantiationForTesting() {
  allow_reinstantiation_ = true;
}

void ServiceWorkerDispatcher::WillStopCurrentWorkerThread() {
  delete this;
}

scoped_refptr<WebServiceWorkerImpl>
ServiceWorkerDispatcher::GetOrCreateServiceWorker(
    blink::mojom::ServiceWorkerObjectInfoPtr info) {
  if (!info)
    return nullptr;
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerHandleId, info->handle_id);
  DCHECK_NE(blink::mojom::kInvalidServiceWorkerVersionId, info->version_id);

  DCHECK(info->request.is_pending());
  WorkerObjectMap::iterator found = service_workers_.find(info->handle_id);
  if (found != service_workers_.end()) {
    return found->second;
  }

  return WebServiceWorkerImpl::CreateForServiceWorkerGlobalScope(
      std::move(info));
}

void ServiceWorkerDispatcher::AddServiceWorker(
    int handle_id, WebServiceWorkerImpl* worker) {
  DCHECK(!base::ContainsKey(service_workers_, handle_id));
  service_workers_[handle_id] = worker;
}

void ServiceWorkerDispatcher::RemoveServiceWorker(int handle_id) {
  DCHECK(base::ContainsKey(service_workers_, handle_id));
  service_workers_.erase(handle_id);
}

}  // namespace content
