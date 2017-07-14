// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_wrapper.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/profiler/scoped_tracker.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_quota_client.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "net/base/url_util.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"

namespace content {

namespace {

typedef std::set<std::string> HeaderNameSet;
base::LazyInstance<HeaderNameSet>::DestructorAtExit g_excluded_header_name_set =
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
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status == SERVICE_WORKER_OK) {
    // Pass the reference of |registration| to WorkerStarted callback to prevent
    // it from being deleted while starting the worker. If the refcount of
    // |registration| is 1, it will be deleted after WorkerStarted is called.
    registration->active_version()->StartWorker(
        ServiceWorkerMetrics::EventType::UNKNOWN,
        base::Bind(WorkerStarted, callback));
    return;
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, SERVICE_WORKER_ERROR_NOT_FOUND));
}

void SkipWaitingWorkerOnIO(
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK || !registration->waiting_version())
    return;

  registration->waiting_version()->set_skip_waiting(true);
  registration->ActivateWaitingVersionWhenReady();
}

void DidStartWorker(
    scoped_refptr<ServiceWorkerVersion> version,
    ServiceWorkerContext::StartActiveWorkerCallback info_callback) {
  EmbeddedWorkerInstance* instance = version->embedded_worker();
  std::move(info_callback).Run(instance->process_id(), instance->thread_id());
}

void DidFailStartWorker(base::OnceClosure error_callback,
                        ServiceWorkerStatusCode code) {
  std::move(error_callback).Run();
}

void FoundReadyRegistrationForStartActiveWorker(
    ServiceWorkerContext::StartActiveWorkerCallback info_callback,
    base::OnceClosure failure_callback,
    ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status == SERVICE_WORKER_OK) {
    // Note: There might be a remote possibility that
    // |service_worker_registration|'s active version might change between here
    // and DidStartWorker, so bind |active_version| to RunAfterStartWorker.
    scoped_refptr<ServiceWorkerVersion> active_version =
        service_worker_registration->active_version();
    DCHECK(active_version.get());
    active_version->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::EXTERNAL_REQUEST,
        base::Bind(&DidStartWorker, active_version,
                   base::Passed(&info_callback)),
        base::Bind(&DidFailStartWorker, base::Passed(&failure_callback)));
  } else {
    std::move(failure_callback).Run();
  }
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

bool ServiceWorkerContext::ScopeMatches(const GURL& scope, const GURL& url) {
  return ServiceWorkerUtils::ScopeMatches(scope, url);
}

void ServiceWorkerContextWrapper::OnRegistrationStored(int64_t registration_id,
                                                       const GURL& pattern) {
  for (auto& observer : observer_list_)
    observer.OnRegistrationStored(pattern);
}

void ServiceWorkerContextWrapper::AddObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveObserver(
    ServiceWorkerContextObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

ServiceWorkerContextWrapper::ServiceWorkerContextWrapper(
    BrowserContext* browser_context)
    : core_observer_list_(
          new base::ObserverListThreadSafe<ServiceWorkerContextCoreObserver>()),
      process_manager_(new ServiceWorkerProcessManager(browser_context)),
      is_incognito_(false),
      storage_partition_(nullptr),
      resource_context_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Add this object as an observer of the wrapped |context_core_|. This lets us
  // forward observer methods to observers outside of content.
  core_observer_list_->AddObserver(this);
}

ServiceWorkerContextWrapper::~ServiceWorkerContextWrapper() {
  // Explicitly remove this object as an observer to avoid use-after-frees in
  // tests where this object is not guaranteed to outlive the
  // ServiceWorkerContextCore it wraps.
  core_observer_list_->RemoveObserver(this);
  DCHECK(!resource_context_);
}

void ServiceWorkerContextWrapper::Init(
    const base::FilePath& user_data_directory,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy,
    ChromeBlobStorageContext* blob_context,
    URLLoaderFactoryGetter* loader_factory_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  is_incognito_ = user_data_directory.empty();
  base::SequencedWorkerPool* pool = BrowserThread::GetBlockingPool();
  std::unique_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager(
      new ServiceWorkerDatabaseTaskManagerImpl(pool));
  scoped_refptr<base::SingleThreadTaskRunner> disk_cache_thread =
      BrowserThread::GetTaskRunnerForThread(BrowserThread::CACHE);
  InitInternal(user_data_directory, std::move(database_task_manager),
               disk_cache_thread, quota_manager_proxy, special_storage_policy,
               blob_context, loader_factory_getter);
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

void ServiceWorkerContextWrapper::InitializeResourceContext(
    ResourceContext* resource_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  resource_context_ = resource_context;
}

void ServiceWorkerContextWrapper::DeleteAndStartOver() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    // The context could be null due to system shutdown or restart failure. In
    // either case, we should not have to recover the system, so just return
    // here.
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

ResourceContext* ServiceWorkerContextWrapper::resource_context() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return resource_context_;
}

static void FinishRegistrationOnIO(
    const ServiceWorkerContext::ResultCallback& continuation,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    int64_t registration_id) {
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
  if (!context_core_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(continuation, false));
    return;
  }
  ServiceWorkerRegistrationOptions options(net::SimplifyUrlForRequest(pattern));
  context()->RegisterServiceWorker(
      net::SimplifyUrlForRequest(script_url), options,
      nullptr /* provider_host */,
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
  if (!context_core_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
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
  if (!context_core_)
    return;
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
  if (!context_core_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  context_core_->storage()->FindRegistrationForPattern(
      net::SimplifyUrlForRequest(pattern),
      base::Bind(&StartActiveWorkerOnIO, callback));
}

void ServiceWorkerContextWrapper::SkipWaitingWorker(const GURL& pattern) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::SkipWaitingWorker, this,
                   pattern));
    return;
  }
  if (!context_core_)
    return;
  context_core_->storage()->FindRegistrationForPattern(
      net::SimplifyUrlForRequest(pattern), base::Bind(&SkipWaitingWorkerOnIO));
}

void ServiceWorkerContextWrapper::SetForceUpdateOnPageLoad(
    bool force_update_on_page_load) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::SetForceUpdateOnPageLoad, this,
                   force_update_on_page_load));
    return;
  }
  if (!context_core_)
    return;
  context_core_->set_force_update_on_page_load(force_update_on_page_load);
}

void ServiceWorkerContextWrapper::GetAllOriginsInfo(
    const GetUsageInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
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
    ServiceWorkerStatusCode status,
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

void ServiceWorkerContextWrapper::DidCheckHasServiceWorker(
    const CheckHasServiceWorkerCallback& callback,
    ServiceWorkerCapability capability) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, capability));
}

void ServiceWorkerContextWrapper::StopAllServiceWorkersForOrigin(
    const GURL& origin) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::StopAllServiceWorkersForOrigin,
                   this, origin));
    return;
  }
  if (!context_core_.get()) {
    return;
  }
  std::vector<ServiceWorkerVersionInfo> live_versions = GetAllLiveVersionInfo();
  for (const ServiceWorkerVersionInfo& info : live_versions) {
    ServiceWorkerVersion* version = GetLiveVersion(info.version_id);
    if (version && version->scope().GetOrigin() == origin)
      version->StopWorker(base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  }
}

void ServiceWorkerContextWrapper::DidFindRegistrationForUpdate(
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != SERVICE_WORKER_OK)
    return;
  if (!context_core_)
    return;
  DCHECK(registration);
  // TODO(jungkees): |force_bypass_cache| is set to true because the call stack
  // is initiated by an update button on DevTools that expects the cache is
  // bypassed. However, in order to provide options for callers to choose the
  // cache bypass mode, plumb |force_bypass_cache| through to
  // UpdateRegistration().
  context_core_->UpdateServiceWorker(registration.get(),
                                     true /* force_bypass_cache */);
}

namespace {

void StatusCodeToBoolCallbackAdapter(
    const ServiceWorkerContext::ResultCallback& callback,
    ServiceWorkerStatusCode code) {
  callback.Run(code == ServiceWorkerStatusCode::SERVICE_WORKER_OK);
}

}  // namespace

void ServiceWorkerContextWrapper::DeleteForOrigin(
    const GURL& origin,
    const ResultCallback& result) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::DeleteForOrigin, this, origin,
                   result));
    return;
  }
  if (!context_core_) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(result, false));
    return;
  }
  context()->UnregisterServiceWorkers(
      origin.GetOrigin(), base::Bind(&StatusCodeToBoolCallbackAdapter, result));
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
  if (!context_core_) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(callback, ServiceWorkerCapability::NO_SERVICE_WORKER));
    return;
  }
  context()->CheckHasServiceWorker(
      net::SimplifyUrlForRequest(url), net::SimplifyUrlForRequest(other_url),
      base::Bind(&ServiceWorkerContextWrapper::DidCheckHasServiceWorker, this,
                 callback));
}

void ServiceWorkerContextWrapper::CountExternalRequestsForTest(
    const GURL& origin,
    const CountExternalRequestsCallback& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::CountExternalRequestsForTest,
                   this, origin, callback));
    return;
  }

  std::vector<ServiceWorkerVersionInfo> live_version_info =
      GetAllLiveVersionInfo();
  size_t pending_external_request_count = 0;
  for (const ServiceWorkerVersionInfo& info : live_version_info) {
    ServiceWorkerVersion* version = GetLiveVersion(info.version_id);
    if (version && version->scope().GetOrigin() == origin) {
      pending_external_request_count =
          version->GetExternalRequestCountForTest();
      break;
    }
  }
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, pending_external_request_count));
}

void ServiceWorkerContextWrapper::ClearAllServiceWorkersForTest(
    const base::Closure& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::ClearAllServiceWorkersForTest,
                   this, callback));
    return;
  }
  if (!context_core_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
    return;
  }
  context_core_->ClearAllServiceWorkersForTest(callback);
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

void ServiceWorkerContextWrapper::HasMainFrameProviderHost(
    const GURL& origin,
    const BoolCallback& callback) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, false));
    return;
  }
  context_core_->HasMainFrameProviderHost(origin, callback);
}

std::unique_ptr<std::vector<std::pair<int, int>>>
ServiceWorkerContextWrapper::GetProviderHostIds(const GURL& origin) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::unique_ptr<std::vector<std::pair<int, int>>> provider_host_ids(
      new std::vector<std::pair<int, int>>());

  for (std::unique_ptr<ServiceWorkerContextCore::ProviderHostIterator> it =
           context_core_->GetClientProviderHostIterator(origin);
       !it->IsAtEnd(); it->Advance()) {
    ServiceWorkerProviderHost* provider_host = it->GetProviderHost();
    provider_host_ids->push_back(
        std::make_pair(provider_host->process_id(), provider_host->frame_id()));
  }

  return provider_host_ids;
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForDocument(
    const GURL& document_url,
    const FindRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    // FindRegistrationForDocument() can run the callback synchronously.
    callback.Run(SERVICE_WORKER_ERROR_ABORT, nullptr);
    return;
  }
  context_core_->storage()->FindRegistrationForDocument(
      net::SimplifyUrlForRequest(document_url),
      base::Bind(&ServiceWorkerContextWrapper::DidFindRegistrationForFindReady,
                 this, callback));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForPattern(
    const GURL& scope,
    const FindRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_ABORT, nullptr));
    return;
  }
  context_core_->storage()->FindRegistrationForPattern(
      net::SimplifyUrlForRequest(scope),
      base::Bind(&ServiceWorkerContextWrapper::DidFindRegistrationForFindReady,
                 this, callback));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForId(
    int64_t registration_id,
    const GURL& origin,
    const FindRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    // FindRegistrationForId() can run the callback synchronously.
    callback.Run(SERVICE_WORKER_ERROR_ABORT, nullptr);
    return;
  }
  context_core_->storage()->FindRegistrationForId(
      registration_id, origin.GetOrigin(),
      base::Bind(&ServiceWorkerContextWrapper::DidFindRegistrationForFindReady,
                 this, callback));
}

void ServiceWorkerContextWrapper::FindReadyRegistrationForIdOnly(
    int64_t registration_id,
    const FindRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    // FindRegistrationForIdOnly() can run the callback synchronously.
    callback.Run(SERVICE_WORKER_ERROR_ABORT, nullptr);
    return;
  }
  context_core_->storage()->FindRegistrationForIdOnly(
      registration_id,
      base::Bind(&ServiceWorkerContextWrapper::DidFindRegistrationForFindReady,
                 this, callback));
}

void ServiceWorkerContextWrapper::DidFindRegistrationForFindReady(
    const FindRegistrationCallback& callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (status != SERVICE_WORKER_OK) {
    callback.Run(status, nullptr);
    return;
  }

  // Attempt to activate the waiting version because the registration retrieved
  // from the disk might have only the waiting version.
  if (registration->waiting_version())
    registration->ActivateWaitingVersionWhenReady();

  scoped_refptr<ServiceWorkerVersion> active_version =
      registration->active_version();
  if (!active_version) {
    callback.Run(SERVICE_WORKER_ERROR_NOT_FOUND, nullptr);
    return;
  }

  if (active_version->status() == ServiceWorkerVersion::ACTIVATING) {
    // Wait until the version is activated.
    active_version->RegisterStatusChangeCallback(base::Bind(
        &ServiceWorkerContextWrapper::OnStatusChangedForFindReadyRegistration,
        this, callback, std::move(registration)));
    return;
  }

  DCHECK_EQ(ServiceWorkerVersion::ACTIVATED, active_version->status());
  callback.Run(SERVICE_WORKER_OK, std::move(registration));
}

void ServiceWorkerContextWrapper::OnStatusChangedForFindReadyRegistration(
    const FindRegistrationCallback& callback,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scoped_refptr<ServiceWorkerVersion> active_version =
      registration->active_version();
  if (!active_version ||
      active_version->status() != ServiceWorkerVersion::ACTIVATED) {
    callback.Run(SERVICE_WORKER_ERROR_NOT_FOUND, nullptr);
    return;
  }
  callback.Run(SERVICE_WORKER_OK, registration);
}

void ServiceWorkerContextWrapper::GetAllRegistrations(
    const GetRegistrationsInfosCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_ABORT,
                       std::vector<ServiceWorkerRegistrationInfo>()));
    return;
  }
  context_core_->storage()->GetAllRegistrationsInfos(callback);
}

void ServiceWorkerContextWrapper::GetRegistrationUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    const GetUserDataCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, std::vector<std::string>(),
                       SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  context_core_->storage()->GetUserData(registration_id, keys, callback);
}

void ServiceWorkerContextWrapper::GetRegistrationUserDataByKeyPrefix(
    int64_t registration_id,
    const std::string& key_prefix,
    const GetUserDataCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, std::vector<std::string>(),
                       SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  context_core_->storage()->GetUserDataByKeyPrefix(registration_id, key_prefix,
                                                   callback);
}

void ServiceWorkerContextWrapper::StoreRegistrationUserData(
    int64_t registration_id,
    const GURL& origin,
    const std::vector<std::pair<std::string, std::string>>& key_value_pairs,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  context_core_->storage()->StoreUserData(registration_id, origin.GetOrigin(),
                                          key_value_pairs, callback);
}

void ServiceWorkerContextWrapper::ClearRegistrationUserData(
    int64_t registration_id,
    const std::vector<std::string>& keys,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  context_core_->storage()->ClearUserData(registration_id, keys, callback);
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

void ServiceWorkerContextWrapper::GetUserDataForAllRegistrationsByKeyPrefix(
    const std::string& key_prefix,
    const GetUserDataForAllRegistrationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    RunSoon(base::Bind(callback, std::vector<std::pair<int64_t, std::string>>(),
                       SERVICE_WORKER_ERROR_ABORT));
    return;
  }
  context_core_->storage()->GetUserDataForAllRegistrationsByKeyPrefix(
      key_prefix, callback);
}

void ServiceWorkerContextWrapper::AddObserver(
    ServiceWorkerContextCoreObserver* observer) {
  core_observer_list_->AddObserver(observer);
}

void ServiceWorkerContextWrapper::RemoveObserver(
    ServiceWorkerContextCoreObserver* observer) {
  core_observer_list_->RemoveObserver(observer);
}

bool ServiceWorkerContextWrapper::OriginHasForeignFetchRegistrations(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_)
    return false;
  return context_core_->storage()->OriginHasForeignFetchRegistrations(origin);
}

void ServiceWorkerContextWrapper::InitInternal(
    const base::FilePath& user_data_directory,
    std::unique_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy,
    ChromeBlobStorageContext* blob_context,
    URLLoaderFactoryGetter* loader_factory_getter) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ServiceWorkerContextWrapper::InitInternal, this,
                   user_data_directory, base::Passed(&database_task_manager),
                   disk_cache_thread, base::RetainedRef(quota_manager_proxy),
                   base::RetainedRef(special_storage_policy),
                   base::RetainedRef(blob_context),
                   base::RetainedRef(loader_factory_getter)));
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

  base::WeakPtr<storage::BlobStorageContext> blob_storage_context =
      (blob_context && blob_context->context())
          ? blob_context->context()->AsWeakPtr()
          : nullptr;
  context_core_.reset(new ServiceWorkerContextCore(
      user_data_directory, std::move(database_task_manager), disk_cache_thread,
      quota_manager_proxy, special_storage_policy, blob_storage_context,
      loader_factory_getter, core_observer_list_.get(), this));
}

void ServiceWorkerContextWrapper::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  resource_context_ = nullptr;
  context_core_.reset();
}

bool ServiceWorkerContextWrapper::StartingExternalRequest(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerVersion* version =
      context()->GetLiveVersion(service_worker_version_id);
  if (!version)
    return false;
  return version->StartExternalRequest(request_uuid);
}

bool ServiceWorkerContextWrapper::FinishedExternalRequest(
    int64_t service_worker_version_id,
    const std::string& request_uuid) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerVersion* version =
      context()->GetLiveVersion(service_worker_version_id);
  if (!version)
    return false;
  return version->FinishExternalRequest(request_uuid);
}

void ServiceWorkerContextWrapper::StartActiveWorkerForPattern(
    const GURL& pattern,
    StartActiveWorkerCallback info_callback,
    base::OnceClosure failure_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  FindReadyRegistrationForPattern(
      pattern, base::Bind(&FoundReadyRegistrationForStartActiveWorker,
                          base::Passed(&info_callback),
                          base::Passed(&failure_callback)));
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
  context_core_->OnStorageWiped();
}

void ServiceWorkerContextWrapper::StartServiceWorkerForNavigationHint(
    const GURL& document_url,
    const StartServiceWorkerForNavigationHintCallback& callback) {
  TRACE_EVENT1("ServiceWorker", "StartServiceWorkerForNavigationHint",
               "document_url", document_url.spec());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &ServiceWorkerContextWrapper::StartServiceWorkerForNavigationHintOnIO,
          this, document_url,
          base::Bind(&ServiceWorkerContextWrapper::
                         RecordStartServiceWorkerForNavigationHintResult,
                     this, callback)));
}

void ServiceWorkerContextWrapper::StartServiceWorkerForNavigationHintOnIO(
    const GURL& document_url,
    const StartServiceWorkerForNavigationHintCallback& callback) {
  TRACE_EVENT1("ServiceWorker", "StartServiceWorkerForNavigationHintOnIO",
               "document_url", document_url.spec());
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!context_core_) {
    callback.Run(StartServiceWorkerForNavigationHintResult::FAILED);
    return;
  }
  context_core_->storage()->FindRegistrationForDocument(
      net::SimplifyUrlForRequest(document_url),
      base::Bind(
          &ServiceWorkerContextWrapper::DidFindRegistrationForNavigationHint,
          this, callback));
}

void ServiceWorkerContextWrapper::DidFindRegistrationForNavigationHint(
    const StartServiceWorkerForNavigationHintCallback& callback,
    ServiceWorkerStatusCode status,
    scoped_refptr<ServiceWorkerRegistration> registration) {
  TRACE_EVENT1("ServiceWorker", "DidFindRegistrationForNavigationHint",
               "status", ServiceWorkerStatusToString(status));
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!registration) {
    DCHECK_NE(status, SERVICE_WORKER_OK);
    callback.Run(StartServiceWorkerForNavigationHintResult::
                     NO_SERVICE_WORKER_REGISTRATION);
    return;
  }
  if (!registration->active_version()) {
    callback.Run(StartServiceWorkerForNavigationHintResult::
                     NO_ACTIVE_SERVICE_WORKER_VERSION);
    return;
  }
  if (registration->active_version()->fetch_handler_existence() ==
      ServiceWorkerVersion::FetchHandlerExistence::DOES_NOT_EXIST) {
    callback.Run(StartServiceWorkerForNavigationHintResult::NO_FETCH_HANDLER);
    return;
  }
  if (registration->active_version()->running_status() ==
      EmbeddedWorkerStatus::RUNNING) {
    callback.Run(StartServiceWorkerForNavigationHintResult::ALREADY_RUNNING);
    return;
  }

  registration->active_version()->StartWorker(
      ServiceWorkerMetrics::EventType::NAVIGATION_HINT,
      base::Bind(
          &ServiceWorkerContextWrapper::DidStartServiceWorkerForNavigationHint,
          this, registration->pattern(), callback));
}

void ServiceWorkerContextWrapper::DidStartServiceWorkerForNavigationHint(
    const GURL& pattern,
    const StartServiceWorkerForNavigationHintCallback& callback,
    ServiceWorkerStatusCode code) {
  TRACE_EVENT2("ServiceWorker", "DidStartServiceWorkerForNavigationHint", "url",
               pattern.spec(), "code", ServiceWorkerStatusToString(code));
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  callback.Run(code == SERVICE_WORKER_OK
                   ? StartServiceWorkerForNavigationHintResult::STARTED
                   : StartServiceWorkerForNavigationHintResult::FAILED);
}

void ServiceWorkerContextWrapper::
    RecordStartServiceWorkerForNavigationHintResult(
        const StartServiceWorkerForNavigationHintCallback& callback,
        StartServiceWorkerForNavigationHintResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ServiceWorkerMetrics::RecordStartServiceWorkerForNavigationHintResult(result);
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(callback, result));
}

void ServiceWorkerContextWrapper::BindWorkerFetchContext(
    int render_process_id,
    int service_worker_provider_id,
    mojom::ServiceWorkerWorkerClientAssociatedPtrInfo client_ptr_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  context()->BindWorkerFetchContext(render_process_id,
                                    service_worker_provider_id,
                                    std::move(client_ptr_info));
}

ServiceWorkerContextCore* ServiceWorkerContextWrapper::context() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return context_core_.get();
}

}  // namespace content
