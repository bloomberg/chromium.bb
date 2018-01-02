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
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/common/quota/storage_type.h"
#include "third_party/WebKit/public/platform/WebStorageQuotaCallbacks.h"
#include "url/origin.h"

using blink::QuotaStatusCode;
using blink::StorageType;
using blink::WebStorageQuotaCallbacks;

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
  base::IDMap<std::unique_ptr<WebStorageQuotaCallbacks>>::iterator iter(
      &pending_quota_callbacks_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->DidFail(blink::QuotaStatusCode::kErrorAbort);
    iter.Advance();
  }

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
    std::unique_ptr<WebStorageQuotaCallbacks> callback) {
  DCHECK(callback);
  int request_id = pending_quota_callbacks_.Add(std::move(callback));
  quota_host_->QueryStorageUsageAndQuota(
      origin, type,
      base::BindOnce(&QuotaDispatcher::DidQueryStorageUsageAndQuota,
                     base::Unretained(this), request_id));
}

void QuotaDispatcher::RequestStorageQuota(
    int render_frame_id,
    const url::Origin& origin,
    StorageType type,
    int64_t requested_size,
    std::unique_ptr<WebStorageQuotaCallbacks> callback) {
  DCHECK(callback);
  DCHECK_EQ(CurrentWorkerId(), 0)
      << "Requests may show permission UI and are not allowed from workers.";
  int request_id = pending_quota_callbacks_.Add(std::move(callback));
  quota_host_->RequestStorageQuota(
      render_frame_id, origin, type, requested_size,
      base::BindOnce(&QuotaDispatcher::DidGrantStorageQuota,
                     base::Unretained(this), request_id));
}

void QuotaDispatcher::DidGrantStorageQuota(int64_t request_id,
                                           QuotaStatusCode status,
                                           int64_t current_usage,
                                           int64_t granted_quota) {
  if (status != blink::QuotaStatusCode::kOk) {
    DidFail(request_id, status);
    return;
  }

  WebStorageQuotaCallbacks* callback =
      pending_quota_callbacks_.Lookup(request_id);
  DCHECK(callback);
  callback->DidGrantStorageQuota(current_usage, granted_quota);
  pending_quota_callbacks_.Remove(request_id);
}

void QuotaDispatcher::DidQueryStorageUsageAndQuota(int64_t request_id,
                                                   QuotaStatusCode status,
                                                   int64_t current_usage,
                                                   int64_t current_quota) {
  if (status != blink::QuotaStatusCode::kOk) {
    DidFail(request_id, status);
    return;
  }

  WebStorageQuotaCallbacks* callback =
      pending_quota_callbacks_.Lookup(request_id);
  DCHECK(callback);
  callback->DidQueryStorageUsageAndQuota(current_usage, current_quota);
  pending_quota_callbacks_.Remove(request_id);
}

void QuotaDispatcher::DidFail(
    int request_id,
    QuotaStatusCode error) {
  WebStorageQuotaCallbacks* callback =
      pending_quota_callbacks_.Lookup(request_id);
  DCHECK(callback);
  callback->DidFail(error);
  pending_quota_callbacks_.Remove(request_id);
}

}  // namespace content
