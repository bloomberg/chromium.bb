// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_provider_linux.h"

#include "base/memory/singleton.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "device/generic_sensor/linux/sensor_data_linux.h"
#include "device/generic_sensor/platform_sensor_linux.h"
#include "device/generic_sensor/platform_sensor_reader_linux.h"

namespace device {

// static
PlatformSensorProviderLinux* PlatformSensorProviderLinux::GetInstance() {
  return base::Singleton<
      PlatformSensorProviderLinux,
      base::LeakySingletonTraits<PlatformSensorProviderLinux>>::get();
}

PlatformSensorProviderLinux::PlatformSensorProviderLinux()
    : sensor_nodes_enumerated_(false),
      sensor_nodes_enumeration_started_(false),
      sensor_device_manager_(nullptr) {}

PlatformSensorProviderLinux::~PlatformSensorProviderLinux() {
  DCHECK(!sensor_device_manager_);
}

void PlatformSensorProviderLinux::CreateSensorInternal(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    const CreateSensorCallback& callback) {
  if (!sensor_device_manager_)
    sensor_device_manager_.reset(new SensorDeviceManager());

  if (!sensor_nodes_enumerated_) {
    if (!sensor_nodes_enumeration_started_) {
      sensor_nodes_enumeration_started_ = file_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&SensorDeviceManager::Start,
                     base::Unretained(sensor_device_manager_.get()), this));
    }
    return;
  }

  SensorInfoLinux* sensor_device = GetSensorDevice(type);
  if (!sensor_device) {
    // If there are no sensors, stop polling thread.
    if (!HasSensors())
      AllSensorsRemoved();
    callback.Run(nullptr);
    return;
  }
  SensorDeviceFound(type, std::move(mapping), callback, sensor_device);
}

void PlatformSensorProviderLinux::SensorDeviceFound(
    mojom::SensorType type,
    mojo::ScopedSharedBufferMapping mapping,
    const PlatformSensorProviderBase::CreateSensorCallback& callback,
    const SensorInfoLinux* sensor_device) {
  DCHECK(CalledOnValidThread());
  DCHECK(sensor_device);

  if (!StartPollingThread()) {
    callback.Run(nullptr);
    return;
  }

  scoped_refptr<PlatformSensorLinux> sensor =
      new PlatformSensorLinux(type, std::move(mapping), this, sensor_device,
                              polling_thread_->task_runner());
  callback.Run(sensor);
}

void PlatformSensorProviderLinux::SetFileTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {
  DCHECK(CalledOnValidThread());
  if (!file_task_runner_)
    file_task_runner_ = file_task_runner;
}

void PlatformSensorProviderLinux::AllSensorsRemoved() {
  DCHECK(CalledOnValidThread());
  DCHECK(file_task_runner_);
  Shutdown();
  // When there are no sensors left, the polling thread must be stopped.
  // Stop() can only be called on a different thread that allows I/O.
  // Thus, browser's file thread is used for this purpose.
  file_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PlatformSensorProviderLinux::StopPollingThread,
                            base::Unretained(this)));
}

bool PlatformSensorProviderLinux::StartPollingThread() {
  if (!polling_thread_)
    polling_thread_.reset(new base::Thread("Sensor polling thread"));

  if (!polling_thread_->IsRunning()) {
    return polling_thread_->StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  }
  return true;
}

void PlatformSensorProviderLinux::StopPollingThread() {
  DCHECK(file_task_runner_);
  DCHECK(file_task_runner_->BelongsToCurrentThread());
  if (polling_thread_ && polling_thread_->IsRunning())
    polling_thread_->Stop();
}

void PlatformSensorProviderLinux::Shutdown() {
  DCHECK(CalledOnValidThread());
  const bool did_post_task = file_task_runner_->DeleteSoon(
      FROM_HERE, sensor_device_manager_.release());
  DCHECK(did_post_task);
  sensor_nodes_enumerated_ = false;
  sensor_nodes_enumeration_started_ = false;
  sensor_devices_by_type_.clear();
}

SensorInfoLinux* PlatformSensorProviderLinux::GetSensorDevice(
    mojom::SensorType type) {
  DCHECK(CalledOnValidThread());
  auto sensor = sensor_devices_by_type_.find(type);
  if (sensor == sensor_devices_by_type_.end())
    return nullptr;
  return sensor->second.get();
}

void PlatformSensorProviderLinux::GetAllSensorDevices() {
  DCHECK(CalledOnValidThread());
  // TODO(maksims): implement this method once we have discovery API.
  NOTIMPLEMENTED();
}

void PlatformSensorProviderLinux::SetSensorDeviceManagerForTesting(
    std::unique_ptr<SensorDeviceManager> sensor_device_manager) {
  DCHECK(CalledOnValidThread());
  Shutdown();
  sensor_device_manager_ = std::move(sensor_device_manager);
}

void PlatformSensorProviderLinux::SetFileTaskRunnerForTesting(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(CalledOnValidThread());
  file_task_runner_ = std::move(task_runner);
}

void PlatformSensorProviderLinux::ProcessStoredRequests() {
  DCHECK(CalledOnValidThread());
  std::vector<mojom::SensorType> request_types = GetPendingRequestTypes();
  if (request_types.empty())
    return;

  for (auto const& type : request_types) {
    SensorInfoLinux* device = nullptr;
    auto device_entry = sensor_devices_by_type_.find(type);
    if (device_entry != sensor_devices_by_type_.end())
      device = device_entry->second.get();
    CreateSensorAndNotify(type, device);
  }
}

void PlatformSensorProviderLinux::CreateSensorAndNotify(
    mojom::SensorType type,
    SensorInfoLinux* sensor_device) {
  DCHECK(CalledOnValidThread());
  scoped_refptr<PlatformSensorLinux> sensor;
  mojo::ScopedSharedBufferMapping mapping = MapSharedBufferForType(type);
  if (sensor_device && mapping && StartPollingThread()) {
    sensor =
        new PlatformSensorLinux(type, std::move(mapping), this, sensor_device,
                                polling_thread_->task_runner());
  }
  NotifySensorCreated(type, sensor);
}

void PlatformSensorProviderLinux::OnSensorNodesEnumerated() {
  DCHECK(CalledOnValidThread());
  DCHECK(!sensor_nodes_enumerated_);
  sensor_nodes_enumerated_ = true;
  ProcessStoredRequests();
}

void PlatformSensorProviderLinux::OnDeviceAdded(
    mojom::SensorType type,
    std::unique_ptr<SensorInfoLinux> sensor_device) {
  DCHECK(CalledOnValidThread());
  // At the moment, we support only one device per type.
  if (base::ContainsKey(sensor_devices_by_type_, type)) {
    DVLOG(1) << "Sensor ignored. Type " << type
             << ". Node: " << sensor_device->device_node;
    return;
  }
  sensor_devices_by_type_[type] = std::move(sensor_device);
}

void PlatformSensorProviderLinux::OnDeviceRemoved(
    mojom::SensorType type,
    const std::string& device_node) {
  DCHECK(CalledOnValidThread());
  auto it = sensor_devices_by_type_.find(type);
  if (it != sensor_devices_by_type_.end() &&
      it->second->device_node == device_node)
    sensor_devices_by_type_.erase(it);
}

}  // namespace device
