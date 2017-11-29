// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_synchronizer.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

namespace {

const int64_t kTimeBetweenEachCommandMs = 200;

}  // namespace

BleSynchronizer::BleSynchronizer(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : bluetooth_adapter_(bluetooth_adapter),
      timer_(base::MakeUnique<base::OneShotTimer>()),
      clock_(base::MakeUnique<base::DefaultClock>()),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {}

BleSynchronizer::~BleSynchronizer() = default;

void BleSynchronizer::ProcessQueue() {
  if (current_command_ || command_queue().empty())
    return;

  // If the timer is already running, there is nothing to do until the timer
  // fires.
  if (timer_->IsRunning())
    return;

  base::Time current_timestamp = clock_->Now();
  base::TimeDelta time_since_last_command_ended =
      current_timestamp - last_command_end_timestamp_;

  // If we have finished a Bluetooth command in the last
  // kTimeBetweenEachCommandMs milliseconds, wait. Sending commands too
  // frequently can cause race conditions. See crbug.com/760792.
  if (!last_command_end_timestamp_.is_null() &&
      time_since_last_command_ended <
          base::TimeDelta::FromMilliseconds(kTimeBetweenEachCommandMs)) {
    timer_->Start(FROM_HERE,
                  base::TimeDelta::FromMilliseconds(kTimeBetweenEachCommandMs),
                  base::Bind(&BleSynchronizer::ProcessQueue,
                             weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  current_command_ = std::move(command_queue().front());
  command_queue().pop_front();

  switch (current_command_->command_type) {
    case CommandType::REGISTER_ADVERTISEMENT: {
      RegisterArgs* register_args = current_command_->register_args.get();
      DCHECK(register_args);
      bluetooth_adapter_->RegisterAdvertisement(
          std::move(register_args->advertisement_data),
          base::Bind(&BleSynchronizer::OnAdvertisementRegistered,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BleSynchronizer::OnErrorRegisteringAdvertisement,
                     weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    case CommandType::UNREGISTER_ADVERTISEMENT: {
      UnregisterArgs* unregister_args = current_command_->unregister_args.get();
      DCHECK(unregister_args);
      unregister_args->advertisement->Unregister(
          base::Bind(&BleSynchronizer::OnAdvertisementUnregistered,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BleSynchronizer::OnErrorUnregisteringAdvertisement,
                     weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    case CommandType::START_DISCOVERY: {
      StartDiscoveryArgs* start_discovery_args =
          current_command_->start_discovery_args.get();
      DCHECK(start_discovery_args);

      // Note: Ideally, we would use a filter for only LE devices here. However,
      // using a filter here triggers a bug in some kernel implementations which
      // causes LE scanning to toggle rapidly on and off. This can cause race
      // conditions which result in Bluetooth bugs. See crbug.com/759090.
      // TODO(mcchou): Once these issues have been resolved, add the filter
      // back. See crbug.com/759091.
      bluetooth_adapter_->StartDiscoverySession(
          base::Bind(&BleSynchronizer::OnDiscoverySessionStarted,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BleSynchronizer::OnErrorStartingDiscoverySession,
                     weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    case CommandType::STOP_DISCOVERY: {
      StopDiscoveryArgs* stop_discovery_args =
          current_command_->stop_discovery_args.get();
      DCHECK(stop_discovery_args);

      // If the discovery session has been deleted, there is nothing to stop.
      if (!stop_discovery_args->discovery_session.get()) {
        PA_LOG(WARNING) << "Could not process \"stop discovery\" command "
                        << "because the DiscoverySession object passed is "
                        << "deleted.";
        current_command_.reset();
        ProcessQueue();
        break;
      }

      stop_discovery_args->discovery_session->Stop(
          base::Bind(&BleSynchronizer::OnDiscoverySessionStopped,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&BleSynchronizer::OnErrorStoppingDiscoverySession,
                     weak_ptr_factory_.GetWeakPtr()));
      break;
    }
    default:
      NOTREACHED();
      return;
  }
}

void BleSynchronizer::SetTestDoubles(
    std::unique_ptr<base::Timer> test_timer,
    std::unique_ptr<base::Clock> test_clock,
    scoped_refptr<base::TaskRunner> test_task_runner) {
  timer_ = std::move(test_timer);
  clock_ = std::move(test_clock);
  task_runner_ = test_task_runner;
}

void BleSynchronizer::OnAdvertisementRegistered(
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  RecordBluetoothAdvertisementRegistrationResult(
      BluetoothAdvertisementResult::SUCCESS);
  ScheduleCommandCompletion();
  RegisterArgs* register_args = current_command_->register_args.get();
  DCHECK(register_args);
  register_args->callback.Run(std::move(advertisement));
}

void BleSynchronizer::OnErrorRegisteringAdvertisement(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  RecordBluetoothAdvertisementRegistrationResult(
      BluetoothAdvertisementErrorCodeToResult(error_code));
  ScheduleCommandCompletion();
  RegisterArgs* register_args = current_command_->register_args.get();
  DCHECK(register_args);
  register_args->error_callback.Run(error_code);
}

void BleSynchronizer::OnAdvertisementUnregistered() {
  RecordBluetoothAdvertisementUnregistrationResult(
      BluetoothAdvertisementResult::SUCCESS);
  ScheduleCommandCompletion();
  UnregisterArgs* unregister_args = current_command_->unregister_args.get();
  DCHECK(unregister_args);
  unregister_args->callback.Run();
}

void BleSynchronizer::OnErrorUnregisteringAdvertisement(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  RecordBluetoothAdvertisementUnregistrationResult(
      BluetoothAdvertisementErrorCodeToResult(error_code));
  ScheduleCommandCompletion();
  UnregisterArgs* unregister_args = current_command_->unregister_args.get();
  DCHECK(unregister_args);
  unregister_args->error_callback.Run(error_code);
}

void BleSynchronizer::OnDiscoverySessionStarted(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  RecordDiscoverySessionStarted(true /* success */);
  ScheduleCommandCompletion();
  StartDiscoveryArgs* start_discovery_args =
      current_command_->start_discovery_args.get();
  DCHECK(start_discovery_args);
  start_discovery_args->callback.Run(std::move(discovery_session));
}

void BleSynchronizer::OnErrorStartingDiscoverySession() {
  RecordDiscoverySessionStarted(false /* success */);
  ScheduleCommandCompletion();
  StartDiscoveryArgs* start_discovery_args =
      current_command_->start_discovery_args.get();
  DCHECK(start_discovery_args);
  start_discovery_args->error_callback.Run();
}

void BleSynchronizer::OnDiscoverySessionStopped() {
  RecordDiscoverySessionStopped(true /* success */);
  ScheduleCommandCompletion();
  StopDiscoveryArgs* stop_discovery_args =
      current_command_->stop_discovery_args.get();
  DCHECK(stop_discovery_args);
  stop_discovery_args->callback.Run();
}

void BleSynchronizer::OnErrorStoppingDiscoverySession() {
  RecordDiscoverySessionStopped(false /* success */);
  ScheduleCommandCompletion();
  StopDiscoveryArgs* stop_discovery_args =
      current_command_->stop_discovery_args.get();
  DCHECK(stop_discovery_args);
  stop_discovery_args->error_callback.Run();
}

void BleSynchronizer::ScheduleCommandCompletion() {
  // Schedule the task to run after the current task has completed. This is
  // necessary because the completion of a Bluetooth task may cause the Tether
  // component to be shut down; if that occurs, then we cannot reference
  // instance variables in this class after the object has been deleted.
  // Completing the current command as part of the next task ensures that this
  // cannot occur. See crbug.com/770863.
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&BleSynchronizer::CompleteCurrentCommand,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void BleSynchronizer::CompleteCurrentCommand() {
  current_command_.reset();
  last_command_end_timestamp_ = clock_->Now();
  ProcessQueue();
}

void BleSynchronizer::RecordBluetoothAdvertisementRegistrationResult(
    BluetoothAdvertisementResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.BluetoothAdvertisementRegistrationResult", result,
      BluetoothAdvertisementResult::BLUETOOTH_ADVERTISEMENT_RESULT_MAX);
}

void BleSynchronizer::RecordBluetoothAdvertisementUnregistrationResult(
    BluetoothAdvertisementResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      "InstantTethering.BluetoothAdvertisementUnregistrationResult", result,
      BluetoothAdvertisementResult::BLUETOOTH_ADVERTISEMENT_RESULT_MAX);
}

BleSynchronizer::BluetoothAdvertisementResult
BleSynchronizer::BluetoothAdvertisementErrorCodeToResult(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothAdvertisement::ErrorCode::ERROR_UNSUPPORTED_PLATFORM:
      return BluetoothAdvertisementResult::ERROR_UNSUPPORTED_PLATFORM;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_ALREADY_EXISTS:
      return BluetoothAdvertisementResult::ERROR_ADVERTISEMENT_ALREADY_EXISTS;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_DOES_NOT_EXIST:
      return BluetoothAdvertisementResult::ERROR_ADVERTISEMENT_DOES_NOT_EXIST;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_ADVERTISEMENT_INVALID_LENGTH:
      return BluetoothAdvertisementResult::ERROR_ADVERTISEMENT_INVALID_LENGTH;
    case device::BluetoothAdvertisement::ErrorCode::
        ERROR_INVALID_ADVERTISEMENT_INTERVAL:
      return BluetoothAdvertisementResult::ERROR_INVALID_ADVERTISEMENT_INTERVAL;
    case device::BluetoothAdvertisement::ErrorCode::ERROR_RESET_ADVERTISING:
      return BluetoothAdvertisementResult::ERROR_RESET_ADVERTISING;
    case device::BluetoothAdvertisement::ErrorCode::
        INVALID_ADVERTISEMENT_ERROR_CODE:
      return BluetoothAdvertisementResult::INVALID_ADVERTISEMENT_ERROR_CODE;
    default:
      return BluetoothAdvertisementResult::
          BLUETOOTH_ADVERTISEMENT_RESULT_UNKNOWN;
  }
}

void BleSynchronizer::RecordDiscoverySessionStarted(bool success) {
  UMA_HISTOGRAM_BOOLEAN("InstantTethering.BluetoothDiscoverySessionStarted",
                        success);
}

void BleSynchronizer::RecordDiscoverySessionStopped(bool success) {
  UMA_HISTOGRAM_BOOLEAN("InstantTethering.BluetoothDiscoverySessionStopped",
                        success);
}

}  // namespace tether

}  // namespace chromeos
