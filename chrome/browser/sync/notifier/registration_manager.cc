// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/registration_manager.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "base/rand_util.h"
#include "chrome/browser/sync/notifier/invalidation_util.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "google/cacheinvalidation/v2/invalidation-client.h"

namespace sync_notifier {

RegistrationManager::PendingRegistrationInfo::PendingRegistrationInfo() {}

RegistrationManager::RegistrationStatus::RegistrationStatus()
    : model_type(syncable::UNSPECIFIED),
      registration_manager(NULL),
      enabled(true),
      state(invalidation::InvalidationListener::UNREGISTERED) {}

RegistrationManager::RegistrationStatus::~RegistrationStatus() {}

void RegistrationManager::RegistrationStatus::DoRegister() {
  DCHECK_NE(model_type, syncable::UNSPECIFIED);
  DCHECK(registration_manager);
  CHECK(enabled);
  // We might be called explicitly, so stop the timer manually and
  // reset the delay.
  registration_timer.Stop();
  delay = base::TimeDelta();
  registration_manager->DoRegisterType(model_type);
  DCHECK(!last_registration_request.is_null());
}

void RegistrationManager::RegistrationStatus::Disable() {
  enabled = false;
  state = invalidation::InvalidationListener::UNREGISTERED;
  registration_timer.Stop();
  delay = base::TimeDelta();
}

const int RegistrationManager::kInitialRegistrationDelaySeconds = 5;
const int RegistrationManager::kRegistrationDelayExponent = 2;
const double RegistrationManager::kRegistrationDelayMaxJitter = 0.5;
const int RegistrationManager::kMinRegistrationDelaySeconds = 1;
// 1 hour.
const int RegistrationManager::kMaxRegistrationDelaySeconds = 60 * 60;

RegistrationManager::RegistrationManager(
    invalidation::InvalidationClient* invalidation_client)
    : invalidation_client_(invalidation_client) {
  DCHECK(invalidation_client_);
  // Initialize statuses.
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
    RegistrationStatus* status = &registration_statuses_[model_type];
    status->model_type = model_type;
    status->registration_manager = this;
  }
}

RegistrationManager::~RegistrationManager() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

void RegistrationManager::SetRegisteredTypes(
    const syncable::ModelTypeSet& types) {
  DCHECK(non_thread_safe_.CalledOnValidThread());

  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
    if (types.count(model_type) > 0) {
      if (!IsTypeRegistered(model_type)) {
        TryRegisterType(model_type, false /* is_retry */);
      }
    } else {
      if (IsTypeRegistered(model_type)) {
        UnregisterType(model_type);
      }
    }
  }
}

void RegistrationManager::MarkRegistrationLost(
    syncable::ModelType model_type) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  RegistrationStatus* status = &registration_statuses_[model_type];
  if (!status->enabled) {
    return;
  }
  status->state = invalidation::InvalidationListener::UNREGISTERED;
  bool is_retry = !status->last_registration_request.is_null();
  TryRegisterType(model_type, is_retry);
}

void RegistrationManager::MarkAllRegistrationsLost() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
    if (IsTypeRegistered(model_type)) {
      MarkRegistrationLost(model_type);
    }
  }
}

void RegistrationManager::DisableType(syncable::ModelType model_type) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  RegistrationStatus* status = &registration_statuses_[model_type];
  LOG(INFO) << "Disabling " << syncable::ModelTypeToString(model_type);
  status->Disable();
}

syncable::ModelTypeSet RegistrationManager::GetRegisteredTypes() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  syncable::ModelTypeSet registered_types;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
    if (IsTypeRegistered(model_type)) {
      registered_types.insert(model_type);
    }
  }
  return registered_types;
}

RegistrationManager::PendingRegistrationMap
    RegistrationManager::GetPendingRegistrations() const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  PendingRegistrationMap pending_registrations;
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
    const RegistrationStatus& status = registration_statuses_[model_type];
    if (status.registration_timer.IsRunning()) {
      pending_registrations[model_type].last_registration_request =
          status.last_registration_request;
      pending_registrations[model_type].registration_attempt =
          status.last_registration_attempt;
      pending_registrations[model_type].delay = status.delay;
      pending_registrations[model_type].actual_delay =
          status.registration_timer.GetCurrentDelay();
    }
  }
  return pending_registrations;
}

void RegistrationManager::FirePendingRegistrationsForTest() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  for (int i = syncable::FIRST_REAL_MODEL_TYPE;
       i < syncable::MODEL_TYPE_COUNT; ++i) {
    syncable::ModelType model_type = syncable::ModelTypeFromInt(i);
    RegistrationStatus* status = &registration_statuses_[model_type];
    if (status->registration_timer.IsRunning()) {
      status->DoRegister();
    }
  }
}

// static
double RegistrationManager::CalculateBackoff(
    double retry_interval,
    double initial_retry_interval,
    double min_retry_interval,
    double max_retry_interval,
    double backoff_exponent,
    double jitter,
    double max_jitter) {
  // scaled_jitter lies in [-max_jitter, max_jitter].
  double scaled_jitter = jitter * max_jitter;
  double new_retry_interval =
      (retry_interval == 0.0) ?
      (initial_retry_interval * (1.0 + scaled_jitter)) :
      (retry_interval * (backoff_exponent + scaled_jitter));
  return std::max(min_retry_interval,
                  std::min(max_retry_interval, new_retry_interval));
}

double RegistrationManager::GetJitter() {
  // |jitter| lies in [-1.0, 1.0), which is low-biased, but only
  // barely.
  //
  // TODO(akalin): Fix the bias.
  return 2.0 * base::RandDouble() - 1.0;
}

void RegistrationManager::TryRegisterType(syncable::ModelType model_type,
                                          bool is_retry) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  RegistrationStatus* status = &registration_statuses_[model_type];
  if (!status->enabled) {
    // Disabled, so do nothing.
    return;
  }
  status->last_registration_attempt = base::Time::Now();
  if (is_retry) {
    // If we're a retry, we must have tried at least once before.
    DCHECK(!status->last_registration_request.is_null());
    // delay = max(0, (now - last request) + next_delay)
    status->delay =
        (status->last_registration_request -
         status->last_registration_attempt) +
        status->next_delay;
    base::TimeDelta delay =
        (status->delay <= base::TimeDelta()) ?
        base::TimeDelta() : status->delay;
    DVLOG(2) << "Registering "
             << syncable::ModelTypeToString(model_type) << " in "
             << delay.InMilliseconds() << " ms";
    status->registration_timer.Stop();
    status->registration_timer.Start(FROM_HERE,
        delay, status, &RegistrationManager::RegistrationStatus::DoRegister);
    double next_delay_seconds =
        CalculateBackoff(static_cast<double>(status->next_delay.InSeconds()),
                         kInitialRegistrationDelaySeconds,
                         kMinRegistrationDelaySeconds,
                         kMaxRegistrationDelaySeconds,
                         kRegistrationDelayExponent,
                         GetJitter(),
                         kRegistrationDelayMaxJitter);
    status->next_delay =
        base::TimeDelta::FromSeconds(static_cast<int64>(next_delay_seconds));
    DVLOG(2) << "New next delay for "
             << syncable::ModelTypeToString(model_type) << " is "
             << status->next_delay.InSeconds() << " seconds";
  } else {
    DVLOG(2) << "Not a retry -- registering "
             << syncable::ModelTypeToString(model_type) << " immediately";
    status->delay = base::TimeDelta();
    status->next_delay = base::TimeDelta();
    status->DoRegister();
  }
}

void RegistrationManager::DoRegisterType(syncable::ModelType model_type) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  invalidation::ObjectId object_id;
  if (!RealModelTypeToObjectId(model_type, &object_id)) {
    LOG(DFATAL) << "Invalid model type: " << model_type;
    return;
  }
  invalidation_client_->Register(object_id);
  RegistrationStatus* status = &registration_statuses_[model_type];
  status->state = invalidation::InvalidationListener::REGISTERED;
  status->last_registration_request = base::Time::Now();
}

void RegistrationManager::UnregisterType(syncable::ModelType model_type) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  invalidation::ObjectId object_id;
  if (!RealModelTypeToObjectId(model_type, &object_id)) {
    LOG(DFATAL) << "Invalid model type: " << model_type;
    return;
  }
  invalidation_client_->Unregister(object_id);
  RegistrationStatus* status = &registration_statuses_[model_type];
  status->state = invalidation::InvalidationListener::UNREGISTERED;
}

bool RegistrationManager::IsTypeRegistered(
    syncable::ModelType model_type) const {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return registration_statuses_[model_type].state ==
      invalidation::InvalidationListener::REGISTERED;
}

}  // namespace sync_notifier
