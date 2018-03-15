// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_dispatcher.h"

#include <stddef.h>
#include <utility>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/content_constants.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/service_worker/web_service_worker_impl.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_error_type.mojom.h"
#include "third_party/WebKit/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
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

void ServiceWorkerDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;

  // When you add a new message handler, you should consider adding a similar
  // handler in ServiceWorkerMessageFilter to release references passed from
  // the browser process in case we fail to post task to the thread.
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerDispatcher, msg)
    IPC_MESSAGE_HANDLER(ServiceWorkerMsg_ServiceWorkerStateChanged,
                        OnServiceWorkerStateChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled) << "Unhandled message:" << msg.type();
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

  WorkerObjectMap::iterator found = service_workers_.find(info->handle_id);
  if (found != service_workers_.end())
    return found->second;

  if (WorkerThread::GetCurrentId()) {
    // Because we do not support navigator.serviceWorker in
    // WorkerNavigator (see https://crbug.com/371690), both dedicated worker and
    // shared worker context can never have a ServiceWorker object, but service
    // worker execution context is different as it can have some ServiceWorker
    // objects via the ServiceWorkerGlobalScope#registration object.
    // So, if we're on a worker thread here we know it's definitely a service
    // worker thread.
    DCHECK(io_thread_task_runner_);
    return WebServiceWorkerImpl::CreateForServiceWorkerGlobalScope(
        std::move(info), io_thread_task_runner_);
  }
  return WebServiceWorkerImpl::CreateForServiceWorkerClient(std::move(info));
}

void ServiceWorkerDispatcher::SetIOThreadTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner) {
  io_thread_task_runner_ = std::move(io_thread_task_runner);
}

void ServiceWorkerDispatcher::OnServiceWorkerStateChanged(
    int thread_id,
    int handle_id,
    blink::mojom::ServiceWorkerState state) {
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerDispatcher::OnServiceWorkerStateChanged",
               "Thread ID", thread_id,
               "State", static_cast<int>(state));
  WorkerObjectMap::iterator worker = service_workers_.find(handle_id);
  if (worker != service_workers_.end())
    worker->second->OnStateChanged(state);
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
