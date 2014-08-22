// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include <map>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/browser/blob/blob_storage_context.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace content {

ServiceWorkerContextWrapper::ServiceWorkerContextWrapper(
    BrowserContext* browser_context)
    : observer_list_(
          new ObserverListThreadSafe<ServiceWorkerContextObserver>()),
      process_manager_(new ServiceWorkerProcessManager(browser_context)),
      is_incognito_(false) {
}

ServiceWorkerContextWrapper::~ServiceWorkerContextWrapper() {
}

void ServiceWorkerContextWrapper::Init(
    const base::FilePath& user_data_directory,
    storage::QuotaManagerProxy* quota_manager_proxy) {
  is_incognito_ = user_data_directory.empty();
  scoped_refptr<base::SequencedTaskRunner> database_task_runner =
      BrowserThread::GetBlockingPool()->
          GetSequencedTaskRunnerWithShutdownBehavior(
              BrowserThread::GetBlockingPool()->GetSequenceToken(),
              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  scoped_refptr<base::SingleThreadTaskRunner> disk_cache_thread =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE);
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner =
      BrowserThread::GetBlockingPool()
          ->GetSequencedTaskRunnerWithShutdownBehavior(
              BrowserThread::GetBlockingPool()->GetSequenceToken(),
              base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  InitInternal(user_data_directory,
               cache_task_runner,
               database_task_runner,
               disk_cache_thread,
               quota_manager_proxy);
}

void ServiceWorkerContextWrapper::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  process_manager_->Shutdown();
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ServiceWorkerContextWrapper::ShutdownOnIO, this));
}

void ServiceWorkerContextWrapper::DeleteAndStartOver() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_core_->DeleteAndStartOver(
      base::Bind(&ServiceWorkerContextWrapper::DidDeleteAndStartOver, this));
}

ServiceWorkerContextCore* ServiceWorkerContextWrapper::context() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return context_core_.get();
}

static void FinishRegistrationOnIO(
    const ServiceWorkerContext::ResultCallback& continuation,
    ServiceWorkerStatusCode status,
    int64 registration_id,
    int64 version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(continuation, status == SERVICE_WORKER_OK));
}

void ServiceWorkerContextWrapper::RegisterServiceWorker(
    const GURL& pattern,
    const GURL& script_url,
    const ResultCallback& continuation) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::RegisterServiceWorker,
                   this,
                   pattern,
                   script_url,
                   continuation));
    return;
  }

  context()->RegisterServiceWorker(
      pattern,
      script_url,
      -1,
      NULL /* provider_host */,
      base::Bind(&FinishRegistrationOnIO, continuation));
}

static void FinishUnregistrationOnIO(
    const ServiceWorkerContext::ResultCallback& continuation,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(continuation, status == SERVICE_WORKER_OK));
}

void ServiceWorkerContextWrapper::UnregisterServiceWorker(
    const GURL& pattern,
    const ResultCallback& continuation) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::UnregisterServiceWorker,
                   this,
                   pattern,
                   continuation));
    return;
  }

  context()->UnregisterServiceWorker(
      pattern,
      base::Bind(&FinishUnregistrationOnIO, continuation));
}

void ServiceWorkerContextWrapper::Terminate() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  process_manager_->Shutdown();
}

void ServiceWorkerContextWrapper::GetAllOriginsInfo(
    const GetUsageInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_core_->storage()->GetAllRegistrations(base::Bind(
      &ServiceWorkerContextWrapper::DidGetAllRegistrationsForGetAllOrigins,
      this,
      callback));
}

void ServiceWorkerContextWrapper::DidGetAllRegistrationsForGetAllOrigins(
    const GetUsageInfoCallback& callback,
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::vector<ServiceWorkerUsageInfo> usage_infos;

  std::map<GURL, ServiceWorkerUsageInfo> origins;
  for (std::vector<ServiceWorkerRegistrationInfo>::const_iterator it =
           registrations.begin();
       it != registrations.end();
       ++it) {
    const ServiceWorkerRegistrationInfo& registration_info = *it;
    GURL origin = registration_info.script_url.GetOrigin();

    ServiceWorkerUsageInfo& usage_info = origins[origin];
    if (usage_info.origin.is_empty())
      usage_info.origin = origin;
    usage_info.scopes.push_back(registration_info.pattern);
  }

  for (std::map<GURL, ServiceWorkerUsageInfo>::const_iterator it =
           origins.begin();
       it != origins.end();
       ++it) {
    usage_infos.push_back(it->second);
  }

  callback.Run(usage_infos);
}

namespace {

void EmptySuccessCallback(bool success) {
}

}  // namespace

void ServiceWorkerContextWrapper::DeleteForOrigin(const GURL& origin_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_core_->storage()->GetAllRegistrations(base::Bind(
      &ServiceWorkerContextWrapper::DidGetAllRegistrationsForDeleteForOrigin,
      this,
      origin_url));
}

void ServiceWorkerContextWrapper::DidGetAllRegistrationsForDeleteForOrigin(
    const GURL& origin,
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (std::vector<ServiceWorkerRegistrationInfo>::const_iterator it =
           registrations.begin();
       it != registrations.end();
       ++it) {
    const ServiceWorkerRegistrationInfo& registration_info = *it;
    if (origin == registration_info.script_url.GetOrigin()) {
      UnregisterServiceWorker(registration_info.pattern,
                              base::Bind(&EmptySuccessCallback));
    }
  }
}

void ServiceWorkerContextWrapper::AddObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->RemoveObserver(observer);
}

void ServiceWorkerContextWrapper::SetBlobParametersForCache(
    net::URLRequestContextGetter* request_context,
    ChromeBlobStorageContext* blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (context_core_ && request_context && blob_storage_context) {
    context_core_->SetBlobParametersForCache(
        request_context->GetURLRequestContext(),
        blob_storage_context->context()->AsWeakPtr());
  }
}

void ServiceWorkerContextWrapper::InitInternal(
    const base::FilePath& user_data_directory,
    const scoped_refptr<base::SequencedTaskRunner>& stores_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& database_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread,
    storage::QuotaManagerProxy* quota_manager_proxy) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::InitInternal,
                   this,
                   user_data_directory,
                   stores_task_runner,
                   database_task_runner,
                   disk_cache_thread,
                   make_scoped_refptr(quota_manager_proxy)));
    return;
  }
  DCHECK(!context_core_);
  context_core_.reset(new ServiceWorkerContextCore(user_data_directory,
                                                   stores_task_runner,
                                                   database_task_runner,
                                                   disk_cache_thread,
                                                   quota_manager_proxy,
                                                   observer_list_,
                                                   this));
}

void ServiceWorkerContextWrapper::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context_core_.reset();
}

void ServiceWorkerContextWrapper::DidDeleteAndStartOver(
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    context_core_.reset();
    return;
  }
  context_core_.reset(new ServiceWorkerContextCore(context_core_.get(), this));
  DVLOG(1) << "Restarted ServiceWorkerContextCore successfully.";
}

}  // namespace content
