// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_manager.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "content/browser/background_sync/background_sync_metrics.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/background_sync/background_sync_registration_handle.h"
#include "content/browser/background_sync/background_sync_registration_options.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_type_converters.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/background_sync.mojom.h"

#if defined(OS_ANDROID)
#include "content/browser/android/background_sync_network_observer_android.h"
#endif

namespace content {

class BackgroundSyncManager::RefCountedRegistration
    : public base::RefCounted<RefCountedRegistration> {
 public:
  BackgroundSyncRegistration* value() { return &registration_; }
  const BackgroundSyncRegistration* value() const { return &registration_; }

 private:
  friend class base::RefCounted<RefCountedRegistration>;
  ~RefCountedRegistration() = default;

  BackgroundSyncRegistration registration_;
};

namespace {

// The key used to index the background sync data in ServiceWorkerStorage.
const char kBackgroundSyncUserDataKey[] = "BackgroundSyncUserData";

void PostErrorResponse(
    BackgroundSyncStatus status,
    const BackgroundSyncManager::StatusAndRegistrationCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, status,
                 base::Passed(scoped_ptr<BackgroundSyncRegistrationHandle>())));
}

// Returns nullptr if the controller cannot be accessed for any reason.
BackgroundSyncController* GetBackgroundSyncControllerOnUIThread(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!service_worker_context)
    return nullptr;
  StoragePartitionImpl* storage_partition_impl =
      service_worker_context->storage_partition();
  if (!storage_partition_impl)  // may be null in tests
    return nullptr;

  return storage_partition_impl->browser_context()
      ->GetBackgroundSyncController();
}

void NotifyBackgroundSyncRegisteredOnUIThread(
    const scoped_refptr<ServiceWorkerContextWrapper>& sw_context_wrapper,
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncControllerOnUIThread(sw_context_wrapper);

  if (!background_sync_controller)
    return;

  background_sync_controller->NotifyBackgroundSyncRegistered(origin);
}

void RunInBackgroundOnUIThread(
    const scoped_refptr<ServiceWorkerContextWrapper>& sw_context_wrapper,
    bool enabled,
    int64_t min_ms) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncControllerOnUIThread(sw_context_wrapper);
  if (background_sync_controller) {
    background_sync_controller->RunInBackground(enabled, min_ms);
  }
}

scoped_ptr<BackgroundSyncParameters> GetControllerParameters(
    const scoped_refptr<ServiceWorkerContextWrapper>& sw_context_wrapper,
    scoped_ptr<BackgroundSyncParameters> parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncControllerOnUIThread(sw_context_wrapper);

  if (!background_sync_controller) {
    // If there is no controller then BackgroundSync can't run in the
    // background, disable it.
    parameters->disable = true;
    return parameters;
  }

  background_sync_controller->GetParameterOverrides(parameters.get());
  return parameters;
}

void OnSyncEventFinished(
    const scoped_refptr<ServiceWorkerVersion>& active_version,
    int request_id,
    const ServiceWorkerVersion::StatusCallback& callback,
    ServiceWorkerEventStatus status) {
  if (!active_version->FinishRequest(request_id))
    return;
  callback.Run(mojo::ConvertTo<ServiceWorkerStatusCode>(status));
}

}  // namespace

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
    : RegistrationKey(registration.options()->tag) {}

BackgroundSyncManager::RegistrationKey::RegistrationKey(
    const BackgroundSyncRegistrationOptions& options)
    : RegistrationKey(options.tag) {}

BackgroundSyncManager::RegistrationKey::RegistrationKey(const std::string& tag)
    : value_("o_" + tag) {
  // Note that the "o_" prefix on the key is because at one time one-shots were
  // prefixed with an "o_" and periodic with a "p_". Removing the "o_" requires
  // migration.
}

void BackgroundSyncManager::Register(
    int64_t sw_registration_id,
    const BackgroundSyncRegistrationOptions& options,
    bool requested_from_service_worker,
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    BackgroundSyncMetrics::CountRegisterFailure(
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    PostErrorResponse(BACKGROUND_SYNC_STATUS_STORAGE_ERROR, callback);
    return;
  }

  if (requested_from_service_worker) {
    op_scheduler_.ScheduleOperation(
        base::Bind(&BackgroundSyncManager::RegisterCheckIfHasMainFrame,
                   weak_ptr_factory_.GetWeakPtr(), sw_registration_id, options,
                   MakeStatusAndRegistrationCompletion(callback)));
    return;
  }

  op_scheduler_.ScheduleOperation(
      base::Bind(&BackgroundSyncManager::RegisterImpl,
                 weak_ptr_factory_.GetWeakPtr(), sw_registration_id, options,
                 MakeStatusAndRegistrationCompletion(callback)));
}

void BackgroundSyncManager::GetRegistration(
    int64_t sw_registration_id,
    const std::string& sync_registration_tag,
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    PostErrorResponse(BACKGROUND_SYNC_STATUS_STORAGE_ERROR, callback);
    return;
  }

  RegistrationKey registration_key(sync_registration_tag);

  op_scheduler_.ScheduleOperation(base::Bind(
      &BackgroundSyncManager::GetRegistrationImpl,
      weak_ptr_factory_.GetWeakPtr(), sw_registration_id, registration_key,
      MakeStatusAndRegistrationCompletion(callback)));
}

void BackgroundSyncManager::GetRegistrations(
    int64_t sw_registration_id,
    const StatusAndRegistrationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(
            callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
            base::Passed(
                scoped_ptr<ScopedVector<BackgroundSyncRegistrationHandle>>(
                    new ScopedVector<BackgroundSyncRegistrationHandle>()))));
    return;
  }

  op_scheduler_.ScheduleOperation(
      base::Bind(&BackgroundSyncManager::GetRegistrationsImpl,
                 weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                 MakeStatusAndRegistrationsCompletion(callback)));
}

// Given a HandleId |handle_id|, return a new handle for the same
// registration.
scoped_ptr<BackgroundSyncRegistrationHandle>
BackgroundSyncManager::DuplicateRegistrationHandle(
    BackgroundSyncRegistrationHandle::HandleId handle_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  scoped_refptr<RefCountedRegistration>* ref_registration =
      registration_handle_ids_.Lookup(handle_id);
  if (!ref_registration)
    return scoped_ptr<BackgroundSyncRegistrationHandle>();
  return CreateRegistrationHandle(ref_registration->get());
}

void BackgroundSyncManager::OnRegistrationDeleted(int64_t sw_registration_id,
                                                  const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Operations already in the queue will either fail when they write to storage
  // or return stale results based on registrations loaded in memory. This is
  // inconsequential since the service worker is gone.
  op_scheduler_.ScheduleOperation(
      base::Bind(&BackgroundSyncManager::OnRegistrationDeletedImpl,
                 weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                 MakeEmptyCompletion()));
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

void BackgroundSyncManager::SetMaxSyncAttemptsForTesting(int max_attempts) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  op_scheduler_.ScheduleOperation(base::Bind(
      &BackgroundSyncManager::SetMaxSyncAttemptsImpl,
      weak_ptr_factory_.GetWeakPtr(), max_attempts, MakeEmptyCompletion()));
}

BackgroundSyncManager::BackgroundSyncManager(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : service_worker_context_(service_worker_context),
      parameters_(new BackgroundSyncParameters()),
      disabled_(false),
      num_firing_registrations_(0),
      clock_(new base::DefaultClock()),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->AddObserver(this);

#if defined(OS_ANDROID)
  network_observer_.reset(new BackgroundSyncNetworkObserverAndroid(
      base::Bind(&BackgroundSyncManager::OnNetworkChanged,
                 weak_ptr_factory_.GetWeakPtr())));
#else
  network_observer_.reset(new BackgroundSyncNetworkObserver(
      base::Bind(&BackgroundSyncManager::OnNetworkChanged,
                 weak_ptr_factory_.GetWeakPtr())));
#endif
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

  scoped_ptr<BackgroundSyncParameters> parameters_copy(
      new BackgroundSyncParameters(*parameters_));

  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&GetControllerParameters, service_worker_context_,
                 base::Passed(std::move(parameters_copy))),
      base::Bind(&BackgroundSyncManager::InitDidGetControllerParameters,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void BackgroundSyncManager::InitDidGetControllerParameters(
    const base::Closure& callback,
    scoped_ptr<BackgroundSyncParameters> updated_parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  parameters_ = std::move(updated_parameters);
  if (parameters_->disable) {
    disabled_ = true;
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
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != SERVICE_WORKER_OK && status != SERVICE_WORKER_ERROR_NOT_FOUND) {
    LOG(ERROR) << "BackgroundSync failed to init due to backend failure.";
    DisableAndClearManager(base::Bind(callback));
    return;
  }

  bool corruption_detected = false;
  for (const std::pair<int64_t, std::string>& data : user_data) {
    BackgroundSyncRegistrationsProto registrations_proto;
    if (registrations_proto.ParseFromString(data.second)) {
      BackgroundSyncRegistrations* registrations =
          &active_registrations_[data.first];
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

        RegistrationKey registration_key(registration_proto.tag());

        scoped_refptr<RefCountedRegistration> ref_registration(
            new RefCountedRegistration());
        registrations->registration_map[registration_key] = ref_registration;
        BackgroundSyncRegistration* registration = ref_registration->value();

        BackgroundSyncRegistrationOptions* options = registration->options();
        options->tag = registration_proto.tag();
        options->network_state = registration_proto.network_state();

        registration->set_id(registration_proto.id());
        registration->set_num_attempts(registration_proto.num_attempts());
        registration->set_delay_until(
            base::Time::FromInternalValue(registration_proto.delay_until()));
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

void BackgroundSyncManager::RegisterCheckIfHasMainFrame(
    int64_t sw_registration_id,
    const BackgroundSyncRegistrationOptions& options,
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    BackgroundSyncMetrics::CountRegisterFailure(
        BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER);
    PostErrorResponse(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER, callback);
    return;
  }

  HasMainFrameProviderHost(
      sw_registration->pattern().GetOrigin(),
      base::Bind(&BackgroundSyncManager::RegisterDidCheckIfMainFrame,
                 weak_ptr_factory_.GetWeakPtr(), sw_registration_id, options,
                 callback));
}

void BackgroundSyncManager::RegisterDidCheckIfMainFrame(
    int64_t sw_registration_id,
    const BackgroundSyncRegistrationOptions& options,
    const StatusAndRegistrationCallback& callback,
    bool has_main_frame_client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!has_main_frame_client) {
    BackgroundSyncMetrics::CountRegisterFailure(
        BACKGROUND_SYNC_STATUS_NOT_ALLOWED);
    PostErrorResponse(BACKGROUND_SYNC_STATUS_NOT_ALLOWED, callback);
    return;
  }
  RegisterImpl(sw_registration_id, options, callback);
}

void BackgroundSyncManager::RegisterImpl(
    int64_t sw_registration_id,
    const BackgroundSyncRegistrationOptions& options,
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    BackgroundSyncMetrics::CountRegisterFailure(
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    PostErrorResponse(BACKGROUND_SYNC_STATUS_STORAGE_ERROR, callback);
    return;
  }

  if (options.tag.length() > kMaxTagLength) {
    BackgroundSyncMetrics::CountRegisterFailure(
        BACKGROUND_SYNC_STATUS_NOT_ALLOWED);
    PostErrorResponse(BACKGROUND_SYNC_STATUS_NOT_ALLOWED, callback);
    return;
  }

  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    BackgroundSyncMetrics::CountRegisterFailure(
        BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER);
    PostErrorResponse(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER, callback);
    return;
  }

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&NotifyBackgroundSyncRegisteredOnUIThread,
                                     service_worker_context_,
                                     sw_registration->pattern().GetOrigin()));

  RefCountedRegistration* existing_registration_ref =
      LookupActiveRegistration(sw_registration_id, RegistrationKey(options));
  if (existing_registration_ref) {
    if (existing_registration_ref->value()->options()->Equals(options)) {
      BackgroundSyncRegistration* existing_registration =
          existing_registration_ref->value();

      BackgroundSyncMetrics::RegistrationCouldFire registration_could_fire =
          AreOptionConditionsMet(options)
              ? BackgroundSyncMetrics::REGISTRATION_COULD_FIRE
              : BackgroundSyncMetrics::REGISTRATION_COULD_NOT_FIRE;
      BackgroundSyncMetrics::CountRegisterSuccess(
          registration_could_fire,
          BackgroundSyncMetrics::REGISTRATION_IS_DUPLICATE);

    if (existing_registration->IsFiring()) {
      existing_registration->set_sync_state(
          BackgroundSyncState::REREGISTERED_WHILE_FIRING);
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(
            callback, BACKGROUND_SYNC_STATUS_OK,
            base::Passed(CreateRegistrationHandle(existing_registration_ref))));
    return;
    } else {
      existing_registration_ref->value()->SetUnregisteredState();
    }
  }

  scoped_refptr<RefCountedRegistration> new_ref_registration(
      new RefCountedRegistration());
  BackgroundSyncRegistration* new_registration = new_ref_registration->value();

  *new_registration->options() = options;

  BackgroundSyncRegistrations* registrations =
      &active_registrations_[sw_registration_id];
  new_registration->set_id(registrations->next_id++);

  AddActiveRegistration(sw_registration_id,
                        sw_registration->pattern().GetOrigin(),
                        new_ref_registration);

  StoreRegistrations(
      sw_registration_id,
      base::Bind(&BackgroundSyncManager::RegisterDidStore,
                 weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                 new_ref_registration, callback));
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

  for (auto& sw_id_and_registrations : active_registrations_) {
    for (auto& key_and_registration :
         sw_id_and_registrations.second.registration_map) {
      BackgroundSyncRegistration* registration =
          key_and_registration.second->value();
      registration->SetUnregisteredState();
    }
  }

  active_registrations_.clear();

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
    const std::vector<std::pair<int64_t, std::string>>& user_data,
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

BackgroundSyncManager::RefCountedRegistration*
BackgroundSyncManager::LookupActiveRegistration(
    int64_t sw_registration_id,
    const RegistrationKey& registration_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SWIdToRegistrationsMap::iterator it =
      active_registrations_.find(sw_registration_id);
  if (it == active_registrations_.end())
    return nullptr;

  BackgroundSyncRegistrations& registrations = it->second;
  DCHECK_LE(BackgroundSyncRegistration::kInitialId, registrations.next_id);
  DCHECK(!registrations.origin.is_empty());

  auto key_and_registration_iter =
      registrations.registration_map.find(registration_key);
  if (key_and_registration_iter == registrations.registration_map.end())
    return nullptr;

  return key_and_registration_iter->second.get();
}

void BackgroundSyncManager::StoreRegistrations(
    int64_t sw_registration_id,
    const ServiceWorkerStorage::StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Serialize the data.
  const BackgroundSyncRegistrations& registrations =
      active_registrations_[sw_registration_id];
  BackgroundSyncRegistrationsProto registrations_proto;
  registrations_proto.set_next_registration_id(registrations.next_id);
  registrations_proto.set_origin(registrations.origin.spec());

  for (const auto& key_and_registration : registrations.registration_map) {
    const BackgroundSyncRegistration& registration =
        *key_and_registration.second->value();
    BackgroundSyncRegistrationProto* registration_proto =
        registrations_proto.add_registration();
    registration_proto->set_id(registration.id());
    registration_proto->set_tag(registration.options()->tag);
    registration_proto->set_network_state(
        registration.options()->network_state);
    registration_proto->set_num_attempts(registration.num_attempts());
    registration_proto->set_delay_until(
        registration.delay_until().ToInternalValue());
  }
  std::string serialized;
  bool success = registrations_proto.SerializeToString(&serialized);
  DCHECK(success);

  StoreDataInBackend(sw_registration_id, registrations.origin,
                     kBackgroundSyncUserDataKey, serialized, callback);
}

void BackgroundSyncManager::RegisterDidStore(
    int64_t sw_registration_id,
    const scoped_refptr<RefCountedRegistration>& new_registration_ref,
    const StatusAndRegistrationCallback& callback,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const BackgroundSyncRegistration* new_registration =
      new_registration_ref->value();

  if (status == SERVICE_WORKER_ERROR_NOT_FOUND) {
    // The service worker registration is gone.
    BackgroundSyncMetrics::CountRegisterFailure(
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    active_registrations_.erase(sw_registration_id);
    PostErrorResponse(BACKGROUND_SYNC_STATUS_STORAGE_ERROR, callback);
    return;
  }

  if (status != SERVICE_WORKER_OK) {
    LOG(ERROR) << "BackgroundSync failed to store registration due to backend "
                  "failure.";
    BackgroundSyncMetrics::CountRegisterFailure(
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    DisableAndClearManager(base::Bind(
        callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
        base::Passed(scoped_ptr<BackgroundSyncRegistrationHandle>())));
    return;
  }

  BackgroundSyncMetrics::RegistrationCouldFire registration_could_fire =
      AreOptionConditionsMet(*new_registration->options())
          ? BackgroundSyncMetrics::REGISTRATION_COULD_FIRE
          : BackgroundSyncMetrics::REGISTRATION_COULD_NOT_FIRE;
  BackgroundSyncMetrics::CountRegisterSuccess(
      registration_could_fire,
      BackgroundSyncMetrics::REGISTRATION_IS_NOT_DUPLICATE);

  FireReadyEvents();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          callback, BACKGROUND_SYNC_STATUS_OK,
          base::Passed(CreateRegistrationHandle(new_registration_ref.get()))));
}

void BackgroundSyncManager::RemoveActiveRegistration(
    int64_t sw_registration_id,
    const RegistrationKey& registration_key) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(LookupActiveRegistration(sw_registration_id, registration_key));

  BackgroundSyncRegistrations* registrations =
      &active_registrations_[sw_registration_id];

  registrations->registration_map.erase(registration_key);
}

void BackgroundSyncManager::AddActiveRegistration(
    int64_t sw_registration_id,
    const GURL& origin,
    const scoped_refptr<RefCountedRegistration>& sync_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(sync_registration->value()->IsValid());

  BackgroundSyncRegistrations* registrations =
      &active_registrations_[sw_registration_id];
  registrations->origin = origin;

  RegistrationKey registration_key(*sync_registration->value());
  registrations->registration_map[registration_key] = sync_registration;
}

void BackgroundSyncManager::StoreDataInBackend(
    int64_t sw_registration_id,
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
    BackgroundSyncRegistrationHandle::HandleId handle_id,
    const scoped_refptr<ServiceWorkerVersion>& active_version,
    BackgroundSyncEventLastChance last_chance,
    const ServiceWorkerVersion::StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(active_version);

  if (active_version->running_status() != ServiceWorkerVersion::RUNNING) {
    active_version->RunAfterStartWorker(
        base::Bind(&BackgroundSyncManager::FireOneShotSync,
                   weak_ptr_factory_.GetWeakPtr(), handle_id, active_version,
                   last_chance, callback),
        callback);
    return;
  }

  int request_id = active_version->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC, callback,
      parameters_->max_sync_event_duration,
      ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);
  base::WeakPtr<BackgroundSyncServiceClient> client =
      active_version->GetMojoServiceForRequest<BackgroundSyncServiceClient>(
          request_id);

  // The ServiceWorkerVersion doesn't know when the client (javascript) is done
  // with the registration so don't give it a BackgroundSyncRegistrationHandle.
  // Once the render process gets the handle_id it can create its own handle
  // (with a new unique handle id).
  client->Sync(
      handle_id, last_chance,
      base::Bind(&OnSyncEventFinished, active_version, request_id, callback));
}

void BackgroundSyncManager::ScheduleDelayedTask(const base::Closure& callback,
                                                base::TimeDelta delay) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, callback,
                                                       delay);
}

void BackgroundSyncManager::HasMainFrameProviderHost(
    const GURL& origin,
    const BoolCallback& callback) {
  service_worker_context_->HasMainFrameProviderHost(origin, callback);
}

scoped_ptr<BackgroundSyncRegistrationHandle>
BackgroundSyncManager::CreateRegistrationHandle(
    const scoped_refptr<RefCountedRegistration>& registration) {
  scoped_refptr<RefCountedRegistration>* ptr =
      new scoped_refptr<RefCountedRegistration>(registration);

  // Registration handles have unique handle ids. The handle id maps to an
  // internal RefCountedRegistration (which has the persistent registration id)
  // via
  // registration_reference_ids_.
  BackgroundSyncRegistrationHandle::HandleId handle_id =
      registration_handle_ids_.Add(ptr);

  return make_scoped_ptr(new BackgroundSyncRegistrationHandle(
      weak_ptr_factory_.GetWeakPtr(), handle_id));
}

BackgroundSyncRegistration* BackgroundSyncManager::GetRegistrationForHandle(
    BackgroundSyncRegistrationHandle::HandleId handle_id) const {
  scoped_refptr<RefCountedRegistration>* ref_registration =
      registration_handle_ids_.Lookup(handle_id);
  if (!ref_registration)
    return nullptr;
  return (*ref_registration)->value();
}

void BackgroundSyncManager::ReleaseRegistrationHandle(
    BackgroundSyncRegistrationHandle::HandleId handle_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(registration_handle_ids_.Lookup(handle_id));
  registration_handle_ids_.Remove(handle_id);
}

void BackgroundSyncManager::Unregister(
    int64_t sw_registration_id,
    BackgroundSyncRegistrationHandle::HandleId handle_id,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncRegistration* registration =
      GetRegistrationForHandle(handle_id);
  DCHECK(registration);

  if (disabled_) {
    BackgroundSyncMetrics::CountUnregister(
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR));
    return;
  }

  op_scheduler_.ScheduleOperation(base::Bind(
      &BackgroundSyncManager::UnregisterImpl, weak_ptr_factory_.GetWeakPtr(),
      sw_registration_id, RegistrationKey(*registration), registration->id(),
      MakeStatusCompletion(callback)));
}

void BackgroundSyncManager::UnregisterImpl(
    int64_t sw_registration_id,
    const RegistrationKey& registration_key,
    BackgroundSyncRegistration::RegistrationId sync_registration_id,
    const StatusCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    BackgroundSyncMetrics::CountUnregister(
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR));
    return;
  }

  RefCountedRegistration* existing_registration =
      LookupActiveRegistration(sw_registration_id, registration_key);

  if (!existing_registration ||
      existing_registration->value()->id() != sync_registration_id) {
    BackgroundSyncMetrics::CountUnregister(BACKGROUND_SYNC_STATUS_NOT_FOUND);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_NOT_FOUND));
    return;
  }

  existing_registration->value()->SetUnregisteredState();

  RemoveActiveRegistration(sw_registration_id, registration_key);

  StoreRegistrations(
      sw_registration_id,
      base::Bind(&BackgroundSyncManager::UnregisterDidStore,
                 weak_ptr_factory_.GetWeakPtr(), sw_registration_id, callback));
}

void BackgroundSyncManager::UnregisterDidStore(int64_t sw_registration_id,
                                               const StatusCallback& callback,
                                               ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status == SERVICE_WORKER_ERROR_NOT_FOUND) {
    // ServiceWorker was unregistered.
    BackgroundSyncMetrics::CountUnregister(
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    active_registrations_.erase(sw_registration_id);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR));
    return;
  }

  if (status != SERVICE_WORKER_OK) {
    LOG(ERROR) << "BackgroundSync failed to unregister due to backend failure.";
    BackgroundSyncMetrics::CountUnregister(
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    DisableAndClearManager(
        base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR));
    return;
  }

  BackgroundSyncMetrics::CountUnregister(BACKGROUND_SYNC_STATUS_OK);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_OK));
}

void BackgroundSyncManager::NotifyWhenFinished(
    BackgroundSyncRegistrationHandle::HandleId handle_id,
    const StatusAndStateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              BackgroundSyncState::FAILED));
    return;
  }

  scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle =
      DuplicateRegistrationHandle(handle_id);

  op_scheduler_.ScheduleOperation(
      base::Bind(&BackgroundSyncManager::NotifyWhenFinishedImpl,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(std::move(registration_handle)), callback));
}

void BackgroundSyncManager::NotifyWhenFinishedImpl(
    scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle,
    const StatusAndStateCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              BackgroundSyncState::FAILED));
    return;
  }

  if (!registration_handle->registration()->HasCompleted()) {
    registration_handle->registration()->AddFinishedCallback(
        base::Bind(&BackgroundSyncManager::NotifyWhenFinishedInvokeCallback,
                   weak_ptr_factory_.GetWeakPtr(), callback));
    op_scheduler_.CompleteOperationAndRunNext();
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_OK,
                            registration_handle->sync_state()));
  op_scheduler_.CompleteOperationAndRunNext();
}

void BackgroundSyncManager::NotifyWhenFinishedInvokeCallback(
    const StatusAndStateCallback& callback,
    BackgroundSyncState sync_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  callback.Run(BACKGROUND_SYNC_STATUS_OK, sync_state);
}

void BackgroundSyncManager::GetRegistrationImpl(
    int64_t sw_registration_id,
    const RegistrationKey& registration_key,
    const StatusAndRegistrationCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    PostErrorResponse(BACKGROUND_SYNC_STATUS_STORAGE_ERROR, callback);
    return;
  }

  RefCountedRegistration* registration =
      LookupActiveRegistration(sw_registration_id, registration_key);
  if (!registration) {
    PostErrorResponse(BACKGROUND_SYNC_STATUS_NOT_FOUND, callback);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, BACKGROUND_SYNC_STATUS_OK,
                 base::Passed(CreateRegistrationHandle(registration))));
}

void BackgroundSyncManager::GetRegistrationsImpl(
    int64_t sw_registration_id,
    const StatusAndRegistrationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  scoped_ptr<ScopedVector<BackgroundSyncRegistrationHandle>> out_registrations(
      new ScopedVector<BackgroundSyncRegistrationHandle>());

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              base::Passed(std::move(out_registrations))));
    return;
  }

  SWIdToRegistrationsMap::iterator it =
      active_registrations_.find(sw_registration_id);

  if (it != active_registrations_.end()) {
    const BackgroundSyncRegistrations& registrations = it->second;
    for (const auto& tag_and_registration : registrations.registration_map) {
      RefCountedRegistration* registration = tag_and_registration.second.get();
      out_registrations->push_back(
          CreateRegistrationHandle(registration).release());
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, BACKGROUND_SYNC_STATUS_OK,
                            base::Passed(std::move(out_registrations))));
}

bool BackgroundSyncManager::AreOptionConditionsMet(
    const BackgroundSyncRegistrationOptions& options) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return network_observer_->NetworkSufficient(options.network_state);
}

bool BackgroundSyncManager::IsRegistrationReadyToFire(
    const BackgroundSyncRegistration& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (registration.sync_state() != BackgroundSyncState::PENDING)
    return false;

  if (clock_->Now() < registration.delay_until())
    return false;

  return AreOptionConditionsMet(*registration.options());
}

void BackgroundSyncManager::RunInBackgroundIfNecessary() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::TimeDelta soonest_wakeup_delta = base::TimeDelta::Max();

  for (const auto& sw_id_and_registrations : active_registrations_) {
    for (const auto& key_and_registration :
         sw_id_and_registrations.second.registration_map) {
      const BackgroundSyncRegistration& registration =
          *key_and_registration.second->value();
      if (registration.sync_state() == BackgroundSyncState::PENDING) {
        if (clock_->Now() >= registration.delay_until()) {
          soonest_wakeup_delta = base::TimeDelta();
        } else {
          base::TimeDelta delay_delta =
              registration.delay_until() - clock_->Now();
          if (delay_delta < soonest_wakeup_delta)
            soonest_wakeup_delta = delay_delta;
        }
      }
    }
  }

  // If the browser is closed while firing events, the browser needs a task to
  // wake it back up and try again.
  if (num_firing_registrations_ > 0 &&
      soonest_wakeup_delta > parameters_->min_sync_recovery_time) {
    soonest_wakeup_delta = parameters_->min_sync_recovery_time;
  }

  // Try firing again after the wakeup delta.
  if (!soonest_wakeup_delta.is_max() &&
      soonest_wakeup_delta != base::TimeDelta()) {
    delayed_sync_task_.Reset(base::Bind(&BackgroundSyncManager::FireReadyEvents,
                                        weak_ptr_factory_.GetWeakPtr()));
    ScheduleDelayedTask(delayed_sync_task_.callback(), soonest_wakeup_delta);
  }

  // In case the browser closes (or to prevent it from closing), call
  // RunInBackground to either wake up the browser at the wakeup delta or to
  // keep the browser running.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(RunInBackgroundOnUIThread, service_worker_context_,
                 !soonest_wakeup_delta.is_max() /* should run in background */,
                 soonest_wakeup_delta.InMilliseconds()));
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
  std::vector<std::pair<int64_t, RegistrationKey>> sw_id_and_keys_to_fire;

  for (auto& sw_id_and_registrations : active_registrations_) {
    const int64_t service_worker_id = sw_id_and_registrations.first;
    for (auto& key_and_registration :
         sw_id_and_registrations.second.registration_map) {
      BackgroundSyncRegistration* registration =
          key_and_registration.second->value();
      if (IsRegistrationReadyToFire(*registration)) {
        sw_id_and_keys_to_fire.push_back(
            std::make_pair(service_worker_id, key_and_registration.first));
        // The state change is not saved to persistent storage because
        // if the sync event is killed mid-sync then it should return to
        // SYNC_STATE_PENDING.
        registration->set_sync_state(BackgroundSyncState::FIRING);
      }
    }
  }

  if (sw_id_and_keys_to_fire.empty()) {
    RunInBackgroundIfNecessary();
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback));
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();

  // Fire the sync event of the ready registrations and run |callback| once
  // they're all done.
  base::Closure events_fired_barrier_closure = base::BarrierClosure(
      sw_id_and_keys_to_fire.size(),
      base::Bind(&BackgroundSyncManager::FireReadyEventsAllEventsFiring,
                 weak_ptr_factory_.GetWeakPtr(), callback));

  // Record the total time taken after all events have run to completion.
  base::Closure events_completed_barrier_closure =
      base::BarrierClosure(sw_id_and_keys_to_fire.size(),
                           base::Bind(&OnAllSyncEventsCompleted, start_time,
                                      sw_id_and_keys_to_fire.size()));

  for (const auto& sw_id_and_key : sw_id_and_keys_to_fire) {
    int64_t service_worker_id = sw_id_and_key.first;
    const RefCountedRegistration* registration =
        LookupActiveRegistration(service_worker_id, sw_id_and_key.second);
    DCHECK(registration);

    service_worker_context_->FindReadyRegistrationForId(
        service_worker_id, active_registrations_[service_worker_id].origin,
        base::Bind(&BackgroundSyncManager::FireReadyEventsDidFindRegistration,
                   weak_ptr_factory_.GetWeakPtr(), sw_id_and_key.second,
                   registration->value()->id(), events_fired_barrier_closure,
                   events_completed_barrier_closure));
  }
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

  RefCountedRegistration* registration = LookupActiveRegistration(
      service_worker_registration->id(), registration_key);
  DCHECK(registration);

  // Create a handle and keep it until the sync event completes. The client can
  // acquire its own handle for longer-term use.
  scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle =
      CreateRegistrationHandle(registration);

  num_firing_registrations_ += 1;

  BackgroundSyncRegistrationHandle::HandleId handle_id =
      registration_handle->handle_id();

  BackgroundSyncEventLastChance last_chance =
      registration->value()->num_attempts() ==
              parameters_->max_sync_attempts - 1
          ? BackgroundSyncEventLastChance::IS_LAST_CHANCE
          : BackgroundSyncEventLastChance::IS_NOT_LAST_CHANCE;

  HasMainFrameProviderHost(
      service_worker_registration->pattern().GetOrigin(),
      base::Bind(&BackgroundSyncMetrics::RecordEventStarted));

  FireOneShotSync(
      handle_id, service_worker_registration->active_version(), last_chance,
      base::Bind(&BackgroundSyncManager::EventComplete,
                 weak_ptr_factory_.GetWeakPtr(), service_worker_registration,
                 service_worker_registration->id(),
                 base::Passed(std::move(registration_handle)),
                 event_completed_callback));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(event_fired_callback));
}

void BackgroundSyncManager::FireReadyEventsAllEventsFiring(
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunInBackgroundIfNecessary();
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback));
}

// |service_worker_registration| is just to keep the registration alive
// while the event is firing.
void BackgroundSyncManager::EventComplete(
    const scoped_refptr<ServiceWorkerRegistration>& service_worker_registration,
    int64_t service_worker_id,
    scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle,
    const base::Closure& callback,
    ServiceWorkerStatusCode status_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Do not check for disabled as events that were firing when disabled should
  // be allowed to complete (for NotifyWhenFinished).
  op_scheduler_.ScheduleOperation(base::Bind(
      &BackgroundSyncManager::EventCompleteImpl, weak_ptr_factory_.GetWeakPtr(),
      service_worker_id, base::Passed(std::move(registration_handle)),
      status_code, MakeClosureCompletion(callback)));
}

void BackgroundSyncManager::EventCompleteImpl(
    int64_t service_worker_id,
    scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle,
    ServiceWorkerStatusCode status_code,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncRegistration* registration =
      registration_handle->registration();
  DCHECK(registration);
  DCHECK(!registration->HasCompleted());

  registration->set_num_attempts(registration->num_attempts() + 1);

  num_firing_registrations_ -= 1;

  // The event ran to completion, we should count it, no matter what happens
  // from here.
  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(service_worker_id);
  if (sw_registration) {
    HasMainFrameProviderHost(
        sw_registration->pattern().GetOrigin(),
        base::Bind(&BackgroundSyncMetrics::RecordEventResult,
                   status_code == SERVICE_WORKER_OK));
  }

  if (registration->sync_state() ==
      BackgroundSyncState::REREGISTERED_WHILE_FIRING) {
    registration->set_sync_state(BackgroundSyncState::PENDING);
    registration->set_num_attempts(0);
  } else if (status_code != SERVICE_WORKER_OK) {  // Sync failed
    bool can_retry =
        registration->num_attempts() < parameters_->max_sync_attempts;
    if (registration->sync_state() ==
        BackgroundSyncState::UNREGISTERED_WHILE_FIRING) {
      registration->set_sync_state(can_retry ? BackgroundSyncState::UNREGISTERED
                                             : BackgroundSyncState::FAILED);
      registration->RunFinishedCallbacks();
    } else if (can_retry) {
      registration->set_sync_state(BackgroundSyncState::PENDING);
      registration->set_delay_until(clock_->Now() +
                                    parameters_->initial_retry_delay *
                                        pow(parameters_->retry_delay_factor,
                                            registration->num_attempts() - 1));
    } else {
      registration->set_sync_state(BackgroundSyncState::FAILED);
      registration->RunFinishedCallbacks();
    }
  } else {  // Sync succeeded
    registration->set_sync_state(BackgroundSyncState::SUCCESS);
    registration->RunFinishedCallbacks();
  }

  if (registration->HasCompleted()) {
    RegistrationKey key(*registration);
    RefCountedRegistration* active_registration =
        LookupActiveRegistration(service_worker_id, key);
    if (active_registration &&
        active_registration->value()->id() == registration->id()) {
      RemoveActiveRegistration(service_worker_id, key);
    }
  }

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback));
    return;
  }

  StoreRegistrations(
      service_worker_id,
      base::Bind(&BackgroundSyncManager::EventCompleteDidStore,
                 weak_ptr_factory_.GetWeakPtr(), service_worker_id, callback));
}

void BackgroundSyncManager::EventCompleteDidStore(
    int64_t service_worker_id,
    const base::Closure& callback,
    ServiceWorkerStatusCode status_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status_code == SERVICE_WORKER_ERROR_NOT_FOUND) {
    // The registration is gone.
    active_registrations_.erase(service_worker_id);
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

  // Fire any ready events and call RunInBackground if anything is waiting.
  FireReadyEvents();

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
    int64_t sw_registration_id,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The backend (ServiceWorkerStorage) will delete the data, so just delete the
  // memory representation here.
  active_registrations_.erase(sw_registration_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback));
}

void BackgroundSyncManager::OnStorageWipedImpl(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  active_registrations_.clear();
  disabled_ = false;
  InitImpl(callback);
}

void BackgroundSyncManager::OnNetworkChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  FireReadyEvents();
}

void BackgroundSyncManager::SetMaxSyncAttemptsImpl(
    int max_attempts,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  parameters_->max_sync_attempts = max_attempts;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

// TODO(jkarlin): Figure out how to pass scoped_ptrs with this.
template <typename CallbackT, typename... Params>
void BackgroundSyncManager::CompleteOperationCallback(const CallbackT& callback,
                                                      Params... parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  callback.Run(parameters...);
  op_scheduler_.CompleteOperationAndRunNext();
}

void BackgroundSyncManager::CompleteStatusAndRegistrationCallback(
    StatusAndRegistrationCallback callback,
    BackgroundSyncStatus status,
    scoped_ptr<BackgroundSyncRegistrationHandle> registration_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  callback.Run(status, std::move(registration_handle));
  op_scheduler_.CompleteOperationAndRunNext();
}

void BackgroundSyncManager::CompleteStatusAndRegistrationsCallback(
    StatusAndRegistrationsCallback callback,
    BackgroundSyncStatus status,
    scoped_ptr<ScopedVector<BackgroundSyncRegistrationHandle>>
        registration_handles) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  callback.Run(status, std::move(registration_handles));
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

  return base::Bind(
      &BackgroundSyncManager::CompleteStatusAndRegistrationCallback,
      weak_ptr_factory_.GetWeakPtr(), callback);
}

BackgroundSyncManager::StatusAndRegistrationsCallback
BackgroundSyncManager::MakeStatusAndRegistrationsCompletion(
    const StatusAndRegistrationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return base::Bind(
      &BackgroundSyncManager::CompleteStatusAndRegistrationsCallback,
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
