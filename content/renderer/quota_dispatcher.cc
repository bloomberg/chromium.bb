// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/quota_dispatcher.h"

#include <memory>
#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_local.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/common/quota/quota_types.mojom.h"
#include "url/origin.h"

using blink::mojom::QuotaStatusCode;
using blink::mojom::StorageType;

namespace content {

static base::LazyInstance<base::ThreadLocalPointer<QuotaDispatcher>>::Leaky
    g_quota_dispatcher_tls = LAZY_INSTANCE_INITIALIZER;

namespace {

int CurrentWorkerId() {
  return WorkerThread::GetCurrentId();
}

void BindConnectorOnMainThread(
    blink::mojom::QuotaDispatcherHostRequest request) {
  DCHECK(RenderThread::Get());
  RenderThread::Get()->GetConnector()->BindInterface(mojom::kBrowserServiceName,
                                                     std::move(request));
}

}  // namespace

QuotaDispatcher::QuotaDispatcher(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : main_thread_task_runner_(main_thread_task_runner) {
  g_quota_dispatcher_tls.Pointer()->Set(this);

  auto request = mojo::MakeRequest(&quota_host_);
  if (main_thread_task_runner_->BelongsToCurrentThread()) {
    BindConnectorOnMainThread(std::move(request));
  } else {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&BindConnectorOnMainThread, std::move(request)));
  }
}

QuotaDispatcher::~QuotaDispatcher() {
  g_quota_dispatcher_tls.Pointer()->Set(nullptr);
}

QuotaDispatcher* QuotaDispatcher::ThreadSpecificInstance(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (g_quota_dispatcher_tls.Pointer()->Get())
    return g_quota_dispatcher_tls.Pointer()->Get();

  QuotaDispatcher* dispatcher = new QuotaDispatcher(task_runner);
  if (WorkerThread::GetCurrentId())
    WorkerThread::AddObserver(dispatcher);
  return dispatcher;
}

void QuotaDispatcher::WillStopCurrentWorkerThread() {
  delete this;
}

void QuotaDispatcher::QueryStorageUsageAndQuota(
    const url::Origin& origin,
    StorageType type,
    blink::mojom::QuotaDispatcherHost::QueryStorageUsageAndQuotaCallback
        callback) {
  DCHECK(callback);
  // Use WrapCallbackWithDefaultInvokeIfNotRun() to ensure the callback is run
  // with a failure code if QuotaDispatcher is destroyed.
  quota_host_->QueryStorageUsageAndQuota(
      origin, type,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), blink::mojom::QuotaStatusCode::kErrorAbort, 0,
          0));
}

void QuotaDispatcher::RequestStorageQuota(
    int render_frame_id,
    const url::Origin& origin,
    StorageType type,
    int64_t requested_size,
    blink::mojom::QuotaDispatcherHost::RequestStorageQuotaCallback callback) {
  DCHECK(callback);
  DCHECK_EQ(CurrentWorkerId(), 0)
      << "Requests may show permission UI and are not allowed from workers.";
  // Use WrapCallbackWithDefaultInvokeIfNotRun() to ensure the callback is run
  // with a failure code if QuotaDispatcher is destroyed.
  quota_host_->RequestStorageQuota(
      render_frame_id, origin, type, requested_size,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback), blink::mojom::QuotaStatusCode::kErrorAbort, 0,
          0));
}

}  // namespace content
