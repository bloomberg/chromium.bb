// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_process_manager.h"

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "url/gurl.h"

namespace content {

ServiceWorkerProcessManager::ServiceWorkerProcessManager(
    ServiceWorkerContextWrapper* context_wrapper)
    : context_wrapper_(context_wrapper),
      weak_this_factory_(this),
      weak_this_(weak_this_factory_.GetWeakPtr()) {
}

ServiceWorkerProcessManager::~ServiceWorkerProcessManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerProcessManager::AllocateWorkerProcess(
    const std::vector<int>& process_ids,
    const GURL& script_url,
    const base::Callback<void(ServiceWorkerStatusCode, int process_id)>&
        callback) const {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ServiceWorkerProcessManager::AllocateWorkerProcess,
                   weak_this_,
                   process_ids,
                   script_url,
                   callback));
    return;
  }

  for (std::vector<int>::const_iterator it = process_ids.begin();
       it != process_ids.end();
       ++it) {
    if (IncrementWorkerRefcountByPid(*it)) {
      BrowserThread::PostTask(BrowserThread::IO,
                              FROM_HERE,
                              base::Bind(callback, SERVICE_WORKER_OK, *it));
      return;
    }
  }

  if (!context_wrapper_->browser_context_) {
    // Shutdown has started.
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED, -1));
    return;
  }
  // No existing processes available; start a new one.
  scoped_refptr<SiteInstance> site_instance = SiteInstance::CreateForURL(
      context_wrapper_->browser_context_, script_url);
  RenderProcessHost* rph = site_instance->GetProcess();
  // This Init() call posts a task to the IO thread that adds the RPH's
  // ServiceWorkerDispatcherHost to the
  // EmbeddedWorkerRegistry::process_sender_map_.
  if (!rph->Init()) {
    LOG(ERROR) << "Couldn't start a new process!";
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED, -1));
    return;
  }

  static_cast<RenderProcessHostImpl*>(rph)->IncrementWorkerRefCount();
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, SERVICE_WORKER_OK, rph->GetID()));
}

void ServiceWorkerProcessManager::ReleaseWorkerProcess(int process_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ServiceWorkerProcessManager::ReleaseWorkerProcess,
                   weak_this_,
                   process_id));
    return;
  }
  if (!DecrementWorkerRefcountByPid(process_id)) {
    DCHECK(false) << "DecrementWorkerRef(" << process_id
                  << ") doesn't match a previous IncrementWorkerRef";
  }
}

void ServiceWorkerProcessManager::SetProcessRefcountOpsForTest(
    const base::Callback<bool(int)>& increment_for_test,
    const base::Callback<bool(int)>& decrement_for_test) {
  increment_for_test_ = increment_for_test;
  decrement_for_test_ = decrement_for_test;
}

bool ServiceWorkerProcessManager::IncrementWorkerRefcountByPid(
    int process_id) const {
  if (!increment_for_test_.is_null())
    return increment_for_test_.Run(process_id);

  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  if (rph && !rph->FastShutdownStarted()) {
    static_cast<RenderProcessHostImpl*>(rph)->IncrementWorkerRefCount();
    return true;
  }

  return false;
}

bool ServiceWorkerProcessManager::DecrementWorkerRefcountByPid(
    int process_id) const {
  if (!decrement_for_test_.is_null())
    return decrement_for_test_.Run(process_id);

  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  if (rph)
    static_cast<RenderProcessHostImpl*>(rph)->DecrementWorkerRefCount();

  return rph != NULL;
}

}  // namespace content

namespace base {
// Destroying ServiceWorkerProcessManagers only on the UI thread allows the
// member WeakPtr to safely guard the object's lifetime when used on that
// thread.
void DefaultDeleter<content::ServiceWorkerProcessManager>::operator()(
    content::ServiceWorkerProcessManager* ptr) const {
  content::BrowserThread::DeleteSoon(
      content::BrowserThread::UI, FROM_HERE, ptr);
}
}  // namespace base
