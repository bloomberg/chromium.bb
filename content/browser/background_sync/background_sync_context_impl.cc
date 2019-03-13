// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/background_sync/background_sync_launcher.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/background_sync_service_impl.h"
#include "content/browser/devtools/devtools_background_services_context.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

BackgroundSyncContextImpl::BackgroundSyncContextImpl()
    : base::RefCountedDeleteOnSequence<BackgroundSyncContextImpl>(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})) {}

BackgroundSyncContextImpl::~BackgroundSyncContextImpl() {
  // The destructor must run on the IO thread because it implicitly accesses
  // background_sync_manager_ and services_, when it runs their destructors.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!background_sync_manager_);
  DCHECK(services_.empty());
}

void BackgroundSyncContextImpl::Init(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<DevToolsBackgroundServicesContext>& devtools_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BackgroundSyncContextImpl::CreateBackgroundSyncManager,
                     this, service_worker_context, devtools_context));
}

void BackgroundSyncContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BackgroundSyncContextImpl::ShutdownOnIO, this));
}

void BackgroundSyncContextImpl::CreateService(
    blink::mojom::BackgroundSyncServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BackgroundSyncContextImpl::CreateServiceOnIOThread, this,
                     std::move(request)));
}

void BackgroundSyncContextImpl::ServiceHadConnectionError(
    BackgroundSyncServiceImpl* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(base::ContainsKey(services_, service));

  services_.erase(service);
}

BackgroundSyncManager* BackgroundSyncContextImpl::background_sync_manager()
    const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return background_sync_manager_.get();
}

void BackgroundSyncContextImpl::set_background_sync_manager_for_testing(
    std::unique_ptr<BackgroundSyncManager> manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  background_sync_manager_ = std::move(manager);
}

void BackgroundSyncContextImpl::set_wakeup_delta_for_testing(
    base::TimeDelta wakeup_delta) {
  test_wakeup_delta_ = wakeup_delta;
}

base::TimeDelta BackgroundSyncContextImpl::GetSoonestWakeupDelta() {
  if (!test_wakeup_delta_.is_max())
    return test_wakeup_delta_;

  if (!background_sync_manager_)
    return base::TimeDelta::Max();

  return background_sync_manager_->GetSoonestWakeupDelta();
}

void BackgroundSyncContextImpl::GetSoonestWakeupDeltaAcrossPartitions(
    BrowserContext* browser_context,
    base::OnceCallback<void(base::TimeDelta)> callback) {
  DCHECK(browser_context);

  BackgroundSyncLauncher::GetSoonestWakeupDelta(browser_context,
                                                std::move(callback));
}

void BackgroundSyncContextImpl::FireBackgroundSyncEvents(
    base::OnceClosure done_closure) {
  if (!background_sync_manager_) {
    std::move(done_closure).Run();
    return;
  }
  background_sync_manager_->FireReadyEventsThenRunCallback(
      std::move(done_closure));
}

#if defined(OS_ANDROID)
void BackgroundSyncContextImpl::FireBackgroundSyncEventsAcrossPartitions(
    BrowserContext* browser_context,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  DCHECK(browser_context);
  BackgroundSyncLauncher::FireBackgroundSyncEvents(browser_context, j_runnable);
}
#endif

void BackgroundSyncContextImpl::CreateBackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<DevToolsBackgroundServicesContext> devtools_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!background_sync_manager_);

  background_sync_manager_ = BackgroundSyncManager::Create(
      std::move(service_worker_context), std::move(devtools_context));
}

void BackgroundSyncContextImpl::CreateServiceOnIOThread(
    mojo::InterfaceRequest<blink::mojom::BackgroundSyncService> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(background_sync_manager_);
  auto service =
      std::make_unique<BackgroundSyncServiceImpl>(this, std::move(request));
  services_[service.get()] = std::move(service);
}

void BackgroundSyncContextImpl::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  services_.clear();
  background_sync_manager_.reset();
}

}  // namespace content
