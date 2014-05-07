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

typedef base::Callback<void(ServiceWorkerStorage::InitialData* data,
                            bool success)> InitializeCallback;

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
    FILE_PATH_LITERAL("ServiceWorker");

const int kMaxMemDiskCacheSize = 10 * 1024 * 1024;

void EmptyCompletionCallback(int) {}

void InitializeOnDatabaseTaskRunner(
    ServiceWorkerDatabase* database,
    scoped_refptr<base::SequencedTaskRunner> original_task_runner,
    const InitializeCallback& callback) {
  DCHECK(database);
  ServiceWorkerStorage::InitialData* data =
      new ServiceWorkerStorage::InitialData();
  bool success =
      database->GetNextAvailableIds(&data->next_registration_id,
                                    &data->next_version_id,
                                    &data->next_resource_id) &&
      database->GetOriginsWithRegistrations(&data->origins);
  original_task_runner->PostTask(
      FROM_HERE, base::Bind(callback, base::Owned(data), success));
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
    quota::QuotaManagerProxy* quota_manager_proxy)
    : next_registration_id_(kInvalidServiceWorkerRegistrationId),
      next_version_id_(kInvalidServiceWorkerVersionId),
      next_resource_id_(kInvalidServiceWorkerResourceId),
      state_(UNINITIALIZED),
      context_(context),
      database_task_runner_(database_task_runner),
      quota_manager_proxy_(quota_manager_proxy),
      weak_factory_(this) {
  if (!path.empty())
    path_ = path.Append(kServiceWorkerDirectory);

  // TODO(nhiroki): Create a database on-disk after the database schema gets
  // stable.
  database_.reset(new ServiceWorkerDatabase(base::FilePath()));
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

  scoped_refptr<ServiceWorkerRegistration> installing_registration =
      FindInstallingRegistrationForPattern(scope);
  if (installing_registration) {
    CompleteFindSoon(
        FROM_HERE, installing_registration, SERVICE_WORKER_OK, callback);
    return;
  }

  // See if there are any registrations for the origin.
  OriginRegistrationsMap::const_iterator
      found = stored_registrations_.find(scope.GetOrigin());
  if (found == stored_registrations_.end()) {
    CompleteFindSoon(
        FROM_HERE, null_registration, SERVICE_WORKER_ERROR_NOT_FOUND, callback);
    return;
  }

  // Find one with a matching scope.
  for (RegistrationsMap::const_iterator it = found->second.begin();
       it != found->second.end(); ++it) {
    if (scope == it->second.scope) {
      const ServiceWorkerDatabase::RegistrationData* data = &(it->second);
      scoped_refptr<ServiceWorkerRegistration> registration =
          context_->GetLiveRegistration(data->registration_id);
      if (registration) {
        CompleteFindSoon(FROM_HERE, registration, SERVICE_WORKER_OK, callback);
        return;
      }

      registration = CreateRegistration(data);
      CompleteFindSoon(FROM_HERE, registration, SERVICE_WORKER_OK, callback);
      return;
    }
  }

  CompleteFindSoon(
      FROM_HERE, null_registration, SERVICE_WORKER_ERROR_NOT_FOUND, callback);
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

  // See if there are any registrations for the origin.
  OriginRegistrationsMap::const_iterator
      found = stored_registrations_.find(document_url.GetOrigin());
  if (found == stored_registrations_.end()) {
    // Look for something currently being installed.
    scoped_refptr<ServiceWorkerRegistration> installing_registration =
        FindInstallingRegistrationForDocument(document_url);
    if (installing_registration) {
      CompleteFindNow(installing_registration, SERVICE_WORKER_OK, callback);
      return;
    }

    // Return syncly to simulate this class having an in memory map of
    // origins with registrations.
    CompleteFindNow(null_registration, SERVICE_WORKER_ERROR_NOT_FOUND,
                    callback);
    return;
  }

  // Find one with a pattern match.
  for (RegistrationsMap::const_iterator it = found->second.begin();
       it != found->second.end(); ++it) {
    // TODO(michaeln): if there are multiple matches the one with
    // the longest scope should win.
    if (ServiceWorkerUtils::ScopeMatches(it->second.scope, document_url)) {
      const ServiceWorkerDatabase::RegistrationData* data = &(it->second);

      // If its in the live map, return syncly to simulate this class having
      // iterated over the values in that map instead of reading the db.
      scoped_refptr<ServiceWorkerRegistration> registration =
          context_->GetLiveRegistration(data->registration_id);
      if (registration) {
        CompleteFindNow(registration, SERVICE_WORKER_OK, callback);
        return;
      }

      // If we have to create a new instance, return it asyncly to simulate
      // having had to retreive the RegistrationData from the db.
      registration = CreateRegistration(data);
      CompleteFindSoon(FROM_HERE, registration, SERVICE_WORKER_OK, callback);
      return;
    }
  }

  // Look for something currently being installed.
  // TODO(michaeln): Should be mixed in with the stored registrations
  // for this test.
  scoped_refptr<ServiceWorkerRegistration> installing_registration =
      FindInstallingRegistrationForDocument(document_url);
  if (installing_registration) {
    CompleteFindSoon(
        FROM_HERE, installing_registration, SERVICE_WORKER_OK, callback);
    return;
  }

  // Return asyncly to simulate having had to look in the db since this
  // origin does have some registations.
  CompleteFindSoon(
      FROM_HERE, null_registration, SERVICE_WORKER_ERROR_NOT_FOUND, callback);
}

void ServiceWorkerStorage::FindRegistrationForId(
    int64 registration_id,
    const FindRegistrationCallback& callback) {
  scoped_refptr<ServiceWorkerRegistration> null_registration;
  if (!LazyInitialize(base::Bind(
          &ServiceWorkerStorage::FindRegistrationForId,
          weak_factory_.GetWeakPtr(), registration_id, callback))) {
    if (state_ != INITIALIZING || !context_)
      CompleteFindNow(null_registration, SERVICE_WORKER_ERROR_FAILED, callback);
    return;
  }
  DCHECK_EQ(INITIALIZED, state_);

  scoped_refptr<ServiceWorkerRegistration> installing_registration =
      FindInstallingRegistrationForId(registration_id);
  if (installing_registration) {
    CompleteFindNow(installing_registration, SERVICE_WORKER_OK, callback);
    return;
  }
  RegistrationPtrMap::const_iterator found =
      registrations_by_id_.find(registration_id);
  if (found == registrations_by_id_.end()) {
    CompleteFindNow(null_registration, SERVICE_WORKER_ERROR_NOT_FOUND,
                    callback);
    return;
  }
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    CompleteFindNow(registration, SERVICE_WORKER_OK, callback);
    return;
  }
  registration = CreateRegistration(found->second);
  CompleteFindSoon(FROM_HERE, registration, SERVICE_WORKER_OK, callback);
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

  // Add all stored registrations.
  std::vector<ServiceWorkerRegistrationInfo> registrations;
  for (RegistrationPtrMap::const_iterator it = registrations_by_id_.begin();
       it != registrations_by_id_.end(); ++it) {
    ServiceWorkerRegistration* registration =
        context_->GetLiveRegistration(it->first);
    if (registration) {
      registrations.push_back(registration->GetInfo());
      continue;
    }
    ServiceWorkerRegistrationInfo info;
    info.pattern = it->second->scope;
    info.script_url = it->second->script;
    info.active_version.is_null = false;
    if (it->second->is_active)
      info.active_version.status = ServiceWorkerVersion::ACTIVE;
    else
      info.active_version.status = ServiceWorkerVersion::INSTALLED;
    registrations.push_back(info);
  }

  // Add unstored registrations that are being installed.
  for (RegistrationRefsById::const_iterator it =
           installing_registrations_.begin();
       it != installing_registrations_.end(); ++it) {
    if (registrations_by_id_.find(it->first) == registrations_by_id_.end())
      registrations.push_back(it->second->GetInfo());
  }

  RunSoon(FROM_HERE, base::Bind(callback, registrations));
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

  // Keep a database struct in the storage map.
  RegistrationsMap& storage_map =
      stored_registrations_[registration->script_url().GetOrigin()];
  ServiceWorkerDatabase::RegistrationData& data =
      storage_map[registration->id()];
  data.registration_id = registration->id();
  data.scope = registration->pattern();
  data.script = registration->script_url();
  data.has_fetch_handler = true;
  data.version_id = version->version_id();
  data.last_update_check = base::Time::Now();
  data.is_active = false;  // initially stored in the waiting state

  // Keep a seperate map of ptrs keyed by id only.
  registrations_by_id_[registration->id()] = &storage_map[registration->id()];

  RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_OK));
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

  RegistrationPtrMap::const_iterator
       found = registrations_by_id_.find(registration->id());
  if (found == registrations_by_id_.end()) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_NOT_FOUND));
    return;
  }
  DCHECK(!found->second->is_active);
  found->second->is_active = true;
  RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_OK));
}

void ServiceWorkerStorage::DeleteRegistration(
    int64 registration_id,
    const StatusCallback& callback) {
  DCHECK(state_ == INITIALIZED || state_ == DISABLED);
  if (state_ != INITIALIZED || !context_) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }

  RegistrationPtrMap::iterator
      found = registrations_by_id_.find(registration_id);
  if (found == registrations_by_id_.end()) {
    RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_ERROR_NOT_FOUND));
    return;
  }

  GURL origin = found->second->script.GetOrigin();
  stored_registrations_[origin].erase(registration_id);
  if (stored_registrations_[origin].empty())
    stored_registrations_.erase(origin);

  registrations_by_id_.erase(found);

  RunSoon(FROM_HERE, base::Bind(callback, SERVICE_WORKER_OK));
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
      base::Bind(&InitializeOnDatabaseTaskRunner,
                 database_.get(),
                 base::MessageLoopProxy::current(),
                 base::Bind(&ServiceWorkerStorage::DidInitialize,
                            weak_factory_.GetWeakPtr())));
  return false;
}

void ServiceWorkerStorage::DidInitialize(
    InitialData* data,
    bool success) {
  DCHECK(data);
  DCHECK_EQ(INITIALIZING, state_);

  if (success) {
    next_registration_id_ = data->next_registration_id;
    next_version_id_ = data->next_version_id;
    next_resource_id_ = data->next_resource_id;
    registered_origins_.swap(data->origins);
    state_ = INITIALIZED;
  } else {
    DLOG(WARNING) << "Failed to initialize.";
    state_ = DISABLED;
  }

  for (std::vector<base::Closure>::const_iterator it = pending_tasks_.begin();
       it != pending_tasks_.end(); ++it) {
    RunSoon(FROM_HERE, *it);
  }
  pending_tasks_.clear();
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerStorage::CreateRegistration(
    const ServiceWorkerDatabase::RegistrationData* data) {
  scoped_refptr<ServiceWorkerRegistration> registration(
      new ServiceWorkerRegistration(
          data->scope, data->script, data->registration_id, context_));

  scoped_refptr<ServiceWorkerVersion> version =
      context_->GetLiveVersion(data->version_id);
  if (!version) {
    version = new ServiceWorkerVersion(
        registration, data->version_id, context_);
    version->SetStatus(data->GetVersionStatus());
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
