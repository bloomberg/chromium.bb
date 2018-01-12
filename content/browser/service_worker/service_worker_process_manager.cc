// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_process_manager.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "url/gurl.h"

namespace content {

ServiceWorkerProcessManager::ServiceWorkerProcessManager(
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      storage_partition_(nullptr),
      process_id_for_test_(ChildProcessHost::kInvalidUniqueID),
      new_process_id_for_test_(ChildProcessHost::kInvalidUniqueID),
      weak_this_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

ServiceWorkerProcessManager::~ServiceWorkerProcessManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(IsShutdown())
      << "Call Shutdown() before destroying |this|, so that racing method "
      << "invocations don't use a destroyed BrowserContext.";
  // TODO(horo): Remove after collecting crash data.
  // Temporary checks to verify that ServiceWorkerProcessManager doesn't prevent
  // render process hosts from shutting down: crbug.com/639193
  CHECK(worker_process_map_.empty());
}

BrowserContext* ServiceWorkerProcessManager::browser_context() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // This is safe because reading |browser_context_| on the UI thread doesn't
  // need locking (while modifying does).
  return browser_context_;
}

void ServiceWorkerProcessManager::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  {
    base::AutoLock lock(browser_context_lock_);
    browser_context_ = nullptr;
  }

  // In single-process mode, Shutdown() is called when deleting the default
  // browser context, which is itself destroyed after the RenderProcessHost.
  // The refcount decrement can be skipped anyway since there's only one
  // process.
  if (!RenderProcessHost::run_renderer_in_process()) {
    for (const auto& it : worker_process_map_)
      it.second->GetProcess()->DecrementKeepAliveRefCount();
  }
  worker_process_map_.clear();
}

bool ServiceWorkerProcessManager::IsShutdown() {
  base::AutoLock lock(browser_context_lock_);
  return !browser_context_;
}

ServiceWorkerStatusCode ServiceWorkerProcessManager::AllocateWorkerProcess(
    int embedded_worker_id,
    const GURL& pattern,
    const GURL& script_url,
    bool can_use_existing_process,
    AllocatedProcessInfo* out_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  out_info->process_id = ChildProcessHost::kInvalidUniqueID;
  out_info->start_situation = ServiceWorkerMetrics::StartSituation::UNKNOWN;

  if (process_id_for_test_ != ChildProcessHost::kInvalidUniqueID) {
    // Let tests specify the returned process ID.
    int result = can_use_existing_process ? process_id_for_test_
                                          : new_process_id_for_test_;
    out_info->process_id = result;
    out_info->start_situation =
        ServiceWorkerMetrics::StartSituation::EXISTING_READY_PROCESS;
    return SERVICE_WORKER_OK;
  }

  if (IsShutdown()) {
    return SERVICE_WORKER_ERROR_ABORT;
  }

  DCHECK(!base::ContainsKey(worker_process_map_, embedded_worker_id))
      << embedded_worker_id << " already has a process allocated";

  // Create a SiteInstance to get the renderer process from. Use the site URL
  // from the StoragePartition in case this StoragePartition is for guests
  // (e.g., <webview>).
  bool use_url_from_storage_partition =
      storage_partition_ &&
      !storage_partition_->site_for_service_worker().is_empty();
  scoped_refptr<SiteInstanceImpl> site_instance =
      SiteInstanceImpl::CreateForURL(
          browser_context_, use_url_from_storage_partition
                                ? storage_partition_->site_for_service_worker()
                                : script_url);
  site_instance->set_is_for_service_worker();

  // Attempt to reuse a renderer process if possible. Note that in the
  // <webview> case, process reuse isn't currently supported and a new
  // process will always be created (https://crbug.com/752667).
  DCHECK(site_instance->process_reuse_policy() ==
             SiteInstanceImpl::ProcessReusePolicy::DEFAULT ||
         site_instance->process_reuse_policy() ==
             SiteInstanceImpl::ProcessReusePolicy::PROCESS_PER_SITE);
  if (can_use_existing_process) {
    site_instance->set_process_reuse_policy(
        SiteInstanceImpl::ProcessReusePolicy::REUSE_PENDING_OR_COMMITTED_SITE);
  }

  // Get the process from the SiteInstance.
  RenderProcessHost* rph = site_instance->GetProcess();
  DCHECK(!storage_partition_ ||
         rph->InSameStoragePartition(storage_partition_));

  ServiceWorkerMetrics::StartSituation start_situation;
  if (!rph->HasConnection()) {
    // HasConnection() is false means that Init() has not been called or the
    // process has been killed.
    start_situation = ServiceWorkerMetrics::StartSituation::NEW_PROCESS;
  } else if (!rph->IsReady()) {
    start_situation =
        ServiceWorkerMetrics::StartSituation::EXISTING_UNREADY_PROCESS;
  } else {
    start_situation =
        ServiceWorkerMetrics::StartSituation::EXISTING_READY_PROCESS;
  }

  if (!rph->Init()) {
    LOG(ERROR) << "Couldn't start a new process!";
    return SERVICE_WORKER_ERROR_PROCESS_NOT_FOUND;
  }

  worker_process_map_.emplace(embedded_worker_id, std::move(site_instance));
  rph->IncrementKeepAliveRefCount();
  out_info->process_id = rph->GetID();
  out_info->start_situation = start_situation;
  return SERVICE_WORKER_OK;
}

void ServiceWorkerProcessManager::ReleaseWorkerProcess(int embedded_worker_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (process_id_for_test_ != ChildProcessHost::kInvalidUniqueID) {
    // Unittests don't increment or decrement the worker refcount of a
    // RenderProcessHost.
    return;
  }

  if (IsShutdown()) {
    // Shutdown already released all instances.
    DCHECK(worker_process_map_.empty());
    return;
  }

  auto it = worker_process_map_.find(embedded_worker_id);
  // ReleaseWorkerProcess could be called for a nonexistent worker id, for
  // example, when request to start a worker is aborted on the IO thread during
  // process allocation that is failed on the UI thread.
  if (it == worker_process_map_.end())
    return;

  it->second->GetProcess()->DecrementKeepAliveRefCount();
  worker_process_map_.erase(it);
}

}  // namespace content

namespace std {
// Destroying ServiceWorkerProcessManagers only on the UI thread allows the
// member WeakPtr to safely guard the object's lifetime when used on that
// thread.
void default_delete<content::ServiceWorkerProcessManager>::operator()(
    content::ServiceWorkerProcessManager* ptr) const {
  content::BrowserThread::DeleteSoon(
      content::BrowserThread::UI, FROM_HERE, ptr);
}
}  // namespace std
