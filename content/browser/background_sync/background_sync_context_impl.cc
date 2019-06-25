// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "content/browser/background_sync/background_sync_launcher.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/browser/background_sync/one_shot_background_sync_service_impl.h"
#include "content/browser/background_sync/periodic_background_sync_service_impl.h"
#include "content/browser/devtools/devtools_background_services_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

namespace content {

BackgroundSyncContextImpl::BackgroundSyncContextImpl()
    : base::RefCountedDeleteOnSequence<BackgroundSyncContextImpl>(
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})) {}

BackgroundSyncContextImpl::~BackgroundSyncContextImpl() {
  // The destructor must run on the IO thread because it implicitly accesses
  // background_sync_manager_ and services_, when it runs their destructors.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(!background_sync_manager_);
  DCHECK(one_shot_sync_services_.empty());
  DCHECK(periodic_sync_services_.empty());
}

// static
#if defined(OS_ANDROID)
void BackgroundSyncContext::FireBackgroundSyncEventsAcrossPartitions(
    BrowserContext* browser_context,
    blink::mojom::BackgroundSyncType sync_type,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);
  BackgroundSyncLauncher::FireBackgroundSyncEvents(browser_context, sync_type,
                                                   j_runnable);
}
#endif

// static
void BackgroundSyncContext::GetSoonestWakeupDeltaAcrossPartitions(
    BrowserContext* browser_context,
    base::OnceCallback<void(base::TimeDelta)> callback) {
  DCHECK(browser_context);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncLauncher::GetSoonestWakeupDelta(browser_context,
                                                std::move(callback));
}

void BackgroundSyncContextImpl::Init(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const scoped_refptr<DevToolsBackgroundServicesContextImpl>&
        devtools_context) {
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

void BackgroundSyncContextImpl::CreateOneShotSyncService(
    blink::mojom::OneShotBackgroundSyncServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &BackgroundSyncContextImpl::CreateOneShotSyncServiceOnIOThread, this,
          std::move(request)));
}

void BackgroundSyncContextImpl::CreatePeriodicSyncService(
    blink::mojom::PeriodicBackgroundSyncServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &BackgroundSyncContextImpl::CreatePeriodicSyncServiceOnIOThread, this,
          std::move(request)));
}

void BackgroundSyncContextImpl::OneShotSyncServiceHadConnectionError(
    OneShotBackgroundSyncServiceImpl* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(service);

  auto iter = one_shot_sync_services_.find(service);
  DCHECK(iter != one_shot_sync_services_.end());
  one_shot_sync_services_.erase(iter);
}

void BackgroundSyncContextImpl::PeriodicSyncServiceHadConnectionError(
    PeriodicBackgroundSyncServiceImpl* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(service);

  auto iter = periodic_sync_services_.find(service);
  DCHECK(iter != periodic_sync_services_.end());
  periodic_sync_services_.erase(iter);
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

void BackgroundSyncContextImpl::GetSoonestWakeupDelta(
    base::OnceCallback<void(base::TimeDelta)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &BackgroundSyncContextImpl::GetSoonestWakeupDeltaOnIOThread, this),
      base::BindOnce(&BackgroundSyncContextImpl::DidGetSoonestWakeupDelta, this,
                     std::move(callback)));
}

base::TimeDelta BackgroundSyncContextImpl::GetSoonestWakeupDeltaOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!test_wakeup_delta_.is_max())
    return test_wakeup_delta_;
  if (!background_sync_manager_)
    return base::TimeDelta::Max();

  // TODO(crbug.com/925297): Add a wakeup task for PERIODIC_SYNC registrations.
  return background_sync_manager_->GetSoonestWakeupDelta(
      blink::mojom::BackgroundSyncType::ONE_SHOT);
}

void BackgroundSyncContextImpl::DidGetSoonestWakeupDelta(
    base::OnceCallback<void(base::TimeDelta)> callback,
    base::TimeDelta soonest_wakeup_delta) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::move(callback).Run(soonest_wakeup_delta);
}

void BackgroundSyncContextImpl::FireBackgroundSyncEvents(
    blink::mojom::BackgroundSyncType sync_type,
    base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &BackgroundSyncContextImpl::FireBackgroundSyncEventsOnIOThread, this,
          sync_type, std::move(done_closure)));
}

void BackgroundSyncContextImpl::FireBackgroundSyncEventsOnIOThread(
    blink::mojom::BackgroundSyncType sync_type,
    base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!background_sync_manager_) {
    std::move(done_closure).Run();
    return;
  }

  background_sync_manager_->FireReadyEvents(
      sync_type,
      base::BindOnce(
          &BackgroundSyncContextImpl::DidFireBackgroundSyncEventsOnIOThread,
          this, std::move(done_closure)));
}

void BackgroundSyncContextImpl::DidFireBackgroundSyncEventsOnIOThread(
    base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           std::move(done_closure));
}

void BackgroundSyncContextImpl::CreateBackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<DevToolsBackgroundServicesContextImpl> devtools_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!background_sync_manager_);

  background_sync_manager_ = BackgroundSyncManager::Create(
      std::move(service_worker_context), std::move(devtools_context));
}

void BackgroundSyncContextImpl::CreateOneShotSyncServiceOnIOThread(
    mojo::InterfaceRequest<blink::mojom::OneShotBackgroundSyncService>
        request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(background_sync_manager_);
  one_shot_sync_services_.insert(
      std::make_unique<OneShotBackgroundSyncServiceImpl>(this,
                                                         std::move(request)));
}

void BackgroundSyncContextImpl::CreatePeriodicSyncServiceOnIOThread(
    mojo::InterfaceRequest<blink::mojom::PeriodicBackgroundSyncService>
        request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(background_sync_manager_);
  periodic_sync_services_.insert(
      std::make_unique<PeriodicBackgroundSyncServiceImpl>(this,
                                                          std::move(request)));
}

void BackgroundSyncContextImpl::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  one_shot_sync_services_.clear();
  periodic_sync_services_.clear();
  background_sync_manager_.reset();
}

}  // namespace content
