// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_manager.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "content/browser/background_sync/background_sync.pb.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/public/browser/browser_thread.h"

namespace {
const char kBackgroundSyncUserDataKey[] = "BackgroundSyncUserData";
}

namespace content {

const BackgroundSyncManager::BackgroundSyncRegistration::RegistrationId
    BackgroundSyncManager::BackgroundSyncRegistration::kInvalidRegistrationId =
        -1;

const BackgroundSyncManager::BackgroundSyncRegistration::RegistrationId
    BackgroundSyncManager::BackgroundSyncRegistrations::kInitialId = 0;

BackgroundSyncManager::BackgroundSyncRegistrations::
    BackgroundSyncRegistrations()
    : next_id(kInitialId) {
}
BackgroundSyncManager::BackgroundSyncRegistrations::BackgroundSyncRegistrations(
    BackgroundSyncRegistration::RegistrationId next_id)
    : next_id(next_id) {
}
BackgroundSyncManager::BackgroundSyncRegistrations::
    ~BackgroundSyncRegistrations() {
}

// static
scoped_ptr<BackgroundSyncManager> BackgroundSyncManager::Create(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context) {
  BackgroundSyncManager* sync_manager =
      new BackgroundSyncManager(service_worker_context);
  sync_manager->Init();
  return make_scoped_ptr(sync_manager);
}

BackgroundSyncManager::~BackgroundSyncManager() {
}

void BackgroundSyncManager::Register(
    const GURL& origin,
    int64 sw_registration_id,
    const BackgroundSyncRegistration& sync_registration,
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(BackgroundSyncRegistration::kInvalidRegistrationId,
            sync_registration.id);

  StatusAndRegistrationCallback pending_callback =
      base::Bind(&BackgroundSyncManager::PendingStatusAndRegistrationCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);

  op_scheduler_.ScheduleOperation(base::Bind(
      &BackgroundSyncManager::RegisterImpl, weak_ptr_factory_.GetWeakPtr(),
      origin, sw_registration_id, sync_registration, pending_callback));
}

void BackgroundSyncManager::Unregister(
    const GURL& origin,
    int64 sw_registration_id,
    const std::string& sync_registration_name,
    BackgroundSyncRegistration::RegistrationId sync_registration_id,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  StatusCallback pending_callback =
      base::Bind(&BackgroundSyncManager::PendingStatusCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);

  op_scheduler_.ScheduleOperation(base::Bind(
      &BackgroundSyncManager::UnregisterImpl, weak_ptr_factory_.GetWeakPtr(),
      origin, sw_registration_id, sync_registration_name, sync_registration_id,
      pending_callback));
}

void BackgroundSyncManager::GetRegistration(
    const GURL& origin,
    int64 sw_registration_id,
    const std::string sync_registration_name,
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  StatusAndRegistrationCallback pending_callback =
      base::Bind(&BackgroundSyncManager::PendingStatusAndRegistrationCallback,
                 weak_ptr_factory_.GetWeakPtr(), callback);

  op_scheduler_.ScheduleOperation(
      base::Bind(&BackgroundSyncManager::GetRegistrationImpl,
                 weak_ptr_factory_.GetWeakPtr(), origin, sw_registration_id,
                 sync_registration_name, pending_callback));
}

BackgroundSyncManager::BackgroundSyncManager(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : service_worker_context_(service_worker_context), weak_ptr_factory_(this) {
}

void BackgroundSyncManager::Init() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!op_scheduler_.ScheduledOperations());

  op_scheduler_.ScheduleOperation(base::Bind(&BackgroundSyncManager::InitImpl,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundSyncManager::InitImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetDataFromBackend(
      kBackgroundSyncUserDataKey,
      base::Bind(&BackgroundSyncManager::InitDidGetDataFromBackend,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundSyncManager::InitDidGetDataFromBackend(
    const std::vector<std::pair<int64, std::string>>& user_data,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK && status != SERVICE_WORKER_ERROR_NOT_FOUND)
    LOG(ERROR) << "Background Sync Failed to load from backend.";

  bool corruption_detected = false;
  for (const std::pair<int64, std::string>& data : user_data) {
    BackgroundSyncRegistrationsProto registrations_proto;
    if (registrations_proto.ParseFromString(data.second)) {
      sw_to_registrations_map_[data.first] = BackgroundSyncRegistrations(
          registrations_proto.next_registration_id());
      BackgroundSyncRegistrations* registrations =
          &sw_to_registrations_map_[data.first];

      for (int i = 0, max = registrations_proto.registration_size(); i < max;
           ++i) {
        const BackgroundSyncRegistrationProto& registration_proto =
            registrations_proto.registration(i);

        if (registration_proto.id() >= registrations->next_id) {
          corruption_detected = true;
          break;
        }

        BackgroundSyncRegistration registration(registration_proto.id(),
                                                registration_proto.name());
        if (registration_proto.has_min_period())
          registration.min_period = registration_proto.min_period();
        registrations->name_to_registration_map[registration_proto.name()] =
            registration;
      }
    }

    if (corruption_detected)
      break;
  }

  if (corruption_detected) {
    LOG(ERROR) << "Corruption detected in background sync backend";
    sw_to_registrations_map_.clear();
  }

  // TODO(jkarlin): Call the scheduling algorithm here.

  op_scheduler_.CompleteOperationAndRunNext();
}

void BackgroundSyncManager::RegisterImpl(
    const GURL& origin,
    int64 sw_registration_id,
    const BackgroundSyncRegistration& sync_registration,
    const StatusAndRegistrationCallback& callback) {
  BackgroundSyncRegistration existing_registration;
  if (LookupRegistration(sw_registration_id, sync_registration.name,
                         &existing_registration)) {
    if (existing_registration.Equals(sync_registration)) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(callback, ERROR_TYPE_OK, existing_registration));
      return;
    }
  }

  BackgroundSyncRegistration new_registration = sync_registration;
  BackgroundSyncRegistrations* registrations =
      &sw_to_registrations_map_[sw_registration_id];
  new_registration.id = registrations->next_id++;

  AddRegistrationToMap(sw_registration_id, new_registration);

  StoreRegistrations(
      origin, sw_registration_id,
      base::Bind(&BackgroundSyncManager::RegisterDidStore,
                 weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                 new_registration, existing_registration, callback));
}

bool BackgroundSyncManager::LookupRegistration(
    int64 sw_registration_id,
    const std::string& sync_registration_name,
    BackgroundSyncRegistration* existing_registration) {
  SWIdToRegistrationsMap::iterator it =
      sw_to_registrations_map_.find(sw_registration_id);
  if (it == sw_to_registrations_map_.end())
    return false;

  const BackgroundSyncRegistrations& registrations = it->second;
  const auto name_and_registration_iter =
      registrations.name_to_registration_map.find(sync_registration_name);
  if (name_and_registration_iter ==
      registrations.name_to_registration_map.end())
    return false;

  if (existing_registration)
    *existing_registration = name_and_registration_iter->second;

  return true;
}

void BackgroundSyncManager::StoreRegistrations(
    const GURL& origin,
    int64 sw_registration_id,
    const ServiceWorkerStorage::StatusCallback& callback) {
  // Serialize the data.
  const BackgroundSyncRegistrations& registrations =
      sw_to_registrations_map_[sw_registration_id];
  BackgroundSyncRegistrationsProto registrations_proto;
  registrations_proto.set_next_registration_id(registrations.next_id);

  for (const auto& name_and_registration :
       registrations.name_to_registration_map) {
    const BackgroundSyncRegistration& registration =
        name_and_registration.second;
    BackgroundSyncRegistrationProto* registration_proto =
        registrations_proto.add_registration();
    registration_proto->set_id(registration.id);
    registration_proto->set_name(registration.name);
    if (registration.min_period != 0)
      registration_proto->set_min_period(registration.min_period);
  }
  std::string serialized;
  bool success = registrations_proto.SerializeToString(&serialized);
  DCHECK(success);

  StoreDataInBackend(sw_registration_id, origin, kBackgroundSyncUserDataKey,
                     serialized, callback);
}

void BackgroundSyncManager::RegisterDidStore(
    int64 sw_registration_id,
    const BackgroundSyncRegistration& new_registration,
    const BackgroundSyncRegistration& previous_registration,
    const StatusAndRegistrationCallback& callback,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    // Restore the previous state.
    if (previous_registration.id !=
        BackgroundSyncRegistration::kInvalidRegistrationId) {
      AddRegistrationToMap(sw_registration_id, previous_registration);
    } else {
      RemoveRegistrationFromMap(sw_registration_id, new_registration.name,
                                nullptr);
    }
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, ERROR_TYPE_STORAGE, BackgroundSyncRegistration()));
    return;
  }

  // TODO(jkarlin): Run the registration algorithm.
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, ERROR_TYPE_OK, new_registration));
}

void BackgroundSyncManager::RemoveRegistrationFromMap(
    int64 sw_registration_id,
    const std::string& sync_registration_name,
    BackgroundSyncRegistration* old_registration) {
  DCHECK(
      LookupRegistration(sw_registration_id, sync_registration_name, nullptr));

  BackgroundSyncRegistrations* registrations =
      &sw_to_registrations_map_[sw_registration_id];

  const auto name_and_registration_iter =
      registrations->name_to_registration_map.find(sync_registration_name);
  if (old_registration)
    *old_registration = name_and_registration_iter->second;

  registrations->name_to_registration_map.erase(name_and_registration_iter);
}

void BackgroundSyncManager::AddRegistrationToMap(
    int64 sw_registration_id,
    const BackgroundSyncRegistration& sync_registration) {
  DCHECK_NE(BackgroundSyncRegistration::kInvalidRegistrationId,
            sw_registration_id);

  BackgroundSyncRegistrations* registrations =
      &sw_to_registrations_map_[sw_registration_id];
  registrations->name_to_registration_map[sync_registration.name] =
      sync_registration;
}

void BackgroundSyncManager::StoreDataInBackend(
    int64 sw_registration_id,
    const GURL& origin,
    const std::string& key,
    const std::string& data,
    const ServiceWorkerStorage::StatusCallback& callback) {
  service_worker_context_->context()->storage()->StoreUserData(
      sw_registration_id, origin, key, data, callback);
}

void BackgroundSyncManager::GetDataFromBackend(
    const std::string& key,
    const ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback&
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->context()->storage()->GetUserDataForAllRegistrations(
      key, callback);
}

void BackgroundSyncManager::UnregisterImpl(
    const GURL& origin,
    int64 sw_registration_id,
    const std::string& sync_registration_name,
    BackgroundSyncRegistration::RegistrationId sync_registration_id,
    const StatusCallback& callback) {
  BackgroundSyncRegistration existing_registration;
  if (!LookupRegistration(sw_registration_id, sync_registration_name,
                          &existing_registration) ||
      existing_registration.id != sync_registration_id) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, ERROR_TYPE_NOT_FOUND));
    return;
  }

  BackgroundSyncRegistration old_sync_registration;
  RemoveRegistrationFromMap(sw_registration_id, sync_registration_name,
                            &old_sync_registration);

  StoreRegistrations(
      origin, sw_registration_id,
      base::Bind(&BackgroundSyncManager::UnregisterDidStore,
                 weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                 old_sync_registration, callback));
}

void BackgroundSyncManager::UnregisterDidStore(
    int64 sw_registration_id,
    const BackgroundSyncRegistration& old_sync_registration,
    const StatusCallback& callback,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    // Restore the previous state.
    AddRegistrationToMap(sw_registration_id, old_sync_registration);
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, ERROR_TYPE_STORAGE));
    return;
  }

  // TODO(jkarlin): Run the registration algorithm.
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, ERROR_TYPE_OK));
}

void BackgroundSyncManager::GetRegistrationImpl(
    const GURL& origin,
    int64 sw_registration_id,
    const std::string sync_registration_name,
    const StatusAndRegistrationCallback& callback) {
  BackgroundSyncRegistration out_registration;
  if (!LookupRegistration(sw_registration_id, sync_registration_name,
                          &out_registration)) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, ERROR_TYPE_NOT_FOUND,
                              BackgroundSyncRegistration()));
    return;
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, ERROR_TYPE_OK, out_registration));
}

void BackgroundSyncManager::PendingStatusAndRegistrationCallback(
    const StatusAndRegistrationCallback& callback,
    ErrorType error,
    const BackgroundSyncRegistration& sync_registration) {
  // The callback might delete this object, so hang onto a weak ptr to find out.
  base::WeakPtr<BackgroundSyncManager> manager = weak_ptr_factory_.GetWeakPtr();
  callback.Run(error, sync_registration);
  if (manager)
    op_scheduler_.CompleteOperationAndRunNext();
}

void BackgroundSyncManager::PendingStatusCallback(
    const StatusCallback& callback,
    ErrorType error) {
  // The callback might delete this object, so hang onto a weak ptr to find out.
  base::WeakPtr<BackgroundSyncManager> manager = weak_ptr_factory_.GetWeakPtr();
  callback.Run(error);
  if (manager)
    op_scheduler_.CompleteOperationAndRunNext();
}

}  // namespace content
