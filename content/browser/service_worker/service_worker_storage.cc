// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage.h"

#include <string>

#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace content {

namespace {

typedef base::Callback<void(
    ServiceWorkerStorage::InitialData* data,
    ServiceWorkerDatabase::Status status)> InitializeCallback;
typedef base::Callback<void(
    const ServiceWorkerDatabase::RegistrationData& data,
    const std::vector<ServiceWorkerDatabase::ResourceRecord>& resources,
    ServiceWorkerDatabase::Status status)> ReadRegistrationCallback;
typedef base::Callback<void(
    bool origin_is_deletable,
    ServiceWorkerDatabase::Status status)> DeleteRegistrationCallback;

void RunSoon(const tracked_objects::Location& from_here,
             const base::Closure& closure) {
  base::MessageLoop::current()->PostTask(from_here, closure);
}

void CompleteFindNow(
    const scoped_refptr<ServiceWorkerRegistration>& registration,
    ServiceWorkerStatusCode status,
    const ServiceWorkerStorage::FindRegistrationCallback& callback) {
  callback.Run(status, registration);
}

void CompleteFindSoon(
    const tracked_objects::Location& from_here,
    const scoped_refptr<ServiceWorkerRegistration>& registration,
    ServiceWorkerStatusCode status,
    const ServiceWorkerStorage::FindRegistrationCallback& callback) {
  RunSoon(from_here, base::Bind(callback, status, registration));
}

const base::FilePath::CharType kServiceWorkerDirectory[] =
    FILE_PATH_LITERAL("Service Worker");
const base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("Database");

const int kMaxMemDiskCacheSize = 10 * 1024 * 1024;

void EmptyCompletionCallback(int) {}

ServiceWorkerStatusCode DatabaseStatusToStatusCode(
    ServiceWorkerDatabase::Status status) {
  switch (status) {
    case ServiceWorkerDatabase::STATUS_OK:
      return SERVICE_WORKER_OK;
    case ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND:
      return SERVICE_WORKER_ERROR_NOT_FOUND;
    default:
      return SERVICE_WORKER_ERROR_FAILED;
  }
}

void ReadInitialDataFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const InitializeCallback& callback) {
  DCHECK(database);
  scoped_ptr<ServiceWorkerStorage::InitialData> data(
      new ServiceWorkerStorage::InitialData());

  ServiceWorkerDatabase::Status status =
      database->GetNextAvailableIds(&data->next_registration_id,
                                    &data->next_version_id,
                                    &data->next_resource_id);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, base::Owned(data.release()), status));
    return;
  }

  status = database->GetOriginsWithRegistrations(&data->origins);
  original_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, base::Owned(data.release()), status));
}

void DeleteRegistrationFromDB(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    int64 registration_id,
    const GURL& origin,
    const DeleteRegistrationCallback& callback) {
  DCHECK(database);
  ServiceWorkerDatabase::Status status =
      database->DeleteRegistration(registration_id, origin);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, false, status));
    return;
  }

  // TODO(nhiroki): Add convenient method to ServiceWorkerDatabase to check the
  // unique origin list.
  std::vector<ServiceWorkerDatabase::RegistrationData> registrations;
  status = database->GetRegistrationsForOrigin(origin, &registrations);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    original_task_runner->PostTask(
        FROM_HERE, base::Bind(callback, false, status));
    return;
  }

  bool deletable = registrations.empty();
  original_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, deletable, status));
}

}  // namespace

ServiceWorkerStorage::InitialData::InitialData()
    : next_registration_id(kInvalidServiceWorkerRegistrationId),
      next_version_id(kInvalidServiceWorkerVersionId),
      next_resource_id(kInvalidServiceWorkerResourceId) {
}

ServiceWorkerStorage::InitialData::~InitialData() {
}

ServiceWorkerStorage::ServiceWorkerStorage(
    const base::FilePath& path,
    base::WeakPtr<ServiceWorkerContextCore> context,
    base::SequencedTaskRunner* database_task_runner,
    base::MessageLoopProxy* disk_cache_thread,
    quota::QuotaManagerProxy* quota_manager_proxy)
    : next_registration_id_(kInvalidServiceWorkerRegistrationId),
      next_version_id_(kInvalidServiceWorkerVersionId),
      next_resource_id_(kInvalidServiceWorkerResourceId),
      state_(UNINITIALIZED),
      context_(context),
      database_task_runner_(database_task_runner),
      disk_cache_thread_(disk_cache_thread),
      quota_manager_proxy_(quota_manager_proxy),
      weak_factory_(this) {
  if (!path.empty()) {
    path_ = path.Append(kServiceWorkerDirectory);
    database_.reset(new ServiceWorkerDatabase(path_.Append(kDatabaseName)));
  } else {
    // Create an in-memory database.
    database_.reset(new ServiceWorkerDatabase(base::FilePath()));
  }
}

ServiceWorkerStorage::~ServiceWorkerStorage() {
  weak_factory_.InvalidateWeakPtrs();
  database_task_runner_->DeleteSoon(FROM_HERE, database_.release());
}

void ServiceWorkerStorage::FindRegistrationForPattern(
    const GURL& scope,
    const FindRegistrationCallback& callback) {
  scoped_refptr<ServiceWorkerRegistration> null_registration;
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::FindRegistrationForPattern,
          weak_factory_.GetWeakPtr(), scope, callback))) {
    if (state_ != INITIALIZING || !context_) {
      CompleteFindSoon(FROM_HERE, null_registration,
                       SERVICE_WORKER_ERROR_FAILED, callback);
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  // See if there are any stored registrations for the origin.
  if (!ContainsKey(registered_origins_, scope.GetOrigin())) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForPattern(scope);
    if (installing_registration) {
      CompleteFindSoon(
          FROM_HERE, installing_registration, SERVICE_WORKER_OK, callback);
      return;
    }
    CompleteFindSoon(
        FROM_HERE, null_registration, SERVICE_WORKER_ERROR_NOT_FOUND, callback);
    return;
  }

  RegistrationList* registrations = new RegistrationList();
  PostTaskAndReplyWithResult(
      database_task_runner_,
      FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::GetRegistrationsForOrigin,
                 base::Unretained(database_.get()),
                 scope.GetOrigin(), base::Unretained(registrations)),
      base::Bind(&ServiceWorkerStorage::DidGetRegistrationsForPattern,
                 weak_factory_.GetWeakPtr(), scope, callback,
                 base::Owned(registrations)));
}

void ServiceWorkerStorage::FindRegistrationForDocument(
    const GURL& document_url,
    const FindRegistrationCallback& callback) {
  scoped_refptr<ServiceWorkerRegistration> null_registration;
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::FindRegistrationForDocument,
          weak_factory_.GetWeakPtr(), document_url, callback))) {
    if (state_ != INITIALIZING || !context_)
      CompleteFindNow(null_registration, SERVICE_WORKER_ERROR_FAILED, callback);
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  // See if there are any stored registrations for the origin.
  if (!ContainsKey(registered_origins_, document_url.GetOrigin())) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForDocument(document_url);
    if (installing_registration) {
      CompleteFindNow(installing_registration, SERVICE_WORKER_OK, callback);
      return;
    }
    CompleteFindNow(
        null_registration, SERVICE_WORKER_ERROR_NOT_FOUND, callback);
    return;
  }

  RegistrationList* registrations = new RegistrationList();
  PostTaskAndReplyWithResult(
      database_task_runner_,
      FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::GetRegistrationsForOrigin,
                 base::Unretained(database_.get()),
                 document_url.GetOrigin(), base::Unretained(registrations)),
      base::Bind(&ServiceWorkerStorage::DidGetRegistrationsForDocument,
                 weak_factory_.GetWeakPtr(), document_url, callback,
                 base::Owned(registrations)));
}

void ServiceWorkerStorage::FindRegistrationForId(
    int64 registration_id,
    const GURL& origin,
    const FindRegistrationCallback& callback) {
  scoped_refptr<ServiceWorkerRegistration> null_registration;
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::FindRegistrationForId,
          weak_factory_.GetWeakPtr(), registration_id, origin, callback))) {
    if (state_ != INITIALIZING || !context_)
      CompleteFindNow(null_registration, SERVICE_WORKER_ERROR_FAILED, callback);
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  // See if there are any stored registrations for the origin.
  if (!ContainsKey(registered_origins_, origin)) {
    // Look for somthing currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForId(registration_id);
    if (installing_registration) {
      CompleteFindNow(installing_registration, SERVICE_WORKER_OK, callback);
      return;
    }
    CompleteFindNow(
        null_registration, SERVICE_WORKER_ERROR_NOT_FOUND, callback);
    return;
  }

  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    CompleteFindNow(registration, SERVICE_WORKER_OK, callback);
    return;
  }

  ServiceWorkerDatabase::RegistrationData* data =
      new ServiceWorkerDatabase::RegistrationData;
  ResourceList* resources = new ResourceList;
  PostTaskAndReplyWithResult(
      database_task_runner_,
      FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::ReadRegistration,
                 base::Unretained(database_.get()),
                 registration_id, origin,
                 base::Unretained(data),
                 base::Unretained(resources)),
      base::Bind(&ServiceWorkerStorage::DidReadRegistrationForId,
                 weak_factory_.GetWeakPtr(),
                 callback, base::Owned(data), base::Owned(resources)));
}

void ServiceWorkerStorage::GetAllRegistrations(
    const GetAllRegistrationInfosCallback& callback) {
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::GetAllRegistrations,
          weak_factory_.GetWeakPtr(), callback))) {
    if (state_ != INITIALIZING || !context_) {
      RunSoon(FROM_HERE, base::Bind(
          callback, std::vector<ServiceWorkerRegistrationInfo>()));
    }
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  RegistrationList* registrations = new RegistrationList;
  PostTaskAndReplyWithResult(
      database_task_runner_,
      FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::GetAllRegistrations,
                 base::Unretained(database_.get()),
                 base::Unretained(registrations)),
      base::Bind(&ServiceWorkerStorage::DidGetAllRegistrations,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(registrations)));
}

void ServiceWorkerStorage::StoreRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version,
    const StatusCallback& callback) {
  DCHECK(registration);
  DCHECK(version);

  DCHECK(state_ == INITIALIZED || state_ == DISABLED);
  if (state_ != INITIALIZED || !context_) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }

  ServiceWorkerDatabase::RegistrationData data;
  data.registration_id = registration->id();
  data.scope = registration->pattern();
  data.script = registration->script_url();
  data.has_fetch_handler = true;
  data.version_id = version->version_id();
  data.last_update_check = base::Time::Now();
  data.is_active = false;  // initially stored in the waiting state

  ResourceList resources;
  PostTaskAndReplyWithResult(
      database_task_runner_,
      FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::WriteRegistration,
                 base::Unretained(database_.get()), data, resources),
      base::Bind(&ServiceWorkerStorage::DidStoreRegistration,
                 weak_factory_.GetWeakPtr(),
                 registration->script_url().GetOrigin(),
                 callback));
}

void ServiceWorkerStorage::UpdateToActiveState(
    ServiceWorkerRegistration* registration,
    const StatusCallback& callback) {
  DCHECK(registration);

  DCHECK(state_ == INITIALIZED || state_ == DISABLED);
  if (state_ != INITIALIZED || !context_) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }

  PostTaskAndReplyWithResult(
      database_task_runner_,
      FROM_HERE,
      base::Bind(&ServiceWorkerDatabase::UpdateVersionToActive,
                 base::Unretained(database_.get()),
                 registration->id(),
                 registration->script_url().GetOrigin()),
      base::Bind(&ServiceWorkerStorage::DidUpdateToActiveState,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void ServiceWorkerStorage::DeleteRegistration(
    int64 registration_id,
    const GURL& origin,
    const StatusCallback& callback) {
  DCHECK(state_ == INITIALIZED || state_ == DISABLED);
  if (state_ != INITIALIZED || !context_) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }

  database_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DeleteRegistrationFromDB,
                 database_.get(),
                 base::MessageLoopProxy::current(),
                 registration_id, origin,
                 base::Bind(&ServiceWorkerStorage::DidDeleteRegistration,
                            weak_factory_.GetWeakPtr(), origin, callback)));

  // TODO(michaeln): Either its instance should also be
  // removed from liveregistrations map or the live object
  // should marked as deleted in some way and not 'findable'
  // thereafter.
}

scoped_ptr<ServiceWorkerResponseReader>
ServiceWorkerStorage::CreateResponseReader(int64 response_id) {
  return make_scoped_ptr(
      new ServiceWorkerResponseReader(response_id, disk_cache()));
}

scoped_ptr<ServiceWorkerResponseWriter>
ServiceWorkerStorage::CreateResponseWriter(int64 response_id) {
  return make_scoped_ptr(
      new ServiceWorkerResponseWriter(response_id, disk_cache()));
}

int64 ServiceWorkerStorage::NewRegistrationId() {
  if (state_ == DISABLED)
    return kInvalidServiceWorkerRegistrationId;
  DCHECK_EQ(INITIALIZED, state_);
  return next_registration_id_++;
}

int64 ServiceWorkerStorage::NewVersionId() {
  if (state_ == DISABLED)
    return kInvalidServiceWorkerVersionId;
  DCHECK_EQ(INITIALIZED, state_);
  return next_version_id_++;
}

int64 ServiceWorkerStorage::NewResourceId() {
  if (state_ == DISABLED)
    return kInvalidServiceWorkerResourceId;
  DCHECK_EQ(INITIALIZED, state_);
  return next_resource_id_++;
}

void ServiceWorkerStorage::NotifyInstallingRegistration(
      ServiceWorkerRegistration* registration) {
  installing_registrations_[registration->id()] = registration;
}

void ServiceWorkerStorage::NotifyDoneInstallingRegistration(
      ServiceWorkerRegistration* registration) {
  installing_registrations_.erase(registration->id());
}

bool ServiceWorkerStorage::LazyInitialize(const base::Closure& callback) {
  if (!context_)
    return false;

  switch (state_) {
    case INITIALIZED:
      return true;
    case DISABLED:
      return false;
    case INITIALIZING:
      pending_tasks_.push_back(callback);
      return false;
    case UNINITIALIZED:
      pending_tasks_.push_back(callback);
      // Fall-through.
  }

  state_ = INITIALIZING;
  database_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ReadInitialDataFromDB,
                 database_.get(),
                 base::MessageLoopProxy::current(),
                 base::Bind(&ServiceWorkerStorage::DidReadInitialData,
                            weak_factory_.GetWeakPtr())));
  return false;
}

void ServiceWorkerStorage::DidReadInitialData(
    InitialData* data,
    ServiceWorkerDatabase::Status status) {
  DCHECK(data);
  DCHECK_EQ(INITIALIZING, state_);

  if (status == ServiceWorkerDatabase::STATUS_OK) {
    next_registration_id_ = data->next_registration_id;
    next_version_id_ = data->next_version_id;
    next_resource_id_ = data->next_resource_id;
    registered_origins_.swap(data->origins);
    state_ = INITIALIZED;
  } else {
    // TODO(nhiroki): If status==STATUS_ERROR_CORRUPTED, do corruption recovery
    // (http://crbug.com/371675).
    DLOG(WARNING) << "Failed to initialize: " << status;
    state_ = DISABLED;
  }

  for (std::vector<base::Closure>::const_iterator it = pending_tasks_.begin();
       it != pending_tasks_.end(); ++it) {
    RunSoon(FROM_HERE, *it);
  }
  pending_tasks_.clear();
}

void ServiceWorkerStorage::DidGetRegistrationsForPattern(
    const GURL& scope,
    const FindRegistrationCallback& callback,
    RegistrationList* registrations,
    ServiceWorkerDatabase::Status status) {
  DCHECK(registrations);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    // TODO(nhiroki): Handle database error (http://crbug.com/371675).
    callback.Run(SERVICE_WORKER_ERROR_FAILED,
                 scoped_refptr<ServiceWorkerRegistration>());
    return;
  }

  // Find one with a matching scope.
  for (RegistrationList::const_iterator it = registrations->begin();
       it != registrations->end(); ++it) {
    if (scope == it->scope) {
      scoped_refptr<ServiceWorkerRegistration> registration =
          context_->GetLiveRegistration(it->registration_id);
      if (!registration)
        registration = CreateRegistration(*it);
      callback.Run(SERVICE_WORKER_OK, registration);
      return;
    }
  }

  // Look for something currently being installed.
  scoped_refptr<ServiceWorkerRegistration> installing_registration =
      FindInstallingRegistrationForPattern(scope);
  if (installing_registration) {
    callback.Run(SERVICE_WORKER_OK, installing_registration);
    return;
  }

  callback.Run(SERVICE_WORKER_ERROR_NOT_FOUND,
               scoped_refptr<ServiceWorkerRegistration>());
}

void ServiceWorkerStorage::DidGetRegistrationsForDocument(
    const GURL& document_url,
    const FindRegistrationCallback& callback,
    RegistrationList* registrations,
    ServiceWorkerDatabase::Status status) {
  DCHECK(registrations);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    // TODO(nhiroki): Handle database error (http://crbug.com/371675).
    callback.Run(SERVICE_WORKER_ERROR_FAILED,
                 scoped_refptr<ServiceWorkerRegistration>());
    return;
  }

  // Find one with a pattern match.
  for (RegistrationList::const_iterator it = registrations->begin();
       it != registrations->end(); ++it) {
    // TODO(michaeln): if there are multiple matches the one with
    // the longest scope should win.
    if (ServiceWorkerUtils::ScopeMatches(it->scope, document_url)) {
      scoped_refptr<ServiceWorkerRegistration> registration =
          context_->GetLiveRegistration(it->registration_id);
      if (registration) {
        callback.Run(SERVICE_WORKER_OK, registration);
        return;
      }
      callback.Run(SERVICE_WORKER_OK, CreateRegistration(*it));
      return;
    }
  }

  // Look for something currently being installed.
  // TODO(michaeln): Should be mixed in with the stored registrations
  // for this test.
  scoped_refptr<ServiceWorkerRegistration> installing_registration =
      FindInstallingRegistrationForDocument(document_url);
  if (installing_registration) {
    callback.Run(SERVICE_WORKER_OK, installing_registration);
    return;
  }

  callback.Run(SERVICE_WORKER_ERROR_NOT_FOUND,
               scoped_refptr<ServiceWorkerRegistration>());
}

void ServiceWorkerStorage::DidReadRegistrationForId(
    const FindRegistrationCallback& callback,
    ServiceWorkerDatabase::RegistrationData* registration,
    ResourceList* resources,
    ServiceWorkerDatabase::Status status) {
  DCHECK(registration);
  DCHECK(resources);

  if (status == ServiceWorkerDatabase::STATUS_OK) {
    callback.Run(SERVICE_WORKER_OK, CreateRegistration(*registration));
    return;
  }

  if (status == ServiceWorkerDatabase::STATUS_ERROR_NOT_FOUND) {
    // Look for somthing currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForId(registration->registration_id);
    if (installing_registration) {
      callback.Run(SERVICE_WORKER_OK, installing_registration);
      return;
    }
    callback.Run(SERVICE_WORKER_ERROR_NOT_FOUND,
                 scoped_refptr<ServiceWorkerRegistration>());
    return;
  }

  // TODO(nhiroki): Handle database error (http://crbug.com/371675).
  callback.Run(DatabaseStatusToStatusCode(status),
               scoped_refptr<ServiceWorkerRegistration>());
  return;
}

void ServiceWorkerStorage::DidGetAllRegistrations(
    const GetAllRegistrationInfosCallback& callback,
    RegistrationList* registrations,
    ServiceWorkerDatabase::Status status) {
  DCHECK(registrations);
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    // TODO(nhiroki): Handle database error (http://crbug.com/371675).
    callback.Run(std::vector<ServiceWorkerRegistrationInfo>());
    return;
  }

  // Add all stored registrations.
  std::set<int64> pushed_registrations;
  std::vector<ServiceWorkerRegistrationInfo> infos;
  for (RegistrationList::const_iterator it = registrations->begin();
       it != registrations->end(); ++it) {
    DCHECK(pushed_registrations.insert(it->registration_id).second);
    ServiceWorkerRegistration* registration =
        context_->GetLiveRegistration(it->registration_id);
    if (registration) {
      infos.push_back(registration->GetInfo());
      continue;
    }
    ServiceWorkerRegistrationInfo info;
    info.pattern = it->scope;
    info.script_url = it->script;
    info.active_version.is_null = false;
    if (it->is_active)
      info.active_version.status = ServiceWorkerVersion::ACTIVE;
    else
      info.active_version.status = ServiceWorkerVersion::INSTALLED;
    info.active_version.version_id = it->version_id;
    infos.push_back(info);
  }

  // Add unstored registrations that are being installed.
  for (RegistrationRefsById::const_iterator it =
           installing_registrations_.begin();
       it != installing_registrations_.end(); ++it) {
    if (pushed_registrations.insert(it->first).second)
      infos.push_back(it->second->GetInfo());
  }

  callback.Run(infos);
}

void ServiceWorkerStorage::DidStoreRegistration(
    const GURL& origin,
    const StatusCallback& callback,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    // TODO(nhiroki): Handle database error (http://crbug.com/371675).
    callback.Run(DatabaseStatusToStatusCode(status));
    return;
  }
  registered_origins_.insert(origin);
  callback.Run(SERVICE_WORKER_OK);
}

void ServiceWorkerStorage::DidUpdateToActiveState(
    const StatusCallback& callback,
    ServiceWorkerDatabase::Status status) {
  // TODO(nhiroki): Handle database error (http://crbug.com/371675).
  callback.Run(DatabaseStatusToStatusCode(status));
}

void ServiceWorkerStorage::DidDeleteRegistration(
    const GURL& origin,
    const StatusCallback& callback,
    bool origin_is_deletable,
    ServiceWorkerDatabase::Status status) {
  if (status != ServiceWorkerDatabase::STATUS_OK) {
    // TODO(nhiroki): Handle database error (http://crbug.com/371675).
    callback.Run(DatabaseStatusToStatusCode(status));
    return;
  }
  if (origin_is_deletable)
    registered_origins_.erase(origin);
  callback.Run(SERVICE_WORKER_OK);
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerStorage::CreateRegistration(
    const ServiceWorkerDatabase::RegistrationData& data) {
  scoped_refptr<ServiceWorkerRegistration> registration(
      new ServiceWorkerRegistration(
          data.scope, data.script, data.registration_id, context_));

  scoped_refptr<ServiceWorkerVersion> version =
      context_->GetLiveVersion(data.version_id);
  if (!version) {
    version = new ServiceWorkerVersion(registration, data.version_id, context_);
    version->SetStatus(data.is_active ?
        ServiceWorkerVersion::ACTIVE : ServiceWorkerVersion::INSTALLED);
  }

  if (version->status() == ServiceWorkerVersion::ACTIVE)
    registration->set_active_version(version);
  else if (version->status() == ServiceWorkerVersion::INSTALLED)
    registration->set_pending_version(version);
  else
    NOTREACHED();
  // TODO(michaeln): Hmmm, what if DeleteReg was invoked after
  // the Find result we're returning here? NOTREACHED condition?

  return registration;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForDocument(
    const GURL& document_url) {
  // TODO(michaeln): if there are multiple matches the one with
  // the longest scope should win, and these should on equal footing
  // with the stored registrations in FindRegistrationForDocument().
  for (RegistrationRefsById::const_iterator it =
           installing_registrations_.begin();
       it != installing_registrations_.end(); ++it) {
    if (ServiceWorkerUtils::ScopeMatches(
            it->second->pattern(), document_url)) {
      return it->second;
    }
  }
  return NULL;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForPattern(
    const GURL& scope) {
  for (RegistrationRefsById::const_iterator it =
           installing_registrations_.begin();
       it != installing_registrations_.end(); ++it) {
    if (it->second->pattern() == scope)
      return it->second;
  }
  return NULL;
}

ServiceWorkerRegistration*
ServiceWorkerStorage::FindInstallingRegistrationForId(
    int64 registration_id) {
  RegistrationRefsById::const_iterator found =
      installing_registrations_.find(registration_id);
  if (found == installing_registrations_.end())
    return NULL;
  return found->second;
}

ServiceWorkerDiskCache* ServiceWorkerStorage::disk_cache() {
  if (disk_cache_)
    return disk_cache_.get();

  // TODO(michaeln): Store data on disk and do error checking.
  disk_cache_.reset(new ServiceWorkerDiskCache);
  int rv = disk_cache_->InitWithMemBackend(
      kMaxMemDiskCacheSize,
      base::Bind(&EmptyCompletionCallback));
  DCHECK_EQ(net::OK, rv);
  return disk_cache_.get();
}

}  // namespace content
