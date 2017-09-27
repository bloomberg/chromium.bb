// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/ble_synchronizer.h"

#include "base/memory/ptr_util.h"
#include "base/time/default_clock.h"
#include "components/proximity_auth/logging/logging.h"

namespace chromeos {

namespace tether {

namespace {

const int64_t kTimeBetweenEachCommandMs = 200;

}  // namespace

BleSynchronizer::RegisterArgs::RegisterArgs(
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
    const device::BluetoothAdapter::AdvertisementErrorCallback& error_callback)
    : advertisement_data(std::move(advertisement_data)),
      callback(callback),
      error_callback(error_callback) {}

BleSynchronizer::RegisterArgs::~RegisterArgs() {}

BleSynchronizer::UnregisterArgs::UnregisterArgs(
    scoped_refptr<device::BluetoothAdvertisement> advertisement,
    const device::BluetoothAdvertisement::SuccessCallback& callback,
    const device::BluetoothAdvertisement::ErrorCallback& error_callback)
    : advertisement(std::move(advertisement)),
      callback(callback),
      error_callback(error_callback) {}

BleSynchronizer::UnregisterArgs::~UnregisterArgs() {}

BleSynchronizer::StartDiscoveryArgs::StartDiscoveryArgs(
    const device::BluetoothAdapter::DiscoverySessionCallback& callback,
    const device::BluetoothAdapter::ErrorCallback& error_callback)
    : callback(callback), error_callback(error_callback) {}

BleSynchronizer::StartDiscoveryArgs::~StartDiscoveryArgs() {}

BleSynchronizer::StopDiscoveryArgs::StopDiscoveryArgs(
    base::WeakPtr<device::BluetoothDiscoverySession> discovery_session,
    const base::Closure& callback,
    const device::BluetoothDiscoverySession::ErrorCallback& error_callback)
    : discovery_session(discovery_session),
      callback(callback),
      error_callback(error_callback) {}

BleSynchronizer::StopDiscoveryArgs::~StopDiscoveryArgs() {}

BleSynchronizer::Command::Command(std::unique_ptr<RegisterArgs> register_args)
    : command_type(CommandType::REGISTER_ADVERTISEMENT),
      register_args(std::move(register_args)) {}

BleSynchronizer::Command::Command(
    std::unique_ptr<UnregisterArgs> unregister_args)
    : command_type(CommandType::UNREGISTER_ADVERTISEMENT),
      unregister_args(std::move(unregister_args)) {}

BleSynchronizer::Command::Command(
    std::unique_ptr<StartDiscoveryArgs> start_discovery_args)
    : command_type(CommandType::START_DISCOVERY),
      start_discovery_args(std::move(start_discovery_args)) {}

BleSynchronizer::Command::Command(
    std::unique_ptr<StopDiscoveryArgs> stop_discovery_args)
    : command_type(CommandType::STOP_DISCOVERY),
      stop_discovery_args(std::move(stop_discovery_args)) {}

BleSynchronizer::Command::~Command() {}

BleSynchronizer::BleSynchronizer(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : bluetooth_adapter_(bluetooth_adapter),
      timer_(base::MakeUnique<base::OneShotTimer>()),
      clock_(base::MakeUnique<base::DefaultClock>()),
      weak_ptr_factory_(this) {}

BleSynchronizer::~BleSynchronizer() {}

void BleSynchronizer::RegisterAdvertisement(
    std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data,
    const device::BluetoothAdapter::CreateAdvertisementCallback& callback,
    const device::BluetoothAdapter::AdvertisementErrorCallback&
        error_callback) {
  command_queue_.emplace_back(
      base::MakeUnique<Command>(base::MakeUnique<RegisterArgs>(
          std::move(advertisement_data), callback, error_callback)));
  ProcessQueue();
}

void BleSynchronizer::UnregisterAdvertisement(
    scoped_refptr<device::BluetoothAdvertisement> advertisement,
    const device::BluetoothAdvertisement::SuccessCallback& callback,
    const device::BluetoothAdvertisement::ErrorCallback& error_callback) {
  command_queue_.emplace_back(
      base::MakeUnique<Command>(base::MakeUnique<UnregisterArgs>(
          std::move(advertisement), callback, error_callback)));
  ProcessQueue();
}

void BleSynchronizer::StartDiscoverySession(
    const device::BluetoothAdapter::DiscoverySessionCallback& callback,
    const device::BluetoothAdapter::ErrorCallback& error_callback) {
  command_queue_.emplace_back(base::MakeUnique<Command>(
      base::MakeUnique<StartDiscoveryArgs>(callback, error_callback)));
  ProcessQueue();
}

void BleSynchronizer::StopDiscoverySession(
    base::WeakPtr<device::BluetoothDiscoverySession> discovery_session,
    const base::Closure& callback,
    const device::BluetoothDiscoverySession::ErrorCallback& error_callback) {
  command_queue_.emplace_back(
      base::MakeUnique<Command>(base::MakeUnique<StopDiscoveryArgs>(
          discovery_session, callback, error_callback)));
  ProcessQueue();
}

void BleSynchronizer::ProcessQueue() {
  if (current_command_ || command_queue_.empty())
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

  current_command_ = std::move(command_queue_.front());
  command_queue_.pop_front();

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

void BleSynchronizer::SetTestDoubles(std::unique_ptr<base::Timer> test_timer,
                                     std::unique_ptr<base::Clock> test_clock) {
  timer_ = std::move(test_timer);
  clock_ = std::move(test_clock);
}

void BleSynchronizer::OnAdvertisementRegistered(
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  RegisterArgs* register_args = current_command_->register_args.get();
  DCHECK(register_args);
  register_args->callback.Run(std::move(advertisement));
  CompleteCurrentCommand();
}

void BleSynchronizer::OnErrorRegisteringAdvertisement(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  RegisterArgs* register_args = current_command_->register_args.get();
  DCHECK(register_args);
  register_args->error_callback.Run(error_code);
  CompleteCurrentCommand();
}

void BleSynchronizer::OnAdvertisementUnregistered() {
  UnregisterArgs* unregister_args = current_command_->unregister_args.get();
  DCHECK(unregister_args);
  unregister_args->callback.Run();
  CompleteCurrentCommand();
}

void BleSynchronizer::OnErrorUnregisteringAdvertisement(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  UnregisterArgs* unregister_args = current_command_->unregister_args.get();
  DCHECK(unregister_args);
  unregister_args->error_callback.Run(error_code);
  CompleteCurrentCommand();
}

void BleSynchronizer::OnDiscoverySessionStarted(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  StartDiscoveryArgs* start_discovery_args =
      current_command_->start_discovery_args.get();
  DCHECK(start_discovery_args);
  start_discovery_args->callback.Run(std::move(discovery_session));
  CompleteCurrentCommand();
}

void BleSynchronizer::OnErrorStartingDiscoverySession() {
  StartDiscoveryArgs* start_discovery_args =
      current_command_->start_discovery_args.get();
  DCHECK(start_discovery_args);
  start_discovery_args->error_callback.Run();
  CompleteCurrentCommand();
}

void BleSynchronizer::OnDiscoverySessionStopped() {
  StopDiscoveryArgs* stop_discovery_args =
      current_command_->stop_discovery_args.get();
  DCHECK(stop_discovery_args);
  stop_discovery_args->callback.Run();
  CompleteCurrentCommand();
}

void BleSynchronizer::OnErrorStoppingDiscoverySession() {
  StopDiscoveryArgs* stop_discovery_args =
      current_command_->stop_discovery_args.get();
  DCHECK(stop_discovery_args);
  stop_discovery_args->error_callback.Run();
  CompleteCurrentCommand();
}

void BleSynchronizer::CompleteCurrentCommand() {
  current_command_.reset();
  last_command_end_timestamp_ = clock_->Now();
  ProcessQueue();
}

}  // namespace tether

}  // namespace chromeos
