// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_manager.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/background_sync/background_sync_metrics.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/background_sync/background_sync_power_observer.h"
#include "content/browser/background_sync/background_sync_registration_options.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_ANDROID)
#include "content/browser/android/background_sync_launcher_android.h"
#endif

namespace {
const char kBackgroundSyncUserDataKey[] = "BackgroundSyncUserData";
}

namespace content {

BackgroundSyncManager::BackgroundSyncRegistrations::
    BackgroundSyncRegistrations()
    : next_id(BackgroundSyncRegistration::kInitialId) {
}

BackgroundSyncManager::BackgroundSyncRegistrations::
    ~BackgroundSyncRegistrations() {
}

// static
scoped_ptr<BackgroundSyncManager> BackgroundSyncManager::Create(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncManager* sync_manager =
      new BackgroundSyncManager(service_worker_context);
  sync_manager->Init();
  return make_scoped_ptr(sync_manager);
}

BackgroundSyncManager::~BackgroundSyncManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->RemoveObserver(this);
}

BackgroundSyncManager::RegistrationKey::RegistrationKey(
    const BackgroundSyncRegistration& registration)
    : RegistrationKey(registration.options()->tag,
                      registration.options()->periodicity) {
}

BackgroundSyncManager::RegistrationKey::RegistrationKey(
    const BackgroundSyncRegistrationOptions& options)
    : RegistrationKey(options.tag, options.periodicity) {
}

BackgroundSyncManager::RegistrationKey::RegistrationKey(
    const std::string& tag,
    SyncPeriodicity periodicity)
    : value_(periodicity == SYNC_ONE_SHOT ? "o_" + tag : "p_" + tag) {
}

void BackgroundSyncManager::Register(
    int64 sw_registration_id,
    const BackgroundSyncRegistrationOptions& options,
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // For UMA, determine here whether the sync could fire immediately
  BackgroundSyncMetrics::RegistrationCouldFire registration_could_fire =
      AreOptionConditionsMet(options)
          ? BackgroundSyncMetrics::REGISTRATION_COULD_FIRE
          : BackgroundSyncMetrics::REGISTRATION_COULD_NOT_FIRE;

  if (disabled_) {
    BackgroundSyncMetrics::CountRegister(
        options.periodicity, registration_could_fire,
        BackgroundSyncMetrics::REGISTRATION_IS_NOT_DUPLICATE,
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              BackgroundSyncRegistration()));
    return;
  }

  op_scheduler_.ScheduleOperation(
      base::Bind(&BackgroundSyncManager::RegisterImpl,
                 weak_ptr_factory_.GetWeakPtr(), sw_registration_id, options,
                 MakeStatusAndRegistrationCompletion(callback)));
}

void BackgroundSyncManager::Unregister(
    int64 sw_registration_id,
    const std::string& sync_registration_tag,
    SyncPeriodicity periodicity,
    BackgroundSyncRegistration::RegistrationId sync_registration_id,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    BackgroundSyncMetrics::CountUnregister(
        periodicity, BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR));
    return;
  }

  RegistrationKey registration_key(sync_registration_tag, periodicity);

  op_scheduler_.ScheduleOperation(base::Bind(
      &BackgroundSyncManager::UnregisterImpl, weak_ptr_factory_.GetWeakPtr(),
      sw_registration_id, registration_key, sync_registration_id, periodicity,
      MakeStatusCompletion(callback)));
}

void BackgroundSyncManager::GetRegistration(
    int64 sw_registration_id,
    const std::string& sync_registration_tag,
    SyncPeriodicity periodicity,
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              BackgroundSyncRegistration()));
    return;
  }

  RegistrationKey registration_key(sync_registration_tag, periodicity);

  op_scheduler_.ScheduleOperation(base::Bind(
      &BackgroundSyncManager::GetRegistrationImpl,
      weak_ptr_factory_.GetWeakPtr(), sw_registration_id, registration_key,
      MakeStatusAndRegistrationCompletion(callback)));
}

void BackgroundSyncManager::GetRegistrations(
    int64 sw_registration_id,
    SyncPeriodicity periodicity,
    const StatusAndRegistrationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              std::vector<BackgroundSyncRegistration>()));
    return;
  }

  op_scheduler_.ScheduleOperation(
      base::Bind(&BackgroundSyncManager::GetRegistrationsImpl,
                 weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                 periodicity, MakeStatusAndRegistrationsCompletion(callback)));
}

void BackgroundSyncManager::OnRegistrationDeleted(int64 registration_id,
                                                  const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Operations already in the queue will either fail when they write to storage
  // or return stale results based on registrations loaded in memory. This is
  // inconsequential since the service worker is gone.
  op_scheduler_.ScheduleOperation(base::Bind(
      &BackgroundSyncManager::OnRegistrationDeletedImpl,
      weak_ptr_factory_.GetWeakPtr(), registration_id, MakeEmptyCompletion()));
}

void BackgroundSyncManager::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Operations already in the queue will either fail when they write to storage
  // or return stale results based on registrations loaded in memory. This is
  // inconsequential since the service workers are gone.
  op_scheduler_.ScheduleOperation(
      base::Bind(&BackgroundSyncManager::OnStorageWipedImpl,
                 weak_ptr_factory_.GetWeakPtr(), MakeEmptyCompletion()));
}

BackgroundSyncManager::BackgroundSyncManager(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : service_worker_context_(service_worker_context),
      disabled_(false),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->AddObserver(this);

  network_observer_.reset(new BackgroundSyncNetworkObserver(
      base::Bind(&BackgroundSyncManager::OnNetworkChanged,
                 weak_ptr_factory_.GetWeakPtr())));
  power_observer_.reset(new BackgroundSyncPowerObserver(base::Bind(
      &BackgroundSyncManager::OnPowerChanged, weak_ptr_factory_.GetWeakPtr())));
}

void BackgroundSyncManager::Init() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!op_scheduler_.ScheduledOperations());
  DCHECK(!disabled_);

  op_scheduler_.ScheduleOperation(base::Bind(&BackgroundSyncManager::InitImpl,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             MakeEmptyCompletion()));
}

void BackgroundSyncManager::InitImpl(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback));
    return;
  }

  GetDataFromBackend(
      kBackgroundSyncUserDataKey,
      base::Bind(&BackgroundSyncManager::InitDidGetDataFromBackend,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncManager::InitDidGetDataFromBackend(
    const base::Closure& callback,
    const std::vector<std::pair<int64, std::string>>& user_data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != SERVICE_WORKER_OK && status != SERVICE_WORKER_ERROR_NOT_FOUND) {
    LOG(ERROR) << "BackgroundSync failed to init due to backend failure.";
    DisableAndClearManager(base::Bind(callback));
    return;
  }

  bool corruption_detected = false;
  for (const std::pair<int64, std::string>& data : user_data) {
    BackgroundSyncRegistrationsProto registrations_proto;
    if (registrations_proto.ParseFromString(data.second)) {
      BackgroundSyncRegistrations* registrations =
          &sw_to_registrations_map_[data.first];
      registrations->next_id = registrations_proto.next_registration_id();
      registrations->origin = GURL(registrations_proto.origin());

      for (int i = 0, max = registrations_proto.registration_size(); i < max;
           ++i) {
        const BackgroundSyncRegistrationProto& registration_proto =
            registrations_proto.registration(i);

        if (registration_proto.id() >= registrations->next_id) {
          corruption_detected = true;
          break;
        }

        RegistrationKey registration_key(registration_proto.tag(),
                                         registration_proto.periodicity());
        BackgroundSyncRegistration* registration =
            &registrations->registration_map[registration_key];

        BackgroundSyncRegistrationOptions* options = registration->options();
        options->tag = registration_proto.tag();
        options->periodicity = registration_proto.periodicity();
        options->min_period = registration_proto.min_period();
        options->network_state = registration_proto.network_state();
        options->power_state = registration_proto.power_state();

        registration->set_id(registration_proto.id());
        registration->set_sync_state(registration_proto.sync_state());

        if (registration->sync_state() == SYNC_STATE_FIRING) {
          // If the browser (or worker) closed while firing the event, consider
          // it pending again>
          registration->set_sync_state(SYNC_STATE_PENDING);
        }
      }
    }

    if (corruption_detected)
      break;
  }

  if (corruption_detected) {
    LOG(ERROR) << "Corruption detected in background sync backend";
    DisableAndClearManager(base::Bind(callback));
    return;
  }

  FireReadyEvents();

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback));
}

void BackgroundSyncManager::RegisterImpl(
    int64 sw_registration_id,
    const BackgroundSyncRegistrationOptions& options,
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // For UMA, determine here whether the sync could fire immediately
  BackgroundSyncMetrics::RegistrationCouldFire registration_could_fire =
      AreOptionConditionsMet(options)
          ? BackgroundSyncMetrics::REGISTRATION_COULD_FIRE
          : BackgroundSyncMetrics::REGISTRATION_COULD_NOT_FIRE;

  if (disabled_) {
    BackgroundSyncMetrics::CountRegister(
        options.periodicity, registration_could_fire,
        BackgroundSyncMetrics::REGISTRATION_IS_NOT_DUPLICATE,
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              BackgroundSyncRegistration()));
    return;
  }

  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    BackgroundSyncMetrics::CountRegister(
        options.periodicity, registration_could_fire,
        BackgroundSyncMetrics::REGISTRATION_IS_NOT_DUPLICATE,
        BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                   BackgroundSyncRegistration()));
    return;
  }

  if (!sw_registration->active_version()->HasWindowClients()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_NOT_ALLOWED,
                              BackgroundSyncRegistration()));
    return;
  }

  BackgroundSyncRegistration* existing_registration =
      LookupRegistration(sw_registration_id, RegistrationKey(options));
  if (existing_registration &&
      existing_registration->options()->Equals(options)) {
    if (existing_registration->sync_state() == SYNC_STATE_FAILED) {
      existing_registration->set_sync_state(SYNC_STATE_PENDING);
      StoreRegistrations(
          sw_registration_id,
          base::Bind(&BackgroundSyncManager::RegisterDidStore,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     *existing_registration, callback));
      return;
    }

    // Record the duplicated registration
    BackgroundSyncMetrics::CountRegister(
        existing_registration->options()->periodicity, registration_could_fire,
        BackgroundSyncMetrics::REGISTRATION_IS_DUPLICATE,
        BACKGROUND_SYNC_STATUS_OK);

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_OK,
                              *existing_registration));
    return;
  }

  BackgroundSyncRegistration new_registration;
  *new_registration.options() = options;

  BackgroundSyncRegistrations* registrations =
      &sw_to_registrations_map_[sw_registration_id];
  new_registration.set_id(registrations->next_id++);

  AddRegistrationToMap(sw_registration_id,
                       sw_registration->pattern().GetOrigin(),
                       new_registration);

  StoreRegistrations(
      sw_registration_id,
      base::Bind(&BackgroundSyncManager::RegisterDidStore,
                 weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                 new_registration, callback));
}

void BackgroundSyncManager::DisableAndClearManager(
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback));
    return;
  }

  disabled_ = true;
  sw_to_registrations_map_.clear();

  // Delete all backend entries. The memory representation of registered syncs
  // may be out of sync with storage (e.g., due to corruption detection on
  // loading from storage), so reload the registrations from storage again.
  GetDataFromBackend(
      kBackgroundSyncUserDataKey,
      base::Bind(&BackgroundSyncManager::DisableAndClearDidGetRegistrations,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncManager::DisableAndClearDidGetRegistrations(
    const base::Closure& callback,
    const std::vector<std::pair<int64, std::string>>& user_data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != SERVICE_WORKER_OK || user_data.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback));
    return;
  }

  base::Closure barrier_closure =
      base::BarrierClosure(user_data.size(), base::Bind(callback));

  for (const auto& sw_id_and_regs : user_data) {
    service_worker_context_->ClearRegistrationUserData(
        sw_id_and_regs.first, kBackgroundSyncUserDataKey,
        base::Bind(&BackgroundSyncManager::DisableAndClearManagerClearedOne,
                   weak_ptr_factory_.GetWeakPtr(), barrier_closure));
  }
}

void BackgroundSyncManager::DisableAndClearManagerClearedOne(
    const base::Closure& barrier_closure,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The status doesn't matter at this point, there is nothing else to be done.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(barrier_closure));
}

BackgroundSyncRegistration* BackgroundSyncManager::LookupRegistration(
    int64 sw_registration_id,
    const RegistrationKey& registration_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SWIdToRegistrationsMap::iterator it =
      sw_to_registrations_map_.find(sw_registration_id);
  if (it == sw_to_registrations_map_.end())
    return nullptr;

  BackgroundSyncRegistrations& registrations = it->second;
  DCHECK_LE(BackgroundSyncRegistration::kInitialId, registrations.next_id);
  DCHECK(!registrations.origin.is_empty());

  auto key_and_registration_iter =
      registrations.registration_map.find(registration_key);
  if (key_and_registration_iter == registrations.registration_map.end())
    return nullptr;

  return &key_and_registration_iter->second;
}

void BackgroundSyncManager::StoreRegistrations(
    int64 sw_registration_id,
    const ServiceWorkerStorage::StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Serialize the data.
  const BackgroundSyncRegistrations& registrations =
      sw_to_registrations_map_[sw_registration_id];
  BackgroundSyncRegistrationsProto registrations_proto;
  registrations_proto.set_next_registration_id(registrations.next_id);
  registrations_proto.set_origin(registrations.origin.spec());

  for (const auto& key_and_registration : registrations.registration_map) {
    const BackgroundSyncRegistration& registration =
        key_and_registration.second;
    BackgroundSyncRegistrationProto* registration_proto =
        registrations_proto.add_registration();
    registration_proto->set_id(registration.id());
    registration_proto->set_sync_state(registration.sync_state());
    registration_proto->set_tag(registration.options()->tag);
    registration_proto->set_periodicity(registration.options()->periodicity);
    registration_proto->set_min_period(registration.options()->min_period);
    registration_proto->set_network_state(
        registration.options()->network_state);
    registration_proto->set_power_state(registration.options()->power_state);
  }
  std::string serialized;
  bool success = registrations_proto.SerializeToString(&serialized);
  DCHECK(success);

  StoreDataInBackend(sw_registration_id, registrations.origin,
                     kBackgroundSyncUserDataKey, serialized, callback);
}

void BackgroundSyncManager::RegisterDidStore(
    int64 sw_registration_id,
    const BackgroundSyncRegistration& new_registration,
    const StatusAndRegistrationCallback& callback,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // For UMA, determine here whether the sync could fire immediately
  BackgroundSyncMetrics::RegistrationCouldFire registration_could_fire =
      AreOptionConditionsMet(*new_registration.options())
          ? BackgroundSyncMetrics::REGISTRATION_COULD_FIRE
          : BackgroundSyncMetrics::REGISTRATION_COULD_NOT_FIRE;

  if (status == SERVICE_WORKER_ERROR_NOT_FOUND) {
    // The service worker registration is gone.
    BackgroundSyncMetrics::CountRegister(
        new_registration.options()->periodicity, registration_could_fire,
        BackgroundSyncMetrics::REGISTRATION_IS_NOT_DUPLICATE,
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    sw_to_registrations_map_.erase(sw_registration_id);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              BackgroundSyncRegistration()));
    return;
  }

  if (status != SERVICE_WORKER_OK) {
    LOG(ERROR) << "BackgroundSync failed to store registration due to backend "
                  "failure.";
    BackgroundSyncMetrics::CountRegister(
        new_registration.options()->periodicity, registration_could_fire,
        BackgroundSyncMetrics::REGISTRATION_IS_NOT_DUPLICATE,
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    DisableAndClearManager(base::Bind(callback,
                                      BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                                      BackgroundSyncRegistration()));
    return;
  }

  BackgroundSyncMetrics::CountRegister(
      new_registration.options()->periodicity, registration_could_fire,
      BackgroundSyncMetrics::REGISTRATION_IS_NOT_DUPLICATE,
      BACKGROUND_SYNC_STATUS_OK);
  FireReadyEvents();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, BACKGROUND_SYNC_STATUS_OK, new_registration));
}

void BackgroundSyncManager::RemoveRegistrationFromMap(
    int64 sw_registration_id,
    const RegistrationKey& registration_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(LookupRegistration(sw_registration_id, registration_key));

  BackgroundSyncRegistrations* registrations =
      &sw_to_registrations_map_[sw_registration_id];

  registrations->registration_map.erase(registration_key);
}

void BackgroundSyncManager::AddRegistrationToMap(
    int64 sw_registration_id,
    const GURL& origin,
    const BackgroundSyncRegistration& sync_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sync_registration.IsValid());

  BackgroundSyncRegistrations* registrations =
      &sw_to_registrations_map_[sw_registration_id];
  registrations->origin = origin;

  RegistrationKey registration_key(sync_registration);
  registrations->registration_map[registration_key] = sync_registration;
}

void BackgroundSyncManager::StoreDataInBackend(
    int64 sw_registration_id,
    const GURL& origin,
    const std::string& backend_key,
    const std::string& data,
    const ServiceWorkerStorage::StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->StoreRegistrationUserData(
      sw_registration_id, origin, backend_key, data, callback);
}

void BackgroundSyncManager::GetDataFromBackend(
    const std::string& backend_key,
    const ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback&
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetUserDataForAllRegistrations(backend_key,
                                                          callback);
}

void BackgroundSyncManager::FireOneShotSync(
    const BackgroundSyncRegistration& registration,
    const scoped_refptr<ServiceWorkerVersion>& active_version,
    const ServiceWorkerVersion::StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  active_version->DispatchSyncEvent(
      mojo::ConvertTo<SyncRegistrationPtr>(registration), callback);
}

void BackgroundSyncManager::UnregisterImpl(
    int64 sw_registration_id,
    const RegistrationKey& registration_key,
    BackgroundSyncRegistration::RegistrationId sync_registration_id,
    SyncPeriodicity periodicity,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    BackgroundSyncMetrics::CountUnregister(
        periodicity, BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR));
    return;
  }

  const BackgroundSyncRegistration* existing_registration =
      LookupRegistration(sw_registration_id, registration_key);
  if (!existing_registration ||
      existing_registration->id() != sync_registration_id) {
    BackgroundSyncMetrics::CountUnregister(periodicity,
                                           BACKGROUND_SYNC_STATUS_NOT_FOUND);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_NOT_FOUND));
    return;
  }

  RemoveRegistrationFromMap(sw_registration_id, registration_key);

  StoreRegistrations(sw_registration_id,
                     base::Bind(&BackgroundSyncManager::UnregisterDidStore,
                                weak_ptr_factory_.GetWeakPtr(),
                                sw_registration_id, periodicity, callback));
}

void BackgroundSyncManager::UnregisterDidStore(int64 sw_registration_id,
                                               SyncPeriodicity periodicity,
                                               const StatusCallback& callback,
                                               ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status == SERVICE_WORKER_ERROR_NOT_FOUND) {
    // ServiceWorker was unregistered.
    BackgroundSyncMetrics::CountUnregister(
        periodicity, BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    sw_to_registrations_map_.erase(sw_registration_id);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR));
    return;
  }

  if (status != SERVICE_WORKER_OK) {
    LOG(ERROR) << "BackgroundSync failed to unregister due to backend failure.";
    BackgroundSyncMetrics::CountUnregister(
        periodicity, BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    DisableAndClearManager(
        base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR));
    return;
  }

  BackgroundSyncMetrics::CountUnregister(periodicity,
                                         BACKGROUND_SYNC_STATUS_OK);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_OK));
}

void BackgroundSyncManager::GetRegistrationImpl(
    int64 sw_registration_id,
    const RegistrationKey& registration_key,
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              BackgroundSyncRegistration()));
    return;
  }

  const BackgroundSyncRegistration* out_registration =
      LookupRegistration(sw_registration_id, registration_key);
  if (!out_registration) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_NOT_FOUND,
                              BackgroundSyncRegistration()));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, BACKGROUND_SYNC_STATUS_OK, *out_registration));
}

void BackgroundSyncManager::GetRegistrationsImpl(
    int64 sw_registration_id,
    SyncPeriodicity periodicity,
    const StatusAndRegistrationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<BackgroundSyncRegistration> out_registrations;

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              out_registrations));
    return;
  }

  SWIdToRegistrationsMap::iterator it =
      sw_to_registrations_map_.find(sw_registration_id);

  if (it != sw_to_registrations_map_.end()) {
    const BackgroundSyncRegistrations& registrations = it->second;
    for (const auto& tag_and_registration : registrations.registration_map) {
      const BackgroundSyncRegistration& registration =
          tag_and_registration.second;
      if (registration.options()->periodicity == periodicity)
        out_registrations.push_back(registration);
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, BACKGROUND_SYNC_STATUS_OK, out_registrations));
}

bool BackgroundSyncManager::AreOptionConditionsMet(
    const BackgroundSyncRegistrationOptions& options) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return network_observer_->NetworkSufficient(options.network_state) &&
         power_observer_->PowerSufficient(options.power_state);
}

bool BackgroundSyncManager::IsRegistrationReadyToFire(
    const BackgroundSyncRegistration& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(jkarlin): Add support for firing periodic registrations.
  if (registration.options()->periodicity == SYNC_PERIODIC)
    return false;

  if (registration.sync_state() != SYNC_STATE_PENDING)
    return false;

  DCHECK_EQ(SYNC_ONE_SHOT, registration.options()->periodicity);

  return AreOptionConditionsMet(*registration.options());
}

void BackgroundSyncManager::SchedulePendingRegistrations() {
#if defined(OS_ANDROID)
  bool keep_browser_alive_for_one_shot = false;

  for (const auto& sw_id_and_registrations : sw_to_registrations_map_) {
    for (const auto& key_and_registration :
         sw_id_and_registrations.second.registration_map) {
      const BackgroundSyncRegistration& registration =
          key_and_registration.second;
      if (registration.sync_state() == SYNC_STATE_PENDING) {
        if (registration.options()->periodicity == SYNC_ONE_SHOT) {
          keep_browser_alive_for_one_shot = true;
        } else {
          // TODO(jkarlin): Support keeping the browser alive for periodic
          // syncs.
        }
      }
    }
  }

  // TODO(jkarlin): Use the context's path instead of the 'this' pointer as an
  // identifier. See crbug.com/489705.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BackgroundSyncLauncherAndroid::LaunchBrowserWhenNextOnline,
                 this, keep_browser_alive_for_one_shot));
#else
// TODO(jkarlin): Toggle Chrome's background mode.
#endif
}

void BackgroundSyncManager::FireReadyEvents() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_)
    return;

  op_scheduler_.ScheduleOperation(
      base::Bind(&BackgroundSyncManager::FireReadyEventsImpl,
                 weak_ptr_factory_.GetWeakPtr(), MakeEmptyCompletion()));
}

void BackgroundSyncManager::FireReadyEventsImpl(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback));
    return;
  }

  // Find the registrations that are ready to run.
  std::vector<std::pair<int64, RegistrationKey>> sw_id_and_keys_to_fire;

  for (auto& sw_id_and_registrations : sw_to_registrations_map_) {
    const int64 service_worker_id = sw_id_and_registrations.first;
    for (auto& key_and_registration :
         sw_id_and_registrations.second.registration_map) {
      BackgroundSyncRegistration* registration = &key_and_registration.second;
      if (IsRegistrationReadyToFire(*registration)) {
        sw_id_and_keys_to_fire.push_back(
            std::make_pair(service_worker_id, key_and_registration.first));
        // The state change is not saved to persistent storage because
        // if the sync event is killed mid-sync then it should return to
        // SYNC_STATE_PENDING.
        registration->set_sync_state(SYNC_STATE_FIRING);
      }
    }
  }

  // If there are no registrations currently ready, then just run |callback|.
  // Otherwise, fire them all, and record the result when done.
  if (sw_id_and_keys_to_fire.size() == 0) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback));
  } else {
    base::TimeTicks start_time = base::TimeTicks::Now();

    // Fire the sync event of the ready registrations and run |callback| once
    // they're all done.
    base::Closure events_fired_barrier_closure = base::BarrierClosure(
        sw_id_and_keys_to_fire.size(), base::Bind(callback));

    // Record the total time taken after all events have run to completion.
    base::Closure events_completed_barrier_closure =
        base::BarrierClosure(sw_id_and_keys_to_fire.size(),
                             base::Bind(&OnAllSyncEventsCompleted, start_time,
                                        sw_id_and_keys_to_fire.size()));

    for (const auto& sw_id_and_key : sw_id_and_keys_to_fire) {
      int64 service_worker_id = sw_id_and_key.first;
      const BackgroundSyncRegistration* registration =
          LookupRegistration(service_worker_id, sw_id_and_key.second);

      service_worker_context_->FindRegistrationForId(
          service_worker_id, sw_to_registrations_map_[service_worker_id].origin,
          base::Bind(&BackgroundSyncManager::FireReadyEventsDidFindRegistration,
                     weak_ptr_factory_.GetWeakPtr(), sw_id_and_key.second,
                     registration->id(), events_fired_barrier_closure,
                     events_completed_barrier_closure));
    }
  }

  SchedulePendingRegistrations();
}

void BackgroundSyncManager::FireReadyEventsDidFindRegistration(
    const RegistrationKey& registration_key,
    BackgroundSyncRegistration::RegistrationId registration_id,
    const base::Closure& event_fired_callback,
    const base::Closure& event_completed_callback,
    ServiceWorkerStatusCode service_worker_status,
    const scoped_refptr<ServiceWorkerRegistration>&
        service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status != SERVICE_WORKER_OK) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(event_fired_callback));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(event_completed_callback));
    return;
  }

  BackgroundSyncRegistration* registration =
      LookupRegistration(service_worker_registration->id(), registration_key);

  FireOneShotSync(
      *registration, service_worker_registration->active_version(),
      base::Bind(&BackgroundSyncManager::EventComplete,
                 weak_ptr_factory_.GetWeakPtr(), service_worker_registration,
                 service_worker_registration->id(), registration_key,
                 registration_id, event_completed_callback));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(event_fired_callback));
}

// |service_worker_registration| is just to keep the registration alive
// while the event is firing.
void BackgroundSyncManager::EventComplete(
    const scoped_refptr<ServiceWorkerRegistration>& service_worker_registration,
    int64 service_worker_id,
    const RegistrationKey& key,
    BackgroundSyncRegistration::RegistrationId sync_registration_id,
    const base::Closure& callback,
    ServiceWorkerStatusCode status_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (disabled_)
    return;

  op_scheduler_.ScheduleOperation(base::Bind(
      &BackgroundSyncManager::EventCompleteImpl, weak_ptr_factory_.GetWeakPtr(),
      service_worker_id, key, sync_registration_id, status_code,
      MakeClosureCompletion(callback)));
}

void BackgroundSyncManager::EventCompleteImpl(
    int64 service_worker_id,
    const RegistrationKey& key,
    BackgroundSyncRegistration::RegistrationId sync_registration_id,
    ServiceWorkerStatusCode status_code,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback));
    return;
  }

  BackgroundSyncRegistration* registration =
      LookupRegistration(service_worker_id, key);
  if (!registration || registration->id() != sync_registration_id) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback));
    return;
  }

  // The event ran to completion, we should count it, no matter what happens
  // from here.
  BackgroundSyncMetrics::RecordEventResult(registration->options()->periodicity,
                                           status_code == SERVICE_WORKER_OK);

  if (registration->options()->periodicity == SYNC_ONE_SHOT) {
    if (status_code != SERVICE_WORKER_OK) {
      // TODO(jkarlin) Fire the sync event on the next page load controlled by
      // this registration. (crbug.com/479665)
      registration->set_sync_state(SYNC_STATE_FAILED);
    } else {
      registration = nullptr;
      RemoveRegistrationFromMap(service_worker_id, key);
    }
  } else {
    // TODO(jkarlin): Add support for running periodic syncs. (crbug.com/479674)
    NOTREACHED();
  }

  StoreRegistrations(
      service_worker_id,
      base::Bind(&BackgroundSyncManager::EventCompleteDidStore,
                 weak_ptr_factory_.GetWeakPtr(), service_worker_id, callback));
}

void BackgroundSyncManager::EventCompleteDidStore(
    int64 service_worker_id,
    const base::Closure& callback,
    ServiceWorkerStatusCode status_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status_code == SERVICE_WORKER_ERROR_NOT_FOUND) {
    // The registration is gone.
    sw_to_registrations_map_.erase(service_worker_id);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback));
    return;
  }

  if (status_code != SERVICE_WORKER_OK) {
    LOG(ERROR) << "BackgroundSync failed to store registration due to backend "
                  "failure.";
    DisableAndClearManager(base::Bind(callback));
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback));
}

// static
void BackgroundSyncManager::OnAllSyncEventsCompleted(
    const base::TimeTicks& start_time,
    int number_of_batched_sync_events) {
  // Record the combined time taken by all sync events.
  BackgroundSyncMetrics::RecordBatchSyncEventComplete(
      base::TimeTicks::Now() - start_time, number_of_batched_sync_events);
}

void BackgroundSyncManager::OnRegistrationDeletedImpl(
    int64 registration_id,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The backend (ServiceWorkerStorage) will delete the data, so just delete the
  // memory representation here.
  sw_to_registrations_map_.erase(registration_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback));
}

void BackgroundSyncManager::OnStorageWipedImpl(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  sw_to_registrations_map_.clear();
  disabled_ = false;
  InitImpl(callback);
}

void BackgroundSyncManager::OnNetworkChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  FireReadyEvents();
}

void BackgroundSyncManager::OnPowerChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  FireReadyEvents();
}

template <typename CallbackT, typename... Params>
void BackgroundSyncManager::CompleteOperationCallback(const CallbackT& callback,
                                                      Params... parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  callback.Run(parameters...);
  op_scheduler_.CompleteOperationAndRunNext();
}

base::Closure BackgroundSyncManager::MakeEmptyCompletion() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return MakeClosureCompletion(base::Bind(base::DoNothing));
}

base::Closure BackgroundSyncManager::MakeClosureCompletion(
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return base::Bind(
      &BackgroundSyncManager::CompleteOperationCallback<base::Closure>,
      weak_ptr_factory_.GetWeakPtr(), callback);
}

BackgroundSyncManager::StatusAndRegistrationCallback
BackgroundSyncManager::MakeStatusAndRegistrationCompletion(
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return base::Bind(&BackgroundSyncManager::CompleteOperationCallback<
                        StatusAndRegistrationCallback, BackgroundSyncStatus,
                        const BackgroundSyncRegistration&>,
                    weak_ptr_factory_.GetWeakPtr(), callback);
}

BackgroundSyncManager::StatusAndRegistrationsCallback
BackgroundSyncManager::MakeStatusAndRegistrationsCompletion(
    const StatusAndRegistrationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return base::Bind(&BackgroundSyncManager::CompleteOperationCallback<
                        StatusAndRegistrationsCallback, BackgroundSyncStatus,
                        const std::vector<BackgroundSyncRegistration>&>,
                    weak_ptr_factory_.GetWeakPtr(), callback);
}

BackgroundSyncManager::StatusCallback
BackgroundSyncManager::MakeStatusCompletion(const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return base::Bind(
      &BackgroundSyncManager::CompleteOperationCallback<StatusCallback,
                                                        BackgroundSyncStatus>,
      weak_ptr_factory_.GetWeakPtr(), callback);
}

}  // namespace content
