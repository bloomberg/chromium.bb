// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_manager.h"

#include <algorithm>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/background_sync/background_sync_metrics.h"
#include "content/browser/background_sync/background_sync_network_observer.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/background_sync_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_type.h"
#include "third_party/blink/public/common/service_worker/service_worker_type_converters.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

#if defined(OS_ANDROID)
#include "content/browser/android/background_sync_network_observer_android.h"
#endif

using blink::mojom::BackgroundSyncType;

namespace content {

namespace {

// The only allowed value of min_interval for one shot Background Sync
// registrations.
constexpr int kMinIntervalForOneShotSync = -1;

// The key used to index the background sync data in ServiceWorkerStorage.
const char kBackgroundSyncUserDataKey[] = "BackgroundSyncUserData";

void RecordFailureAndPostError(
    BackgroundSyncStatus status,
    BackgroundSyncManager::StatusAndRegistrationCallback callback) {
  BackgroundSyncMetrics::CountRegisterFailure(status);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status, nullptr));
}

// Returns nullptr if the browser context cannot be accessed for any reason.
BrowserContext* GetBrowserContextOnUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!service_worker_context)
    return nullptr;
  StoragePartitionImpl* storage_partition_impl =
      service_worker_context->storage_partition();
  if (!storage_partition_impl)  // may be null in tests
    return nullptr;

  return storage_partition_impl->browser_context();
}

// Returns nullptr if the controller cannot be accessed for any reason.
BackgroundSyncController* GetBackgroundSyncControllerOnUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context =
      GetBrowserContextOnUIThread(std::move(service_worker_context));
  if (!browser_context)
    return nullptr;

  return browser_context->GetBackgroundSyncController();
}

blink::mojom::PermissionStatus GetBackgroundSyncPermissionOnUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BrowserContext* browser_context =
      GetBrowserContextOnUIThread(std::move(service_worker_context));
  if (!browser_context)
    return blink::mojom::PermissionStatus::DENIED;

  PermissionController* permission_controller =
      BrowserContext::GetPermissionController(browser_context);
  DCHECK(permission_controller);

  // The requesting origin always matches the embedding origin.
  GURL origin_url = origin.GetURL();
  return permission_controller->GetPermissionStatus(
      PermissionType::BACKGROUND_SYNC, origin_url, origin_url);
}

void NotifyBackgroundSyncRegisteredOnUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    const url::Origin& origin,
    bool can_fire,
    bool is_reregistered) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncControllerOnUIThread(std::move(sw_context_wrapper));

  if (!background_sync_controller)
    return;

  background_sync_controller->NotifyBackgroundSyncRegistered(origin, can_fire,
                                                             is_reregistered);
}

void NotifyBackgroundSyncCompletedOnUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    const url::Origin& origin,
    blink::ServiceWorkerStatusCode status_code,
    int num_attempts,
    int max_attempts) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncControllerOnUIThread(std::move(sw_context_wrapper));

  if (!background_sync_controller)
    return;

  background_sync_controller->NotifyBackgroundSyncCompleted(
      origin, status_code, num_attempts, max_attempts);
}

void RunInBackgroundOnUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncControllerOnUIThread(sw_context_wrapper);
  if (background_sync_controller) {
    background_sync_controller->RunInBackground();
  }
}

std::unique_ptr<BackgroundSyncParameters> GetControllerParameters(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    std::unique_ptr<BackgroundSyncParameters> parameters) {
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

base::TimeDelta GetNextEventDelay(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    const BackgroundSyncRegistration& registration,
    const url::Origin& origin,
    std::unique_ptr<BackgroundSyncParameters> parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* background_sync_controller =
      GetBackgroundSyncControllerOnUIThread(sw_context_wrapper);

  if (!background_sync_controller)
    return base::TimeDelta::Max();

  return background_sync_controller->GetNextEventDelay(
      origin, registration.options()->min_interval, registration.num_attempts(),
      registration.sync_type(), parameters.get());
}

void OnSyncEventFinished(scoped_refptr<ServiceWorkerVersion> active_version,
                         int request_id,
                         ServiceWorkerVersion::StatusCallback callback,
                         blink::mojom::ServiceWorkerEventStatus status) {
  if (!active_version->FinishRequest(
          request_id,
          status == blink::mojom::ServiceWorkerEventStatus::COMPLETED)) {
    return;
  }
  std::move(callback).Run(
      mojo::ConvertTo<blink::ServiceWorkerStatusCode>(status));
}

void DidStartWorkerForSyncEvent(
    base::OnceCallback<void(ServiceWorkerVersion::StatusCallback)> task,
    ServiceWorkerVersion::StatusCallback callback,
    blink::ServiceWorkerStatusCode start_worker_status) {
  if (start_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(start_worker_status);
    return;
  }
  std::move(task).Run(std::move(callback));
}

BackgroundSyncType GetBackgroundSyncType(
    const blink::mojom::SyncRegistrationOptions& options) {
  return options.min_interval >= 0 ? BackgroundSyncType::PERIODIC
                                   : BackgroundSyncType::ONE_SHOT;
}

std::string GetEventStatusString(blink::ServiceWorkerStatusCode status_code) {
  // The |status_code| is derived from blink::mojom::ServiceWorkerEventStatus.
  switch (status_code) {
    case blink::ServiceWorkerStatusCode::kOk:
      return "succeeded";
    case blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected:
      return "waitUntil rejected";
    case blink::ServiceWorkerStatusCode::kErrorAbort:
      return "aborted";
    case blink::ServiceWorkerStatusCode::kErrorTimeout:
      return "timeout";
    default:
      NOTREACHED();
      return "unknown error";
  }
}

// This prevents the browser process from shutting down when the last browser
// window is closed and there are one-shot Background Sync events ready to fire.
std::unique_ptr<BackgroundSyncController::BackgroundSyncEventKeepAlive>
CreateBackgroundSyncEventKeepAliveOnUIThread(
    scoped_refptr<ServiceWorkerContextWrapper> sw_context_wrapper,
    const blink::mojom::BackgroundSyncRegistrationInfo& registration_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncController* controller =
      GetBackgroundSyncControllerOnUIThread(sw_context_wrapper);
  if (!controller ||
      registration_info.sync_type != BackgroundSyncType::ONE_SHOT) {
    return nullptr;
  }

  return controller->CreateBackgroundSyncEventKeepAlive();
}

}  // namespace

BackgroundSyncManager::BackgroundSyncRegistrations::
    BackgroundSyncRegistrations() = default;

BackgroundSyncManager::BackgroundSyncRegistrations::BackgroundSyncRegistrations(
    const BackgroundSyncRegistrations& other) = default;

BackgroundSyncManager::BackgroundSyncRegistrations::
    ~BackgroundSyncRegistrations() = default;

// static
std::unique_ptr<BackgroundSyncManager> BackgroundSyncManager::Create(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<DevToolsBackgroundServicesContext> devtools_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncManager* sync_manager = new BackgroundSyncManager(
      std::move(service_worker_context), std::move(devtools_context));
  sync_manager->Init();
  return base::WrapUnique(sync_manager);
}

BackgroundSyncManager::~BackgroundSyncManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->RemoveObserver(this);
}

void BackgroundSyncManager::Register(
    int64_t sw_registration_id,
    blink::mojom::SyncRegistrationOptions options,
    StatusAndRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              std::move(callback));
    return;
  }

  if (options.min_interval < 0 &&
      options.min_interval != kMinIntervalForOneShotSync) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NOT_ALLOWED,
                              std::move(callback));
    return;
  }

  op_scheduler_.ScheduleOperation(
      CacheStorageSchedulerOp::kBackgroundSync,
      base::BindOnce(&BackgroundSyncManager::RegisterCheckIfHasMainFrame,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     std::move(options),
                     op_scheduler_.WrapCallbackToRunNext(std::move(callback))));
}

void BackgroundSyncManager::DidResolveRegistration(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_)
    return;
  op_scheduler_.ScheduleOperation(
      CacheStorageSchedulerOp::kBackgroundSync,
      base::BindOnce(&BackgroundSyncManager::DidResolveRegistrationImpl,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(registration_info)));
}

void BackgroundSyncManager::GetRegistrations(
    int64_t sw_registration_id,
    StatusAndRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback), BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
            std::vector<std::unique_ptr<BackgroundSyncRegistration>>()));
    return;
  }

  op_scheduler_.ScheduleOperation(
      CacheStorageSchedulerOp::kBackgroundSync,
      base::BindOnce(&BackgroundSyncManager::GetRegistrationsImpl,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     op_scheduler_.WrapCallbackToRunNext(std::move(callback))));
}

void BackgroundSyncManager::OnRegistrationDeleted(int64_t sw_registration_id,
                                                  const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Operations already in the queue will either fail when they write to storage
  // or return stale results based on registrations loaded in memory. This is
  // inconsequential since the service worker is gone.
  op_scheduler_.ScheduleOperation(
      CacheStorageSchedulerOp::kBackgroundSync,
      base::BindOnce(&BackgroundSyncManager::OnRegistrationDeletedImpl,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     MakeEmptyCompletion()));
}

void BackgroundSyncManager::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Operations already in the queue will either fail when they write to storage
  // or return stale results based on registrations loaded in memory. This is
  // inconsequential since the service workers are gone.
  op_scheduler_.ScheduleOperation(
      CacheStorageSchedulerOp::kBackgroundSync,
      base::BindOnce(&BackgroundSyncManager::OnStorageWipedImpl,
                     weak_ptr_factory_.GetWeakPtr(), MakeEmptyCompletion()));
}

void BackgroundSyncManager::SetMaxSyncAttemptsForTesting(int max_attempts) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  op_scheduler_.ScheduleOperation(
      CacheStorageSchedulerOp::kBackgroundSync,
      base::BindOnce(&BackgroundSyncManager::SetMaxSyncAttemptsImpl,
                     weak_ptr_factory_.GetWeakPtr(), max_attempts,
                     MakeEmptyCompletion()));
}

void BackgroundSyncManager::EmulateDispatchSyncEvent(
    const std::string& tag,
    scoped_refptr<ServiceWorkerVersion> active_version,
    bool last_chance,
    ServiceWorkerVersion::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blink::ServiceWorkerStatusCode code = CanEmulateSyncEvent(active_version);
  if (code != blink::ServiceWorkerStatusCode::kOk) {
    std::move(callback).Run(code);
    return;
  }

  DispatchSyncEvent(tag, std::move(active_version), last_chance,
                    std::move(callback));
}

void BackgroundSyncManager::EmulateServiceWorkerOffline(
    int64_t service_worker_id,
    bool is_offline) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Multiple DevTools sessions may want to set the same SW offline, which
  // is supposed to disable the background sync. For consistency with the
  // network stack, SW remains offline until all DevTools sessions disable
  // the offline mode.
  emulated_offline_sw_[service_worker_id] += is_offline ? 1 : -1;
  if (emulated_offline_sw_[service_worker_id] > 0)
    return;
  emulated_offline_sw_.erase(service_worker_id);
  FireReadyEvents(MakeEmptyCompletion());
}

BackgroundSyncManager::BackgroundSyncManager(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<DevToolsBackgroundServicesContext> devtools_context)
    : op_scheduler_(CacheStorageSchedulerClient::kBackgroundSync),
      service_worker_context_(std::move(service_worker_context)),
      devtools_context_(std::move(devtools_context)),
      parameters_(std::make_unique<BackgroundSyncParameters>()),
      disabled_(false),
      num_firing_registrations_(0),
      clock_(base::DefaultClock::GetInstance()),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(devtools_context_);

  service_worker_context_->AddObserver(this);

#if defined(OS_ANDROID)
  network_observer_ = std::make_unique<BackgroundSyncNetworkObserverAndroid>(
      base::BindRepeating(&BackgroundSyncManager::OnNetworkChanged,
                          weak_ptr_factory_.GetWeakPtr()));
#else
  network_observer_ = std::make_unique<BackgroundSyncNetworkObserver>(
      base::BindRepeating(&BackgroundSyncManager::OnNetworkChanged,
                          weak_ptr_factory_.GetWeakPtr()));
#endif
}

void BackgroundSyncManager::Init() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!op_scheduler_.ScheduledOperations());
  DCHECK(!disabled_);

  op_scheduler_.ScheduleOperation(
      CacheStorageSchedulerOp::kBackgroundSync,
      base::BindOnce(&BackgroundSyncManager::InitImpl,
                     weak_ptr_factory_.GetWeakPtr(), MakeEmptyCompletion()));
}

void BackgroundSyncManager::InitImpl(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&GetControllerParameters, service_worker_context_,
                     std::make_unique<BackgroundSyncParameters>(*parameters_)),
      base::BindOnce(&BackgroundSyncManager::InitDidGetControllerParameters,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundSyncManager::InitDidGetControllerParameters(
    base::OnceClosure callback,
    std::unique_ptr<BackgroundSyncParameters> updated_parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  parameters_ = std::move(updated_parameters);
  if (parameters_->disable) {
    disabled_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  GetDataFromBackend(
      kBackgroundSyncUserDataKey,
      base::BindOnce(&BackgroundSyncManager::InitDidGetDataFromBackend,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundSyncManager::InitDidGetDataFromBackend(
    base::OnceClosure callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != blink::ServiceWorkerStatusCode::kOk &&
      status != blink::ServiceWorkerStatusCode::kErrorNotFound) {
    DisableAndClearManager(std::move(callback));
    return;
  }

  for (const std::pair<int64_t, std::string>& data : user_data) {
    BackgroundSyncRegistrationsProto registrations_proto;
    if (registrations_proto.ParseFromString(data.second)) {
      BackgroundSyncRegistrations* registrations =
          &active_registrations_[data.first];
      registrations->origin =
          url::Origin::Create(GURL(registrations_proto.origin()));

      for (const auto& registration_proto :
           registrations_proto.registration()) {
        BackgroundSyncType sync_type =
            registration_proto.has_periodic_sync_options()
                ? BackgroundSyncType::PERIODIC
                : BackgroundSyncType::ONE_SHOT;
        BackgroundSyncRegistration* registration =
            &registrations
                 ->registration_map[{registration_proto.tag(), sync_type}];

        blink::mojom::SyncRegistrationOptions* options =
            registration->options();
        options->tag = registration_proto.tag();
        if (sync_type == BackgroundSyncType::PERIODIC) {
          options->min_interval =
              registration_proto.periodic_sync_options().min_interval();
          if (options->min_interval < 0) {
            DisableAndClearManager(std::move(callback));
            return;
          }
        } else {
          options->min_interval = kMinIntervalForOneShotSync;
        }

        registration->set_num_attempts(registration_proto.num_attempts());
        registration->set_delay_until(
            base::Time::FromInternalValue(registration_proto.delay_until()));
        registration->set_resolved();
      }
    }
  }

  FireReadyEvents(MakeEmptyCompletion());
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

void BackgroundSyncManager::RegisterCheckIfHasMainFrame(
    int64_t sw_registration_id,
    blink::mojom::SyncRegistrationOptions options,
    StatusAndRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  HasMainFrameProviderHost(
      url::Origin::Create(sw_registration->scope().GetOrigin()),
      base::BindOnce(&BackgroundSyncManager::RegisterDidCheckIfMainFrame,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     std::move(options), std::move(callback)));
}

void BackgroundSyncManager::RegisterDidCheckIfMainFrame(
    int64_t sw_registration_id,
    blink::mojom::SyncRegistrationOptions options,
    StatusAndRegistrationCallback callback,
    bool has_main_frame_client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!has_main_frame_client) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NOT_ALLOWED,
                              std::move(callback));
    return;
  }
  RegisterImpl(sw_registration_id, std::move(options), std::move(callback));
}

void BackgroundSyncManager::RegisterImpl(
    int64_t sw_registration_id,
    blink::mojom::SyncRegistrationOptions options,
    StatusAndRegistrationCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              std::move(callback));
    return;
  }

  if (options.tag.length() > kMaxTagLength) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NOT_ALLOWED,
                              std::move(callback));
    return;
  }

  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&GetBackgroundSyncPermissionOnUIThread,
                     service_worker_context_,
                     url::Origin::Create(sw_registration->scope().GetOrigin())),
      base::BindOnce(&BackgroundSyncManager::RegisterDidAskForPermission,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     std::move(options), std::move(callback)));
}

void BackgroundSyncManager::RegisterDidAskForPermission(
    int64_t sw_registration_id,
    blink::mojom::SyncRegistrationOptions options,
    StatusAndRegistrationCallback callback,
    blink::mojom::PermissionStatus permission_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (permission_status == blink::mojom::PermissionStatus::DENIED) {
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_PERMISSION_DENIED,
                              std::move(callback));
    return;
  }
  DCHECK(permission_status == blink::mojom::PermissionStatus::GRANTED);

  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    // The service worker was shut down in the interim.
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  BackgroundSyncRegistration* existing_registration =
      LookupActiveRegistration(blink::mojom::BackgroundSyncRegistrationInfo(
          sw_registration_id, options.tag, GetBackgroundSyncType(options)));

  url::Origin origin =
      url::Origin::Create(sw_registration->scope().GetOrigin());

  // TODO(crbug.com/925297): Record Periodic Sync metrics.
  if (GetBackgroundSyncType(options) ==
      blink::mojom::BackgroundSyncType::ONE_SHOT) {
    bool is_reregistered =
        existing_registration && existing_registration->IsFiring();
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &NotifyBackgroundSyncRegisteredOnUIThread, service_worker_context_,
            origin, /* can_fire= */ AreOptionConditionsMet(), is_reregistered));
  }

  if (existing_registration) {
    DCHECK(existing_registration->options()->Equals(options));

    BackgroundSyncMetrics::RegistrationCouldFire registration_could_fire =
        AreOptionConditionsMet()
            ? BackgroundSyncMetrics::REGISTRATION_COULD_FIRE
            : BackgroundSyncMetrics::REGISTRATION_COULD_NOT_FIRE;
    BackgroundSyncMetrics::CountRegisterSuccess(
        registration_could_fire,
        BackgroundSyncMetrics::REGISTRATION_IS_DUPLICATE);

    if (existing_registration->IsFiring()) {
      existing_registration->set_sync_state(
          blink::mojom::BackgroundSyncState::REREGISTERED_WHILE_FIRING);
    }

    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), BACKGROUND_SYNC_STATUS_OK,
                       std::make_unique<BackgroundSyncRegistration>(
                           *existing_registration)));
    return;
  }

  BackgroundSyncRegistration new_registration;

  *new_registration.options() = std::move(options);

  if (new_registration.sync_type() == BackgroundSyncType::PERIODIC) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &GetNextEventDelay, service_worker_context_, new_registration,
            origin, std::make_unique<BackgroundSyncParameters>(*parameters_)),
        base::BindOnce(&BackgroundSyncManager::RegisterDidGetDelay,
                       weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                       new_registration, std::move(callback)));
    return;
  }

  RegisterDidGetDelay(sw_registration_id, new_registration, std::move(callback),
                      base::TimeDelta());
}

void BackgroundSyncManager::RegisterDidGetDelay(
    int64_t sw_registration_id,
    BackgroundSyncRegistration new_registration,
    StatusAndRegistrationCallback callback,
    base::TimeDelta delay) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // For one-shot registrations, we let the delay_until be in the past, so that
  // an event is fired at the soonest opportune moment.
  if (new_registration.sync_type() == BackgroundSyncType::PERIODIC) {
    new_registration.set_delay_until(clock_->Now() + delay);
  }

  ServiceWorkerRegistration* sw_registration =
      service_worker_context_->GetLiveRegistration(sw_registration_id);
  if (!sw_registration || !sw_registration->active_version()) {
    // The service worker was shut down in the interim.
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_NO_SERVICE_WORKER,
                              std::move(callback));
    return;
  }

  AddActiveRegistration(
      sw_registration_id,
      url::Origin::Create(sw_registration->scope().GetOrigin()),
      new_registration);

  StoreRegistrations(
      sw_registration_id,
      base::BindOnce(&BackgroundSyncManager::RegisterDidStore,
                     weak_ptr_factory_.GetWeakPtr(), sw_registration_id,
                     new_registration, std::move(callback)));
}

void BackgroundSyncManager::DisableAndClearManager(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  disabled_ = true;

  active_registrations_.clear();

  // Delete all backend entries. The memory representation of registered syncs
  // may be out of sync with storage (e.g., due to corruption detection on
  // loading from storage), so reload the registrations from storage again.
  GetDataFromBackend(
      kBackgroundSyncUserDataKey,
      base::BindOnce(&BackgroundSyncManager::DisableAndClearDidGetRegistrations,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void BackgroundSyncManager::DisableAndClearDidGetRegistrations(
    base::OnceClosure callback,
    const std::vector<std::pair<int64_t, std::string>>& user_data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != blink::ServiceWorkerStatusCode::kOk || user_data.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(user_data.size(), std::move(callback));

  for (const auto& sw_id_and_regs : user_data) {
    service_worker_context_->ClearRegistrationUserData(
        sw_id_and_regs.first, {kBackgroundSyncUserDataKey},
        base::BindOnce(&BackgroundSyncManager::DisableAndClearManagerClearedOne,
                       weak_ptr_factory_.GetWeakPtr(), barrier_closure));
  }
}

void BackgroundSyncManager::DisableAndClearManagerClearedOne(
    base::OnceClosure barrier_closure,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The status doesn't matter at this point, there is nothing else to be done.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                std::move(barrier_closure));
}

BackgroundSyncRegistration* BackgroundSyncManager::LookupActiveRegistration(
    const blink::mojom::BackgroundSyncRegistrationInfo& registration_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = active_registrations_.find(
      registration_info.service_worker_registration_id);
  if (it == active_registrations_.end())
    return nullptr;

  BackgroundSyncRegistrations& registrations = it->second;
  DCHECK(!registrations.origin.opaque());

  auto key_and_registration_iter = registrations.registration_map.find(
      {registration_info.tag, registration_info.sync_type});
  if (key_and_registration_iter == registrations.registration_map.end())
    return nullptr;

  return &key_and_registration_iter->second;
}

void BackgroundSyncManager::StoreRegistrations(
    int64_t sw_registration_id,
    ServiceWorkerStorage::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Serialize the data.
  const BackgroundSyncRegistrations& registrations =
      active_registrations_[sw_registration_id];
  BackgroundSyncRegistrationsProto registrations_proto;
  registrations_proto.set_origin(registrations.origin.Serialize());

  for (const auto& key_and_registration : registrations.registration_map) {
    const BackgroundSyncRegistration& registration =
        key_and_registration.second;
    BackgroundSyncRegistrationProto* registration_proto =
        registrations_proto.add_registration();
    registration_proto->set_tag(registration.options()->tag);
    if (registration.options()->min_interval >= 0) {
      registration_proto->mutable_periodic_sync_options()->set_min_interval(
          registration.options()->min_interval);
    }
    registration_proto->set_num_attempts(registration.num_attempts());
    registration_proto->set_delay_until(
        registration.delay_until().ToInternalValue());
  }
  std::string serialized;
  bool success = registrations_proto.SerializeToString(&serialized);
  DCHECK(success);

  StoreDataInBackend(sw_registration_id, registrations.origin,
                     kBackgroundSyncUserDataKey, serialized,
                     std::move(callback));
}

void BackgroundSyncManager::RegisterDidStore(
    int64_t sw_registration_id,
    const BackgroundSyncRegistration& new_registration,
    StatusAndRegistrationCallback callback,
    blink::ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // The service worker registration is gone.
    active_registrations_.erase(sw_registration_id);
    RecordFailureAndPostError(BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                              std::move(callback));
    return;
  }

  if (status != blink::ServiceWorkerStatusCode::kOk) {
    BackgroundSyncMetrics::CountRegisterFailure(
        BACKGROUND_SYNC_STATUS_STORAGE_ERROR);
    DisableAndClearManager(base::BindOnce(
        std::move(callback), BACKGROUND_SYNC_STATUS_STORAGE_ERROR, nullptr));
    return;
  }

  BackgroundSyncMetrics::RegistrationCouldFire registration_could_fire =
      AreOptionConditionsMet()
          ? BackgroundSyncMetrics::REGISTRATION_COULD_FIRE
          : BackgroundSyncMetrics::REGISTRATION_COULD_NOT_FIRE;
  BackgroundSyncMetrics::CountRegisterSuccess(
      registration_could_fire,
      BackgroundSyncMetrics::REGISTRATION_IS_NOT_DUPLICATE);

  // Tell the client that the registration is ready. We won't fire it until the
  // client has resolved the registration event.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), BACKGROUND_SYNC_STATUS_OK,
                                std::make_unique<BackgroundSyncRegistration>(
                                    new_registration)));
}

void BackgroundSyncManager::DidResolveRegistrationImpl(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncRegistration* registration =
      LookupActiveRegistration(*registration_info);
  if (!registration) {
    // There might not be a registration if the client ack's a registration that
    // was a duplicate in the first place and was already firing and finished by
    // the time the client acknowledged the second registration.
    op_scheduler_.CompleteOperationAndRunNext();
    return;
  }

  registration->set_resolved();
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&CreateBackgroundSyncEventKeepAliveOnUIThread,
                     service_worker_context_, std::move(*registration_info)),
      base::BindOnce(
          &BackgroundSyncManager::ResolveRegistrationDidCreateKeepAlive,
          weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundSyncManager::ResolveRegistrationDidCreateKeepAlive(
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  FireReadyEvents(MakeEmptyCompletion(), std::move(keepalive));
  op_scheduler_.CompleteOperationAndRunNext();
}

void BackgroundSyncManager::RemoveActiveRegistration(
    const blink::mojom::BackgroundSyncRegistrationInfo& registration_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(LookupActiveRegistration(registration_info));

  BackgroundSyncRegistrations* registrations =
      &active_registrations_[registration_info.service_worker_registration_id];

  registrations->registration_map.erase(
      {registration_info.tag, registration_info.sync_type});
}

void BackgroundSyncManager::AddActiveRegistration(
    int64_t sw_registration_id,
    const url::Origin& origin,
    const BackgroundSyncRegistration& sync_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncRegistrations* registrations =
      &active_registrations_[sw_registration_id];
  registrations->origin = origin;

  BackgroundSyncType sync_type = sync_registration.sync_type();
  registrations
      ->registration_map[{sync_registration.options()->tag, sync_type}] =
      sync_registration;

  if (ShouldLogToDevTools(sync_type)) {
    devtools_context_->LogBackgroundServiceEvent(
        sw_registration_id, origin, devtools::proto::BACKGROUND_SYNC,
        /* event_name= */ "registered sync",
        /* instance_id= */ sync_registration.options()->tag,
        /* event_metadata= */ {});
  }
}

void BackgroundSyncManager::StoreDataInBackend(
    int64_t sw_registration_id,
    const url::Origin& origin,
    const std::string& backend_key,
    const std::string& data,
    ServiceWorkerStorage::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->StoreRegistrationUserData(
      sw_registration_id, origin.GetURL(), {{backend_key, data}},
      std::move(callback));
}

void BackgroundSyncManager::GetDataFromBackend(
    const std::string& backend_key,
    ServiceWorkerStorage::GetUserDataForAllRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  service_worker_context_->GetUserDataForAllRegistrations(backend_key,
                                                          std::move(callback));
}

void BackgroundSyncManager::DispatchSyncEvent(
    const std::string& tag,
    scoped_refptr<ServiceWorkerVersion> active_version,
    bool last_chance,
    ServiceWorkerVersion::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(active_version);

  if (active_version->running_status() != EmbeddedWorkerStatus::RUNNING) {
    active_version->RunAfterStartWorker(
        ServiceWorkerMetrics::EventType::SYNC,
        base::BindOnce(&DidStartWorkerForSyncEvent,
                       base::BindOnce(&BackgroundSyncManager::DispatchSyncEvent,
                                      weak_ptr_factory_.GetWeakPtr(), tag,
                                      active_version, last_chance),
                       std::move(callback)));
    return;
  }

  auto repeating_callback =
      base::AdaptCallbackForRepeating(std::move(callback));

  int request_id = active_version->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::SYNC, repeating_callback,
      parameters_->max_sync_event_duration,
      ServiceWorkerVersion::CONTINUE_ON_TIMEOUT);

  active_version->endpoint()->DispatchSyncEvent(
      tag, last_chance, parameters_->max_sync_event_duration,
      base::BindOnce(&OnSyncEventFinished, active_version, request_id,
                     std::move(repeating_callback)));

  if (devtools_context_->IsRecording(devtools::proto::BACKGROUND_SYNC)) {
    devtools_context_->LogBackgroundServiceEvent(
        active_version->registration_id(), active_version->script_origin(),
        devtools::proto::BACKGROUND_SYNC,
        /* event_name= */ "dispatched sync event",
        /* instance_id= */ tag,
        /* event_metadata= */
        {{"last chance", last_chance ? "yes" : "no"}});
  }
}

void BackgroundSyncManager::ScheduleDelayedTask(base::OnceClosure callback,
                                                base::TimeDelta delay) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, std::move(callback), delay);
}

void BackgroundSyncManager::HasMainFrameProviderHost(const url::Origin& origin,
                                                     BoolCallback callback) {
  service_worker_context_->HasMainFrameProviderHost(origin.GetURL(),
                                                    std::move(callback));
}

void BackgroundSyncManager::GetRegistrationsImpl(
    int64_t sw_registration_id,
    StatusAndRegistrationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<std::unique_ptr<BackgroundSyncRegistration>> out_registrations;

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  BACKGROUND_SYNC_STATUS_STORAGE_ERROR,
                                  std::move(out_registrations)));
    return;
  }

  auto it = active_registrations_.find(sw_registration_id);

  if (it != active_registrations_.end()) {
    const BackgroundSyncRegistrations& registrations = it->second;
    for (const auto& key_and_registration : registrations.registration_map) {
      const BackgroundSyncRegistration& registration =
          key_and_registration.second;
      out_registrations.push_back(
          std::make_unique<BackgroundSyncRegistration>(registration));
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), BACKGROUND_SYNC_STATUS_OK,
                                std::move(out_registrations)));
}

bool BackgroundSyncManager::AreOptionConditionsMet() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return network_observer_->NetworkSufficient();
}

bool BackgroundSyncManager::IsRegistrationReadyToFire(
    const BackgroundSyncRegistration& registration,
    int64_t service_worker_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Don't fire the registration if the client hasn't yet resolved its
  // registration promise.
  if (!registration.resolved())
    return false;

  if (registration.sync_state() != blink::mojom::BackgroundSyncState::PENDING)
    return false;

  if (clock_->Now() < registration.delay_until())
    return false;

  if (base::ContainsKey(emulated_offline_sw_, service_worker_id))
    return false;

  return AreOptionConditionsMet();
}

base::TimeDelta BackgroundSyncManager::GetSoonestWakeupDelta(
    BackgroundSyncType sync_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::TimeDelta soonest_wakeup_delta = base::TimeDelta::Max();
  for (const auto& sw_reg_id_and_registrations : active_registrations_) {
    for (const auto& key_and_registration :
         sw_reg_id_and_registrations.second.registration_map) {
      const BackgroundSyncRegistration& registration =
          key_and_registration.second;
      if (registration.sync_type() != sync_type)
        continue;
      if (registration.sync_state() ==
          blink::mojom::BackgroundSyncState::PENDING) {
        if (clock_->Now() >= registration.delay_until()) {
          soonest_wakeup_delta = base::TimeDelta();
          break;
        } else {
          base::TimeDelta delay_delta =
              registration.delay_until() - clock_->Now();
          soonest_wakeup_delta = std::min(delay_delta, soonest_wakeup_delta);
        }
      }
    }
  }

  // If the browser is closed while firing events, the browser needs a task to
  // wake it back up and try again.
  if (sync_type == BackgroundSyncType::ONE_SHOT &&
      num_firing_registrations_ > 0 &&
      soonest_wakeup_delta > parameters_->min_sync_recovery_time) {
    soonest_wakeup_delta = parameters_->min_sync_recovery_time;
  }

  return soonest_wakeup_delta;
}

void BackgroundSyncManager::RunInBackgroundIfNecessary() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::TimeDelta soonest_wakeup_delta =
      std::min(GetSoonestWakeupDelta(BackgroundSyncType::ONE_SHOT),
               GetSoonestWakeupDelta(BackgroundSyncType::PERIODIC));

  // Try firing again after the wakeup delta.
  if (!soonest_wakeup_delta.is_max() && !soonest_wakeup_delta.is_zero()) {
    delayed_sync_task_.Reset(
        base::BindOnce(&BackgroundSyncManager::FireReadyEvents,
                       weak_ptr_factory_.GetWeakPtr(), MakeEmptyCompletion(),
                       /* keepalive= */ nullptr));
    ScheduleDelayedTask(delayed_sync_task_.callback(), soonest_wakeup_delta);
  }

  // In case the browser closes (or to prevent it from closing), call
  // RunInBackground to wake up the browser at the wakeup delta.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(RunInBackgroundOnUIThread, service_worker_context_));
}

void BackgroundSyncManager::FireReadyEvents(
    base::OnceClosure callback,
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_)
    return;

  op_scheduler_.ScheduleOperation(
      CacheStorageSchedulerOp::kBackgroundSync,
      base::BindOnce(&BackgroundSyncManager::FireReadyEventsImpl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(keepalive)));
}

void BackgroundSyncManager::FireReadyEventsImpl(
    base::OnceClosure callback,
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  // Find the registrations that are ready to run.
  // TODO(crbug.com/925297): Periodically re-evaluate suspended Periodic Sync
  // registrations.
  std::vector<blink::mojom::BackgroundSyncRegistrationInfoPtr> to_fire;

  for (auto& sw_reg_id_and_registrations : active_registrations_) {
    const int64_t service_worker_registration_id =
        sw_reg_id_and_registrations.first;
    for (auto& key_and_registration :
         sw_reg_id_and_registrations.second.registration_map) {
      BackgroundSyncRegistration* registration = &key_and_registration.second;

      if (IsRegistrationReadyToFire(*registration,
                                    service_worker_registration_id)) {
        to_fire.emplace_back(blink::mojom::BackgroundSyncRegistrationInfo::New(
            service_worker_registration_id,
            /* tag= */ key_and_registration.first.first,
            /* sync_type= */ key_and_registration.first.second));

        // The state change is not saved to persistent storage because
        // if the sync event is killed mid-sync then it should return to
        // SYNC_STATE_PENDING.
        registration->set_sync_state(blink::mojom::BackgroundSyncState::FIRING);
      }
    }
  }

  if (to_fire.empty()) {
    RunInBackgroundIfNecessary();
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();

  // Fire the sync event of the ready registrations and run |callback| once
  // they're all done.
  base::RepeatingClosure events_fired_barrier_closure = base::BarrierClosure(
      to_fire.size(),
      base::BindOnce(&BackgroundSyncManager::FireReadyEventsAllEventsFiring,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  // Record the total time taken after all events have run to completion.
  base::RepeatingClosure events_completed_barrier_closure =
      base::BarrierClosure(to_fire.size(),
                           base::BindOnce(&OnAllSyncEventsCompleted, start_time,
                                          to_fire.size()));

  for (auto& registration_info : to_fire) {
    const BackgroundSyncRegistration* registration =
        LookupActiveRegistration(*registration_info);
    DCHECK(registration);

    int64_t service_worker_registration_id =
        registration_info->service_worker_registration_id;
    service_worker_context_->FindReadyRegistrationForId(
        service_worker_registration_id,
        active_registrations_[service_worker_registration_id].origin.GetURL(),
        base::BindOnce(
            &BackgroundSyncManager::FireReadyEventsDidFindRegistration,
            weak_ptr_factory_.GetWeakPtr(), std::move(registration_info),
            std::move(keepalive), events_fired_barrier_closure,
            events_completed_barrier_closure));
  }
}

void BackgroundSyncManager::FireReadyEventsDidFindRegistration(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive,
    base::OnceClosure event_fired_callback,
    base::OnceClosure event_completed_callback,
    blink::ServiceWorkerStatusCode service_worker_status,
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncRegistration* registration =
      LookupActiveRegistration(*registration_info);

  if (service_worker_status != blink::ServiceWorkerStatusCode::kOk) {
    if (registration)
      registration->set_sync_state(blink::mojom::BackgroundSyncState::PENDING);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(event_fired_callback));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(event_completed_callback));
    return;
  }

  DCHECK_EQ(registration_info->service_worker_registration_id,
            service_worker_registration->id());
  DCHECK(registration);

  // Don't dispatch a sync event if the sync is periodic.
  // TODO(crbug.com/925297): Remove this code when we've added the logic to
  // dispatch periodic sync events.
  if (registration &&
      registration_info->sync_type == BackgroundSyncType::PERIODIC) {
    RemoveActiveRegistration(*registration_info);
    StoreRegistrations(registration_info->service_worker_registration_id,
                       base::DoNothing());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(event_fired_callback));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(event_completed_callback));
    return;
  }

  // The connectivity was lost before dispatching the sync event, so there is
  // no point in going through with it.
  if (!AreOptionConditionsMet()) {
    registration->set_sync_state(blink::mojom::BackgroundSyncState::PENDING);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(event_fired_callback));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, std::move(event_completed_callback));
    return;
  }

  num_firing_registrations_ += 1;

  const bool last_chance =
      registration->num_attempts() == parameters_->max_sync_attempts - 1;

  HasMainFrameProviderHost(
      url::Origin::Create(service_worker_registration->scope().GetOrigin()),
      base::BindOnce(&BackgroundSyncMetrics::RecordEventStarted));

  DispatchSyncEvent(
      registration->options()->tag,
      service_worker_registration->active_version(), last_chance,
      base::BindOnce(
          &BackgroundSyncManager::EventComplete, weak_ptr_factory_.GetWeakPtr(),
          service_worker_registration, std::move(registration_info),
          std::move(keepalive), std::move(event_completed_callback)));

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, std::move(event_fired_callback));
}

void BackgroundSyncManager::FireReadyEventsAllEventsFiring(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunInBackgroundIfNecessary();
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

// |service_worker_registration| is just to keep the registration alive
// while the event is firing.
void BackgroundSyncManager::EventComplete(
    scoped_refptr<ServiceWorkerRegistration> service_worker_registration,
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive,
    base::OnceClosure callback,
    blink::ServiceWorkerStatusCode status_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  // The event ran to completion, we should count it, no matter what happens
  // from here.
  url::Origin origin =
      url::Origin::Create(service_worker_registration->scope().GetOrigin());
  HasMainFrameProviderHost(
      origin,
      base::BindOnce(&BackgroundSyncMetrics::RecordEventResult,
                     status_code == blink::ServiceWorkerStatusCode::kOk));

  op_scheduler_.ScheduleOperation(
      CacheStorageSchedulerOp::kBackgroundSync,
      base::BindOnce(&BackgroundSyncManager::EventCompleteImpl,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(registration_info), std::move(keepalive),
                     status_code, origin,
                     op_scheduler_.WrapCallbackToRunNext(std::move(callback))));
}

void BackgroundSyncManager::EventCompleteImpl(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
    std::unique_ptr<BackgroundSyncEventKeepAlive> keepalive,
    blink::ServiceWorkerStatusCode status_code,
    const url::Origin& origin,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (disabled_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  num_firing_registrations_ -= 1;

  BackgroundSyncRegistration* registration =
      LookupActiveRegistration(*registration_info);
  if (!registration) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }
  DCHECK_NE(blink::mojom::BackgroundSyncState::PENDING,
            registration->sync_state());

  // It's important to update |num_attempts| before we update |delay_until|.
  registration->set_num_attempts(registration->num_attempts() + 1);
  if ((registration->sync_type() == BackgroundSyncType::PERIODIC &&
       registration->num_attempts() == parameters_->max_sync_attempts) ||
      (registration->sync_state() ==
       blink::mojom::BackgroundSyncState::REREGISTERED_WHILE_FIRING)) {
    registration->set_num_attempts(0);
  }

  // If |delay_until| needs to be updated, get updated delay.
  bool succeeded = status_code == blink::ServiceWorkerStatusCode::kOk;
  if (registration->sync_type() == BackgroundSyncType::PERIODIC ||
      (!succeeded &&
       registration->num_attempts() < parameters_->max_sync_attempts)) {
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &GetNextEventDelay, service_worker_context_, *registration, origin,
            std::make_unique<BackgroundSyncParameters>(*parameters_)),
        base::BindOnce(&BackgroundSyncManager::EventCompleteDidGetDelay,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(registration_info), status_code, origin,
                       std::move(callback)));
    return;
  }

  EventCompleteDidGetDelay(std::move(registration_info), status_code, origin,
                           std::move(callback), base::TimeDelta::Max());
}

void BackgroundSyncManager::EventCompleteDidGetDelay(
    blink::mojom::BackgroundSyncRegistrationInfoPtr registration_info,
    blink::ServiceWorkerStatusCode status_code,
    const url::Origin& origin,
    base::OnceClosure callback,
    base::TimeDelta delay) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  BackgroundSyncRegistration* registration =
      LookupActiveRegistration(*registration_info);
  if (!registration) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  bool succeeded = status_code == blink::ServiceWorkerStatusCode::kOk;
  bool can_retry =
      registration->num_attempts() < parameters_->max_sync_attempts;

  bool registration_completed = true;
  if (registration->sync_state() ==
      blink::mojom::BackgroundSyncState::REREGISTERED_WHILE_FIRING) {
    registration->set_sync_state(blink::mojom::BackgroundSyncState::PENDING);
    registration->set_num_attempts(0);
    registration_completed = false;
    if (ShouldLogToDevTools(registration->sync_type())) {
      devtools_context_->LogBackgroundServiceEvent(
          registration_info->service_worker_registration_id, origin,
          devtools::proto::BACKGROUND_SYNC,
          /* event_name= */ "sync event reregistered",
          /* instance_id= */ registration_info->tag,
          /* event_metadata= */ {});
    }
  } else if ((!succeeded && can_retry) ||
             registration->sync_type() == BackgroundSyncType::PERIODIC) {
    registration->set_sync_state(blink::mojom::BackgroundSyncState::PENDING);
    registration_completed = false;
    registration->set_delay_until(clock_->Now() + delay);

    if (ShouldLogToDevTools(registration->sync_type())) {
      std::string delay_ms = delay.is_max()
                                 ? "infinite"
                                 : base::NumberToString(delay.InMilliseconds());
      devtools_context_->LogBackgroundServiceEvent(
          registration_info->service_worker_registration_id, origin,
          devtools::proto::BACKGROUND_SYNC,
          /* event_name= */ "sync event failed",
          /* instance_id= */ registration_info->tag,
          {{"next attempt delay (ms)", delay_ms},
           {"failure reason", GetEventStatusString(status_code)}});
    }
  }

  if (registration_completed) {
    BackgroundSyncMetrics::RecordRegistrationComplete(
        succeeded, registration->num_attempts());

    if (ShouldLogToDevTools(registration->sync_type())) {
      devtools_context_->LogBackgroundServiceEvent(
          registration_info->service_worker_registration_id, origin,
          devtools::proto::BACKGROUND_SYNC,
          /* event_name= */ "sync complete",
          /* instance_id= */ registration_info->tag,
          {{"status", GetEventStatusString(status_code)}});
    }

    if (registration_info->sync_type ==
        blink::mojom::BackgroundSyncType::ONE_SHOT) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&NotifyBackgroundSyncCompletedOnUIThread,
                         service_worker_context_, origin, status_code,
                         registration->num_attempts(),
                         parameters_->max_sync_attempts));
    }

    RemoveActiveRegistration(*registration_info);
  }

  StoreRegistrations(
      registration_info->service_worker_registration_id,
      base::BindOnce(&BackgroundSyncManager::EventCompleteDidStore,
                     weak_ptr_factory_.GetWeakPtr(),
                     registration_info->service_worker_registration_id,
                     std::move(callback)));
}

void BackgroundSyncManager::EventCompleteDidStore(
    int64_t service_worker_id,
    base::OnceClosure callback,
    blink::ServiceWorkerStatusCode status_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status_code == blink::ServiceWorkerStatusCode::kErrorNotFound) {
    // The registration is gone.
    active_registrations_.erase(service_worker_id);
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
    return;
  }

  if (status_code != blink::ServiceWorkerStatusCode::kOk) {
    DisableAndClearManager(std::move(callback));
    return;
  }

  // Fire any ready events and call RunInBackground if anything is waiting.
  FireReadyEvents(MakeEmptyCompletion());

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
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
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // The backend (ServiceWorkerStorage) will delete the data, so just delete the
  // memory representation here.
  active_registrations_.erase(sw_registration_id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

void BackgroundSyncManager::OnStorageWipedImpl(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  active_registrations_.clear();
  disabled_ = false;
  InitImpl(std::move(callback));
}

void BackgroundSyncManager::OnNetworkChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  FireReadyEvents(MakeEmptyCompletion());
}

void BackgroundSyncManager::SetMaxSyncAttemptsImpl(int max_attempts,
                                                   base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  parameters_->max_sync_attempts = max_attempts;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(callback));
}

base::OnceClosure BackgroundSyncManager::MakeEmptyCompletion() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return op_scheduler_.WrapCallbackToRunNext(base::DoNothing::Once());
}

blink::ServiceWorkerStatusCode BackgroundSyncManager::CanEmulateSyncEvent(
    scoped_refptr<ServiceWorkerVersion> active_version) {
  if (!active_version)
    return blink::ServiceWorkerStatusCode::kErrorAbort;
  if (!network_observer_->NetworkSufficient())
    return blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected;
  int64_t registration_id = active_version->registration_id();
  if (base::ContainsKey(emulated_offline_sw_, registration_id))
    return blink::ServiceWorkerStatusCode::kErrorEventWaitUntilRejected;
  return blink::ServiceWorkerStatusCode::kOk;
}

bool BackgroundSyncManager::ShouldLogToDevTools(BackgroundSyncType sync_type) {
  return sync_type == BackgroundSyncType::ONE_SHOT &&
         devtools_context_->IsRecording(devtools::proto::BACKGROUND_SYNC);
}

}  // namespace content
