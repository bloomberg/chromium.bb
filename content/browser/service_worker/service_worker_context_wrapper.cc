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
#include "base/location.h"
#include "base/logging.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_quota_client.h"
#include "content/browser/service_worker/service_worker_request_handler.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace content {

namespace {

typedef std::set<std::string> HeaderNameSet;
base::LazyInstance<HeaderNameSet> g_excluded_header_name_set =
    LAZY_INSTANCE_INITIALIZER;

void RunSoon(const base::Closure& closure) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, closure);
}

void WorkerStarted(const ServiceWorkerContextWrapper::StatusCallback& callback,
                   ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, status));
}

void StartActiveWorkerOnIO(
    const ServiceWorkerContextWrapper::StatusCallback& callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status == SERVICE_WORKER_OK) {
    // Pass the reference of |registration| to WorkerStarted callback to prevent
    // it from being deleted while starting the worker. If the refcount of
    // |registration| is 1, it will be deleted after WorkerStarted is called.
    registration->active_version()->StartWorker(
        base::Bind(WorkerStarted, callback));
    return;
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, SERVICE_WORKER_ERROR_NOT_FOUND));
}

}  // namespace

void ServiceWorkerContext::AddExcludedHeadersForFetchEvent(
    const std::set<std::string>& header_names) {
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "477117 ServiceWorkerContext::AddExcludedHeadersForFetchEvent"));
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

ServiceWorkerContext* ServiceWorkerContext::GetServiceWorkerContext(
    net::URLRequest* request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerRequestHandler* handler =
      ServiceWorkerRequestHandler::GetHandler(request);
  if (!handler || !handler->context())
    return nullptr;
  return handler->context()->wrapper_;
}

ServiceWorkerContextWrapper::ServiceWorkerContextWrapper(
    BrowserContext* browser_context)
    : observer_list_(
          new base::ObserverListThreadSafe<ServiceWorkerContextObserver>()),
      process_manager_(new ServiceWorkerProcessManager(browser_context)),
      is_incognito_(false),
      storage_partition_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerContextWrapper::~ServiceWorkerContextWrapper() {
}

void ServiceWorkerContextWrapper::Init(
    const base::FilePath& user_data_directory,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  is_incognito_ = user_data_directory.empty();
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  scoped_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager(
      new ServiceWorkerDatabaseTaskManagerImpl(pool));
  scoped_refptr<base::SingleThreadTaskRunner> disk_cache_thread =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::CACHE);
  InitInternal(user_data_directory,
               database_task_manager.Pass(),
               disk_cache_thread,
               quota_manager_proxy,
               special_storage_policy);
}

void ServiceWorkerContextWrapper::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  storage_partition_ = nullptr;
  process_manager_->Shutdown();
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ServiceWorkerContextWrapper::ShutdownOnIO, this));
}

void ServiceWorkerContextWrapper::DeleteAndStartOver() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    // The context could be null due to system shutdown or restart failure. In
    // either case, we should not have to recover the system, so just return
    // here.
    LOG(ERROR) << "ServiceWorkerContextCore is no longer alive.";
    return;
  }
  context_core_->DeleteAndStartOver(
      base::Bind(&ServiceWorkerContextWrapper::DidDeleteAndStartOver, this));
}

StoragePartitionImpl* ServiceWorkerContextWrapper::storage_partition() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return storage_partition_;
}

void ServiceWorkerContextWrapper::set_storage_partition(
    StoragePartitionImpl* storage_partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_ = storage_partition;
}

static void FinishRegistrationOnIO(
    const ServiceWorkerContext::ResultCallback& continuation,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
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
      net::SimplifyUrlForRequest(pattern),
      net::SimplifyUrlForRequest(script_url), NULL /* provider_host */,
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
      net::SimplifyUrlForRequest(pattern),
      base::Bind(&FinishUnregistrationOnIO, continuation));
}

void ServiceWorkerContextWrapper::UpdateRegistration(const GURL& pattern) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::UpdateRegistration, this,
                   pattern));
    return;
  }
  if (!context_core_.get()) {
    LOG(ERROR) << "ServiceWorkerContextCore is no longer alive.";
    return;
  }
  context_core_->storage()->FindRegistrationForPattern(
      net::SimplifyUrlForRequest(pattern),
      base::Bind(&ServiceWorkerContextWrapper::DidFindRegistrationForUpdate,
                 this));
}

void ServiceWorkerContextWrapper::StartServiceWorker(
    const GURL& pattern,
    const StatusCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::StartServiceWorker, this,
                   pattern, callback));
    return;
  }
  if (!context_core_.get()) {
    LOG(ERROR) << "ServiceWorkerContextCore is no longer alive.";
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  context_core_->storage()->FindRegistrationForPattern(
      net::SimplifyUrlForRequest(pattern),
      base::Bind(&StartActiveWorkerOnIO, callback));
}

void ServiceWorkerContextWrapper::SimulateSkipWaiting(int64_t version_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::SimulateSkipWaiting, this,
                   version_id));
    return;
  }
  if (!context_core_.get()) {
    LOG(ERROR) << "ServiceWorkerContextCore is no longer alive.";
    return;
  }
  ServiceWorkerVersion* version = GetLiveVersion(version_id);
  if (!version || version->skip_waiting())
    return;
  ServiceWorkerRegistration* registration =
      GetLiveRegistration(version->registration_id());
  if (!registration || version != registration->waiting_version())
    return;
  version->set_skip_waiting(true);
  registration->ActivateWaitingVersionWhenReady();
}

static void DidFindRegistrationForDocument(
    const net::CompletionCallback& callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  int rv = registration ? net::OK : net::ERR_CACHE_MISS;
  // Use RunSoon here because FindRegistrationForDocument can complete
  // immediately but CanHandleMainResourceOffline must be async.
  RunSoon(base::Bind(callback, rv));
}

void ServiceWorkerContextWrapper::CanHandleMainResourceOffline(
      const GURL& url,
      const GURL& first_party,
      const net::CompletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context()->storage()->FindRegistrationForDocument(
      net::SimplifyUrlForRequest(url),
      base::Bind(&DidFindRegistrationForDocument, callback));
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
  context()->storage()->GetAllRegistrationsInfos(base::Bind(
      &ServiceWorkerContextWrapper::DidGetAllRegistrationsForGetAllOrigins,
      this, callback));
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

void ServiceWorkerContextWrapper::DidFindRegistrationForCheckHasServiceWorker(
    const GURL& other_url,
    const CheckHasServiceWorkerCallback& callback,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != SERVICE_WORKER_OK) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback, false));
    return;
  }

  DCHECK(registration);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, registration->active_version() &&
                               ServiceWorkerUtils::ScopeMatches(
                                   registration->pattern(), other_url)));
}

void ServiceWorkerContextWrapper::DidFindRegistrationForUpdate(
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != SERVICE_WORKER_OK)
    return;
  if (!context_core_.get()) {
    LOG(ERROR) << "ServiceWorkerContextCore is no longer alive.";
    return;
  }
  DCHECK(registration);
  context_core_->UpdateServiceWorker(registration.get(),
                                     true /* force_bypass_cache */);
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
    const GURL& origin,
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
      origin.GetOrigin(), base::Bind(&StatusCodeToBoolCallbackAdapter, result));
}

void ServiceWorkerContextWrapper::DeleteForOrigin(const GURL& origin) {
  DeleteForOrigin(origin, base::Bind(&EmptySuccessCallback));
}

void ServiceWorkerContextWrapper::CheckHasServiceWorker(
    const GURL& url,
    const GURL& other_url,
    const CheckHasServiceWorkerCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::CheckHasServiceWorker, this,
                   url, other_url, callback));
    return;
  }
  if (!context_core_.get()) {
    LOG(ERROR) << "ServiceWorkerContextCore is no longer alive.";
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(callback, false));
    return;
  }
  context()->storage()->FindRegistrationForDocument(
      net::SimplifyUrlForRequest(url),
      base::Bind(&ServiceWorkerContextWrapper::
                     DidFindRegistrationForCheckHasServiceWorker,
                 this, net::SimplifyUrlForRequest(other_url), callback));
}

ServiceWorkerRegistration* ServiceWorkerContextWrapper::GetLiveRegistration(
    int64_t registration_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_)
    return nullptr;
  return context_core_->GetLiveRegistration(registration_id);
}

ServiceWorkerVersion* ServiceWorkerContextWrapper::GetLiveVersion(
    int64_t version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_)
    return nullptr;
  return context_core_->GetLiveVersion(version_id);
}

std::vector<ServiceWorkerRegistrationInfo>
ServiceWorkerContextWrapper::GetAllLiveRegistrationInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_)
    return std::vector<ServiceWorkerRegistrationInfo>();
  return context_core_->GetAllLiveRegistrationInfo();
}

std::vector<ServiceWorkerVersionInfo>
ServiceWorkerContextWrapper::GetAllLiveVersionInfo() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_)
    return std::vector<ServiceWorkerVersionInfo>();
  return context_core_->GetAllLiveVersionInfo();
}

void ServiceWorkerContextWrapper::FindRegistrationForDocument(
    const GURL& document_url,
    const FindRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    // FindRegistrationForDocument() can run the callback synchronously.
    callback.Run(SERVICE_WORKER_ERROR_ABORT, nullptr);
    return;
  }
  context_core_->storage()->FindRegistrationForDocument(
      net::SimplifyUrlForRequest(document_url), callback);
}

void ServiceWorkerContextWrapper::FindRegistrationForId(
    int64_t registration_id,
    const GURL& origin,
    const FindRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    // FindRegistrationForId() can run the callback synchronously.
    callback.Run(SERVICE_WORKER_ERROR_ABORT, nullptr);
    return;
  }
  context_core_->storage()->FindRegistrationForId(registration_id,
                                                  origin.GetOrigin(), callback);
}

void ServiceWorkerContextWrapper::GetAllRegistrations(
    const GetRegistrationsInfosCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, std::vector<ServiceWorkerRegistrationInfo>()));
    return;
  }
  context_core_->storage()->GetAllRegistrationsInfos(callback);
}

void ServiceWorkerContextWrapper::GetRegistrationUserData(
    int64_t registration_id,
    const std::string& key,
    const GetUserDataCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, std::string(), SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  context_core_->storage()->GetUserData(registration_id, key, callback);
}

void ServiceWorkerContextWrapper::StoreRegistrationUserData(
    int64_t registration_id,
    const GURL& origin,
    const std::string& key,
    const std::string& data,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  context_core_->storage()->StoreUserData(registration_id, origin.GetOrigin(),
                                          key, data, callback);
}

void ServiceWorkerContextWrapper::ClearRegistrationUserData(
    int64_t registration_id,
    const std::string& key,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  context_core_->storage()->ClearUserData(registration_id, key, callback);
}

void ServiceWorkerContextWrapper::GetUserDataForAllRegistrations(
    const std::string& key,
    const GetUserDataForAllRegistrationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, std::vector<std::pair<int64_t, std::string>>(),
                       SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  context_core_->storage()->GetUserDataForAllRegistrations(key, callback);
}

void ServiceWorkerContextWrapper::AddObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_->RemoveObserver(observer);
}

void ServiceWorkerContextWrapper::InitInternal(
    const base::FilePath& user_data_directory,
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
                   base::Passed(&database_task_manager),
                   disk_cache_thread,
                   make_scoped_refptr(quota_manager_proxy),
                   make_scoped_refptr(special_storage_policy)));
    return;
  }
  // TODO(pkasting): Remove ScopedTracker below once crbug.com/477117 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "477117 ServiceWorkerContextWrapper::InitInternal"));
  DCHECK(!context_core_);
  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(new ServiceWorkerQuotaClient(this));
  }
  context_core_.reset(new ServiceWorkerContextCore(user_data_directory,
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

  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextObserver::OnStorageWiped);
}

ServiceWorkerContextCore* ServiceWorkerContextWrapper::context() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return context_core_.get();
}

}  // namespace content
