// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_core.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_observer.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_database_task_manager.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_process_manager.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "url/gurl.h"

namespace content {
namespace {

void SuccessCollectorCallback(const base::Closure& done_closure,
                              bool* overall_success,
                              ServiceWorkerStatusCode status) {
  if (status != ServiceWorkerStatusCode::SERVICE_WORKER_OK) {
    *overall_success = false;
  }
  done_closure.Run();
}

void SuccessReportingCallback(
    const bool* success,
    const ServiceWorkerContextCore::UnregistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool result = *success;
  callback.Run(result ? ServiceWorkerStatusCode::SERVICE_WORKER_OK
                      : ServiceWorkerStatusCode::SERVICE_WORKER_ERROR_FAILED);
}

bool IsSameOriginClientProviderHost(const GURL& origin,
                                    ServiceWorkerProviderHost* host) {
  return host->provider_type() == SERVICE_WORKER_PROVIDER_FOR_CONTROLLEE &&
         host->document_url().GetOrigin() == origin;
}

}  // namespace

const base::FilePath::CharType
    ServiceWorkerContextCore::kServiceWorkerDirectory[] =
        FILE_PATH_LITERAL("Service Worker");

ServiceWorkerContextCore::ProviderHostIterator::~ProviderHostIterator() {}

ServiceWorkerProviderHost*
ServiceWorkerContextCore::ProviderHostIterator::GetProviderHost() {
  DCHECK(!IsAtEnd());
  return provider_host_iterator_->GetCurrentValue();
}

void ServiceWorkerContextCore::ProviderHostIterator::Advance() {
  DCHECK(!IsAtEnd());
  DCHECK(!provider_host_iterator_->IsAtEnd());
  DCHECK(!process_iterator_->IsAtEnd());

  // Advance the inner iterator. If an element is reached, we're done.
  provider_host_iterator_->Advance();
  if (ForwardUntilMatchingProviderHost())
    return;

  // Advance the outer iterator until an element is reached, or end is hit.
  while (true) {
    process_iterator_->Advance();
    if (process_iterator_->IsAtEnd())
      return;
    ProviderMap* provider_map = process_iterator_->GetCurrentValue();
    provider_host_iterator_.reset(new ProviderMap::iterator(provider_map));
    if (ForwardUntilMatchingProviderHost())
      return;
  }
}

bool ServiceWorkerContextCore::ProviderHostIterator::IsAtEnd() {
  return process_iterator_->IsAtEnd() &&
         (!provider_host_iterator_ || provider_host_iterator_->IsAtEnd());
}

ServiceWorkerContextCore::ProviderHostIterator::ProviderHostIterator(
    ProcessToProviderMap* map,
    const ProviderHostPredicate& predicate)
    : map_(map), predicate_(predicate) {
  DCHECK(map);
  Initialize();
}

void ServiceWorkerContextCore::ProviderHostIterator::Initialize() {
  process_iterator_.reset(new ProcessToProviderMap::iterator(map_));
  // Advance to the first element.
  while (!process_iterator_->IsAtEnd()) {
    ProviderMap* provider_map = process_iterator_->GetCurrentValue();
    provider_host_iterator_.reset(new ProviderMap::iterator(provider_map));
    if (ForwardUntilMatchingProviderHost())
      return;
    process_iterator_->Advance();
  }
}

bool ServiceWorkerContextCore::ProviderHostIterator::
    ForwardUntilMatchingProviderHost() {
  while (!provider_host_iterator_->IsAtEnd()) {
    if (predicate_.is_null() || predicate_.Run(GetProviderHost()))
      return true;
    provider_host_iterator_->Advance();
  }
  return false;
}

ServiceWorkerContextCore::ServiceWorkerContextCore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner,
    scoped_ptr<ServiceWorkerDatabaseTaskManager> database_task_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& disk_cache_thread,
    storage::QuotaManagerProxy* quota_manager_proxy,
    storage::SpecialStoragePolicy* special_storage_policy,
    ObserverListThreadSafe<ServiceWorkerContextObserver>* observer_list,
    ServiceWorkerContextWrapper* wrapper)
    : wrapper_(wrapper),
      providers_(new ProcessToProviderMap),
      provider_by_uuid_(new ProviderByClientUUIDMap),
      cache_manager_(ServiceWorkerCacheStorageManager::Create(
          path,
          cache_task_runner.get(),
          make_scoped_refptr(quota_manager_proxy))),
      next_handle_id_(0),
      next_registration_handle_id_(0),
      observer_list_(observer_list),
      weak_factory_(this) {
  // These get a WeakPtr from weak_factory_, so must be set after weak_factory_
  // is initialized.
  storage_ = ServiceWorkerStorage::Create(path,
                                          AsWeakPtr(),
                                          database_task_manager.Pass(),
                                          disk_cache_thread,
                                          quota_manager_proxy,
                                          special_storage_policy);
  embedded_worker_registry_ = EmbeddedWorkerRegistry::Create(AsWeakPtr());
  job_coordinator_.reset(new ServiceWorkerJobCoordinator(AsWeakPtr()));
}

ServiceWorkerContextCore::ServiceWorkerContextCore(
    ServiceWorkerContextCore* old_context,
    ServiceWorkerContextWrapper* wrapper)
    : wrapper_(wrapper),
      providers_(old_context->providers_.release()),
      provider_by_uuid_(old_context->provider_by_uuid_.release()),
      cache_manager_(ServiceWorkerCacheStorageManager::Create(
          old_context->cache_manager())),
      next_handle_id_(old_context->next_handle_id_),
      next_registration_handle_id_(old_context->next_registration_handle_id_),
      observer_list_(old_context->observer_list_),
      weak_factory_(this) {
  // These get a WeakPtr from weak_factory_, so must be set after weak_factory_
  // is initialized.
  storage_ = ServiceWorkerStorage::Create(AsWeakPtr(), old_context->storage());
  embedded_worker_registry_ = EmbeddedWorkerRegistry::Create(
      AsWeakPtr(),
      old_context->embedded_worker_registry());
  job_coordinator_.reset(new ServiceWorkerJobCoordinator(AsWeakPtr()));
}

ServiceWorkerContextCore::~ServiceWorkerContextCore() {
  for (VersionMap::iterator it = live_versions_.begin();
       it != live_versions_.end();
       ++it) {
    it->second->RemoveListener(this);
  }
  weak_factory_.InvalidateWeakPtrs();
}

ServiceWorkerProviderHost* ServiceWorkerContextCore::GetProviderHost(
    int process_id, int provider_id) {
  ProviderMap* map = GetProviderMapForProcess(process_id);
  if (!map)
    return NULL;
  return map->Lookup(provider_id);
}

void ServiceWorkerContextCore::AddProviderHost(
    scoped_ptr<ServiceWorkerProviderHost> host) {
  ServiceWorkerProviderHost* host_ptr = host.release();   // we take ownership
  ProviderMap* map = GetProviderMapForProcess(host_ptr->process_id());
  if (!map) {
    map = new ProviderMap;
    providers_->AddWithID(map, host_ptr->process_id());
  }
  map->AddWithID(host_ptr, host_ptr->provider_id());
}

void ServiceWorkerContextCore::RemoveProviderHost(
    int process_id, int provider_id) {
  ProviderMap* map = GetProviderMapForProcess(process_id);
  DCHECK(map);
  map->Remove(provider_id);
}

void ServiceWorkerContextCore::RemoveAllProviderHostsForProcess(
    int process_id) {
  if (providers_->Lookup(process_id))
    providers_->Remove(process_id);
}

scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator>
ServiceWorkerContextCore::GetProviderHostIterator() {
  return make_scoped_ptr(new ProviderHostIterator(
      providers_.get(), ProviderHostIterator::ProviderHostPredicate()));
}

scoped_ptr<ServiceWorkerContextCore::ProviderHostIterator>
ServiceWorkerContextCore::GetClientProviderHostIterator(const GURL& origin) {
  return make_scoped_ptr(new ProviderHostIterator(
      providers_.get(), base::Bind(IsSameOriginClientProviderHost, origin)));
}

void ServiceWorkerContextCore::RegisterProviderHostByClientID(
    const std::string& client_uuid,
    ServiceWorkerProviderHost* provider_host) {
  DCHECK(!ContainsKey(*provider_by_uuid_, client_uuid));
  (*provider_by_uuid_)[client_uuid] = provider_host;
}

void ServiceWorkerContextCore::UnregisterProviderHostByClientID(
    const std::string& client_uuid) {
  DCHECK(ContainsKey(*provider_by_uuid_, client_uuid));
  provider_by_uuid_->erase(client_uuid);
}

ServiceWorkerProviderHost* ServiceWorkerContextCore::GetProviderHostByClientID(
    const std::string& client_uuid) {
  auto found = provider_by_uuid_->find(client_uuid);
  if (found == provider_by_uuid_->end())
    return nullptr;
  return found->second;
}

void ServiceWorkerContextCore::RegisterServiceWorker(
    const GURL& pattern,
    const GURL& script_url,
    ServiceWorkerProviderHost* provider_host,
    const RegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (storage()->IsDisabled()) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT, std::string(),
                 kInvalidServiceWorkerRegistrationId);
    return;
  }

  job_coordinator_->Register(
      pattern,
      script_url,
      provider_host,
      base::Bind(&ServiceWorkerContextCore::RegistrationComplete,
                 AsWeakPtr(),
                 pattern,
                 callback));
}

void ServiceWorkerContextCore::UnregisterServiceWorker(
    const GURL& pattern,
    const UnregistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (storage()->IsDisabled()) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }

  job_coordinator_->Unregister(
      pattern,
      base::Bind(&ServiceWorkerContextCore::UnregistrationComplete,
                 AsWeakPtr(),
                 pattern,
                 callback));
}

void ServiceWorkerContextCore::UnregisterServiceWorkers(
    const GURL& origin,
    const UnregistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (storage()->IsDisabled()) {
    // Not posting as new task to match implementations above.
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }

  storage()->GetAllRegistrations(base::Bind(
      &ServiceWorkerContextCore::DidGetAllRegistrationsForUnregisterForOrigin,
      AsWeakPtr(),
      callback,
      origin));
}

void ServiceWorkerContextCore::DidGetAllRegistrationsForUnregisterForOrigin(
    const UnregistrationCallback& result,
    const GURL& origin,
    const std::vector<ServiceWorkerRegistrationInfo>& registrations) {
  std::set<GURL> scopes;
  for (const auto& registration_info : registrations) {
    if (origin == registration_info.pattern.GetOrigin()) {
      scopes.insert(registration_info.pattern);
    }
  }
  bool* overall_success = new bool(true);
  base::Closure barrier = base::BarrierClosure(
      scopes.size(),
      base::Bind(
          &SuccessReportingCallback, base::Owned(overall_success), result));

  for (const GURL& scope : scopes) {
    UnregisterServiceWorker(
        scope, base::Bind(&SuccessCollectorCallback, barrier, overall_success));
  }
}

void ServiceWorkerContextCore::UpdateServiceWorker(
    ServiceWorkerRegistration* registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (storage()->IsDisabled())
    return;
  job_coordinator_->Update(registration);
}

void ServiceWorkerContextCore::RegistrationComplete(
    const GURL& pattern,
    const ServiceWorkerContextCore::RegistrationCallback& callback,
    ServiceWorkerStatusCode status,
    const std::string& status_message,
    ServiceWorkerRegistration* registration) {
  if (status != SERVICE_WORKER_OK) {
    DCHECK(!registration);
    callback.Run(status, status_message, kInvalidServiceWorkerRegistrationId);
    return;
  }

  DCHECK(registration);
  callback.Run(status, status_message, registration->id());
  if (observer_list_.get()) {
    observer_list_->Notify(FROM_HERE,
                           &ServiceWorkerContextObserver::OnRegistrationStored,
                           registration->id(), pattern);
  }
}

void ServiceWorkerContextCore::UnregistrationComplete(
    const GURL& pattern,
    const ServiceWorkerContextCore::UnregistrationCallback& callback,
    int64 registration_id,
    ServiceWorkerStatusCode status) {
  callback.Run(status);
  if (observer_list_.get()) {
    observer_list_->Notify(FROM_HERE,
                           &ServiceWorkerContextObserver::OnRegistrationDeleted,
                           registration_id, pattern);
  }
}

ServiceWorkerRegistration* ServiceWorkerContextCore::GetLiveRegistration(
    int64 id) {
  RegistrationsMap::iterator it = live_registrations_.find(id);
  return (it != live_registrations_.end()) ? it->second : NULL;
}

void ServiceWorkerContextCore::AddLiveRegistration(
    ServiceWorkerRegistration* registration) {
  DCHECK(!GetLiveRegistration(registration->id()));
  live_registrations_[registration->id()] = registration;
  if (observer_list_.get()) {
    observer_list_->Notify(FROM_HERE,
                           &ServiceWorkerContextObserver::OnNewLiveRegistration,
                           registration->id(), registration->pattern());
  }
}

void ServiceWorkerContextCore::RemoveLiveRegistration(int64 id) {
  live_registrations_.erase(id);
}

ServiceWorkerVersion* ServiceWorkerContextCore::GetLiveVersion(
    int64 id) {
  VersionMap::iterator it = live_versions_.find(id);
  return (it != live_versions_.end()) ? it->second : NULL;
}

void ServiceWorkerContextCore::AddLiveVersion(ServiceWorkerVersion* version) {
  DCHECK(!GetLiveVersion(version->version_id()));
  live_versions_[version->version_id()] = version;
  version->AddListener(this);
  if (observer_list_.get()) {
    observer_list_->Notify(FROM_HERE,
                           &ServiceWorkerContextObserver::OnNewLiveVersion,
                           version->version_id(), version->registration_id(),
                           version->script_url());
  }
}

void ServiceWorkerContextCore::RemoveLiveVersion(int64 id) {
  live_versions_.erase(id);
}

std::vector<ServiceWorkerRegistrationInfo>
ServiceWorkerContextCore::GetAllLiveRegistrationInfo() {
  std::vector<ServiceWorkerRegistrationInfo> infos;
  for (std::map<int64, ServiceWorkerRegistration*>::const_iterator iter =
           live_registrations_.begin();
       iter != live_registrations_.end();
       ++iter) {
    infos.push_back(iter->second->GetInfo());
  }
  return infos;
}

std::vector<ServiceWorkerVersionInfo>
ServiceWorkerContextCore::GetAllLiveVersionInfo() {
  std::vector<ServiceWorkerVersionInfo> infos;
  for (std::map<int64, ServiceWorkerVersion*>::const_iterator iter =
           live_versions_.begin();
       iter != live_versions_.end();
       ++iter) {
    infos.push_back(iter->second->GetInfo());
  }
  return infos;
}

int ServiceWorkerContextCore::GetNewServiceWorkerHandleId() {
  return next_handle_id_++;
}

int ServiceWorkerContextCore::GetNewRegistrationHandleId() {
  return next_registration_handle_id_++;
}

void ServiceWorkerContextCore::ScheduleDeleteAndStartOver() const {
  storage_->Disable();
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ServiceWorkerContextWrapper::DeleteAndStartOver, wrapper_));
}

void ServiceWorkerContextCore::DeleteAndStartOver(
    const StatusCallback& callback) {
  job_coordinator_->AbortAll();
  storage_->DeleteAndStartOver(callback);
}

void ServiceWorkerContextCore::SetBlobParametersForCache(
    net::URLRequestContext* request_context,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  cache_manager_->SetBlobParametersForCache(request_context,
                                            blob_storage_context);
}

scoped_ptr<ServiceWorkerProviderHost>
ServiceWorkerContextCore::TransferProviderHostOut(
    int process_id, int provider_id) {
  ProviderMap* map = GetProviderMapForProcess(process_id);
  ServiceWorkerProviderHost* transferee = map->Lookup(provider_id);
  ServiceWorkerProviderHost* replacement =
      new ServiceWorkerProviderHost(process_id,
                                    transferee->frame_id(),
                                    provider_id,
                                    transferee->provider_type(),
                                    AsWeakPtr(),
                                    transferee->dispatcher_host());
  map->Replace(provider_id, replacement);
  transferee->PrepareForCrossSiteTransfer();
  return make_scoped_ptr(transferee);
}

void ServiceWorkerContextCore::TransferProviderHostIn(
    int new_process_id, int new_provider_id,
    scoped_ptr<ServiceWorkerProviderHost> transferee) {
  ProviderMap* map = GetProviderMapForProcess(new_process_id);
  ServiceWorkerProviderHost* temp = map->Lookup(new_provider_id);
  DCHECK(temp->document_url().is_empty());
  transferee->CompleteCrossSiteTransfer(new_process_id,
                                        temp->frame_id(),
                                        new_provider_id,
                                        temp->provider_type(),
                                        temp->dispatcher_host());
  map->Replace(new_provider_id, transferee.release());
  delete temp;
}

void ServiceWorkerContextCore::OnRunningStateChanged(
    ServiceWorkerVersion* version) {
  if (!observer_list_.get())
    return;
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextObserver::OnRunningStateChanged,
                         version->version_id(), version->running_status());
}

void ServiceWorkerContextCore::OnVersionStateChanged(
    ServiceWorkerVersion* version) {
  if (!observer_list_.get())
    return;
  observer_list_->Notify(FROM_HERE,
                         &ServiceWorkerContextObserver::OnVersionStateChanged,
                         version->version_id(), version->status());
}

void ServiceWorkerContextCore::OnErrorReported(
    ServiceWorkerVersion* version,
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  if (!observer_list_.get())
    return;
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextObserver::OnErrorReported,
      version->version_id(), version->embedded_worker()->process_id(),
      version->embedded_worker()->thread_id(),
      ServiceWorkerContextObserver::ErrorInfo(error_message, line_number,
                                              column_number, source_url));
}

void ServiceWorkerContextCore::OnReportConsoleMessage(
    ServiceWorkerVersion* version,
    int source_identifier,
    int message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  if (!observer_list_.get())
    return;
  observer_list_->Notify(
      FROM_HERE, &ServiceWorkerContextObserver::OnReportConsoleMessage,
      version->version_id(), version->embedded_worker()->process_id(),
      version->embedded_worker()->thread_id(),
      ServiceWorkerContextObserver::ConsoleMessage(
          source_identifier, message_level, message, line_number, source_url));
}

ServiceWorkerProcessManager* ServiceWorkerContextCore::process_manager() {
  return wrapper_->process_manager();
}

}  // namespace content
