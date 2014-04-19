// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage.h"

#include <string>
#include "base/message_loop/message_loop.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_info.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/quota/quota_manager_proxy.h"

namespace content {

namespace {

void RunSoon(const base::Closure& closure) {
  base::MessageLoop::current()->PostTask(FROM_HERE, closure);
}

const base::FilePath::CharType kServiceWorkerDirectory[] =
    FILE_PATH_LITERAL("ServiceWorker");

}  // namespace

ServiceWorkerStorage::ServiceWorkerStorage(
    const base::FilePath& path,
    base::WeakPtr<ServiceWorkerContextCore> context,
    quota::QuotaManagerProxy* quota_manager_proxy)
    : last_registration_id_(0),
      last_version_id_(0),
      last_resource_id_(0),
      simulated_lazy_initted_(false),
      context_(context),
      quota_manager_proxy_(quota_manager_proxy) {
  if (!path.empty())
    path_ = path.Append(kServiceWorkerDirectory);
}

ServiceWorkerStorage::~ServiceWorkerStorage() {
}

void ServiceWorkerStorage::FindRegistrationForPattern(
    const GURL& scope,
    const FindRegistrationCallback& callback) {
  simulated_lazy_initted_ = true;
  scoped_refptr<ServiceWorkerRegistration> null_registration;
  if (!context_) {
    RunSoon(base::Bind(
        callback, SERVICE_WORKER_ERROR_FAILED,
        null_registration));
    return;
  }

  // See if there are any registrations for the origin.
  OriginRegistrationsMap::const_iterator
      found = stored_registrations_.find(scope.GetOrigin());
  if (found == stored_registrations_.end()) {
    RunSoon(base::Bind(
        callback, SERVICE_WORKER_ERROR_NOT_FOUND,
        null_registration));
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
        RunSoon(base::Bind(
            callback, SERVICE_WORKER_OK,
            registration));
        return;
      }

      registration = CreateRegistration(data);
      RunSoon(base::Bind(callback, SERVICE_WORKER_OK, registration));
      return;
    }
  }

  RunSoon(base::Bind(
      callback, SERVICE_WORKER_ERROR_NOT_FOUND,
      null_registration));
}

void ServiceWorkerStorage::FindRegistrationForDocument(
    const GURL& document_url,
    const FindRegistrationCallback& callback) {
  simulated_lazy_initted_ = true;
  scoped_refptr<ServiceWorkerRegistration> null_registration;
  if (!context_) {
    callback.Run(SERVICE_WORKER_ERROR_FAILED, null_registration);
    return;
  }

  // See if there are any registrations for the origin.
  OriginRegistrationsMap::const_iterator
      found = stored_registrations_.find(document_url.GetOrigin());
  if (found == stored_registrations_.end()) {
    // Return syncly to simulate this class having an in memory map of
    // origins with registrations.
    callback.Run(SERVICE_WORKER_ERROR_NOT_FOUND, null_registration);
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
        callback.Run(SERVICE_WORKER_OK, registration);
        return;
      }

      // If we have to create a new instance, return it asyncly to simulate
      // having had to retreive the RegistrationData from the db.
      registration = CreateRegistration(data);
      RunSoon(base::Bind(callback, SERVICE_WORKER_OK, registration));
      return;
    }
  }

  // Return asyncly to simulate having had to look in the db since this
  // origin does have some registations.
  RunSoon(base::Bind(
      callback, SERVICE_WORKER_ERROR_NOT_FOUND,
      null_registration));
}

void ServiceWorkerStorage::FindRegistrationForId(
    int64 registration_id,
    const FindRegistrationCallback& callback) {
  simulated_lazy_initted_ = true;
  scoped_refptr<ServiceWorkerRegistration> null_registration;
  if (!context_) {
    callback.Run(SERVICE_WORKER_ERROR_FAILED, null_registration);
    return;
  }
  RegistrationPtrMap::const_iterator found =
      registrations_by_id_.find(registration_id);
  if (found == registrations_by_id_.end()) {
    callback.Run(SERVICE_WORKER_ERROR_NOT_FOUND, null_registration);
    return;
  }
  scoped_refptr<ServiceWorkerRegistration> registration =
      context_->GetLiveRegistration(registration_id);
  if (registration) {
    callback.Run(SERVICE_WORKER_OK, registration);
    return;
  }
  registration = CreateRegistration(found->second);
  RunSoon(base::Bind(callback, SERVICE_WORKER_OK, registration));
}

void ServiceWorkerStorage::GetAllRegistrations(
    const GetAllRegistrationInfosCallback& callback) {
  simulated_lazy_initted_ = true;
  std::vector<ServiceWorkerRegistrationInfo> registrations;
  if (!context_) {
    RunSoon(base::Bind(callback, registrations));
    return;
  }

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

  RunSoon(base::Bind(callback, registrations));
}

void ServiceWorkerStorage::StoreRegistration(
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version,
    const StatusCallback& callback) {
  DCHECK(registration);
  DCHECK(version);
  DCHECK(simulated_lazy_initted_);
  if (!context_) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
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

  RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
}

 void ServiceWorkerStorage::UpdateToActiveState(
      ServiceWorkerRegistration* registration,
      const StatusCallback& callback) {
  DCHECK(simulated_lazy_initted_);
  if (!context_) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_FAILED));
    return;
  }

  RegistrationPtrMap::const_iterator
       found = registrations_by_id_.find(registration->id());
  if (found == registrations_by_id_.end()) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_NOT_FOUND));
    return;
  }
  DCHECK(!found->second->is_active);
  found->second->is_active = true;
  RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
}

void ServiceWorkerStorage::DeleteRegistration(
    int64 registration_id,
    const StatusCallback& callback) {
  DCHECK(simulated_lazy_initted_);
  RegistrationPtrMap::iterator
      found = registrations_by_id_.find(registration_id);
  if (found == registrations_by_id_.end()) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_NOT_FOUND));
    return;
  }

  GURL origin = found->second->script.GetOrigin();
  stored_registrations_[origin].erase(registration_id);
  if (stored_registrations_[origin].empty())
    stored_registrations_.erase(origin);

  registrations_by_id_.erase(found);

  RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
  // TODO(michaeln): Either its instance should also be
  // removed from liveregistrations map or the live object
  // should marked as deleted in some way and not 'findable'
  // thereafter.
}

int64 ServiceWorkerStorage::NewRegistrationId() {
  DCHECK(simulated_lazy_initted_);
  return ++last_registration_id_;
}

int64 ServiceWorkerStorage::NewVersionId() {
  DCHECK(simulated_lazy_initted_);
  return ++last_version_id_;
}

int64 ServiceWorkerStorage::NewResourceId() {
  DCHECK(simulated_lazy_initted_);
  return ++last_resource_id_;
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

}  // namespace content
