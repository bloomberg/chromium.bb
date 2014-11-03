// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/fileapi/chrome_blob_storage_context.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_quota_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace content {

namespace {

typedef std::set<std::string> HeaderNameSet;
base::LazyInstance<HeaderNameSet> g_excluded_header_name_set =
    LAZY_INSTANCE_INITIALIZER;
}

void ServiceWorkerContext::AddExcludedHeadersForFetchEvent(
    const std::set<std::string>& header_names) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  g_excluded_header_name_set.Get().insert(header_names.begin(),
                                          header_names.end());
}

bool ServiceWorkerContext::IsExcludedHeaderNameForFetchEvent(
    const std::string& header_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return g_excluded_header_name_set.Get().find(header_name) !=
         g_excluded_header_name_set.Get().end();
}

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
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy) {
  is_incognito_ = user_data_directory.empty();
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  scoped_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager(
      new ServiceWorkerDatabaseTaskManagerImpl(pool));
  scoped_refptr<base::SingleThreadTaskRunner> disk_cache_thread =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE);
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner =
      pool->GetSequencedTaskRunnerWithShutdownBehavior(
          BrowserThread::GetBlockingPool()->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
  InitInternal(user_data_directory,
               cache_task_runner,
               database_task_manager.Pass(),
               disk_cache_thread,
               quota_manager_proxy,
               special_storage_policy);
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
    int64 registration_id) {
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
  if (!context_core_.get()) {
    LOG(ERROR) << "ServiceWorkerContextCore is no longer alive.";
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(continuation, false));
    return;
  }
  context()->RegisterServiceWorker(
      pattern,
      script_url,
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
  if (!context_core_.get()) {
    LOG(ERROR) << "ServiceWorkerContextCore is no longer alive.";
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(continuation, false));
    return;
  }

  context()->UnregisterServiceWorker(
      pattern,
      base::Bind(&FinishUnregistrationOnIO, continuation));
}

void ServiceWorkerContextWrapper::GetAllOriginsInfo(
    const GetUsageInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_.get()) {
    LOG(ERROR) << "ServiceWorkerContextCore is no longer alive.";
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(callback, std::vector<ServiceWorkerUsageInfo>()));
    return;
  }
  context()->storage()->GetAllRegistrations(base::Bind(
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
  for (const auto& registration_info : registrations) {
    GURL origin = registration_info.pattern.GetOrigin();

    ServiceWorkerUsageInfo& usage_info = origins[origin];
    if (usage_info.origin.is_empty())
      usage_info.origin = origin;
    usage_info.scopes.push_back(registration_info.pattern);
    usage_info.total_size_bytes += registration_info.stored_version_size_bytes;
  }

  for (const auto& origin_info_pair : origins) {
    usage_infos.push_back(origin_info_pair.second);
  }
  callback.Run(usage_infos);
}

namespace {
void StatusCodeToBoolCallbackAdapter(
    const ServiceWorkerContext::ResultCallback& callback,
    ServiceWorkerStatusCode code) {
  callback.Run(code == ServiceWorkerStatusCode::SERVICE_WORKER_OK);
}

void EmptySuccessCallback(bool success) {
}
}  // namespace

void ServiceWorkerContextWrapper::DeleteForOrigin(
    const GURL& origin_url,
    const ResultCallback& result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_.get()) {
    LOG(ERROR) << "ServiceWorkerContextCore is no longer alive.";
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(result, false));
    return;
  }
  context()->UnregisterServiceWorkers(
      origin_url, base::Bind(&StatusCodeToBoolCallbackAdapter, result));
}

void ServiceWorkerContextWrapper::DeleteForOrigin(const GURL& origin_url) {
  DeleteForOrigin(origin_url, base::Bind(&EmptySuccessCallback));
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
    scoped_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::InitInternal,
                   this,
                   user_data_directory,
                   stores_task_runner,
                   base::Passed(&database_task_manager),
                   disk_cache_thread,
                   make_scoped_refptr(quota_manager_proxy),
                   make_scoped_refptr(special_storage_policy)));
    return;
  }
  DCHECK(!context_core_);
  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(new ServiceWorkerQuotaClient(this));
  }
  context_core_.reset(new ServiceWorkerContextCore(user_data_directory,
                                                   stores_task_runner,
                                                   database_task_manager.Pass(),
                                                   disk_cache_thread,
                                                   quota_manager_proxy,
                                                   special_storage_policy,
                                                   observer_list_.get(),
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
