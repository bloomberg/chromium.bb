// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/platform_sensor_provider_base.h"

#include <utility>

#include "base/stl_util.h"
#include "device/generic_sensor/public/interfaces/sensor_provider.mojom.h"

namespace device {

namespace {

const uint64_t kSharedBufferSizeInBytes =
    mojom::SensorInitParams::kReadBufferSize *
    static_cast<uint64_t>(mojom::SensorType::LAST);

}  // namespace

PlatformSensorProviderBase::PlatformSensorProviderBase() = default;
PlatformSensorProviderBase::~PlatformSensorProviderBase() = default;

void PlatformSensorProviderBase::CreateSensor(
    mojom::SensorType type,
    uint64_t size,
    uint64_t offset,
    const CreateSensorCallback& callback) {
  DCHECK(CalledOnValidThread());

  if (!CreateSharedBufferIfNeeded()) {
    callback.Run(nullptr);
    return;
  }

  mojo::ScopedSharedBufferMapping mapping =
      shared_buffer_handle_->MapAtOffset(size, offset);
  if (!mapping) {
    callback.Run(nullptr);
    return;
  }

  auto it = requests_map_.find(type);
  if (it != requests_map_.end()) {
    it->second.push_back(callback);
  } else {
    requests_map_[type] = CallbackQueue({callback});

    CreateSensorInternal(
        type, std::move(mapping), size,
        base::Bind(&PlatformSensorProviderBase::NotifySensorCreated,
                   base::Unretained(this), type));
  }
}

scoped_refptr<PlatformSensor> PlatformSensorProviderBase::GetSensor(
    mojom::SensorType type) {
  DCHECK(CalledOnValidThread());

  auto it = sensor_map_.find(type);
  if (it != sensor_map_.end())
    return it->second;
  return nullptr;
}

bool PlatformSensorProviderBase::CreateSharedBufferIfNeeded() {
  DCHECK(CalledOnValidThread());
  if (shared_buffer_handle_.is_valid())
    return true;

  shared_buffer_handle_ =
      mojo::SharedBufferHandle::Create(kSharedBufferSizeInBytes);
  return shared_buffer_handle_.is_valid();
}

void PlatformSensorProviderBase::RemoveSensor(mojom::SensorType type) {
  DCHECK(CalledOnValidThread());
  DCHECK(ContainsKey(sensor_map_, type));
  sensor_map_.erase(type);

  if (sensor_map_.empty())
    shared_buffer_handle_.reset();
}

mojo::ScopedSharedBufferHandle
PlatformSensorProviderBase::CloneSharedBufferHandle() {
  DCHECK(CalledOnValidThread());
  CreateSharedBufferIfNeeded();
  return shared_buffer_handle_->Clone();
}

void PlatformSensorProviderBase::NotifySensorCreated(
    mojom::SensorType type,
    scoped_refptr<PlatformSensor> sensor) {
  DCHECK(CalledOnValidThread());
  DCHECK(!ContainsKey(sensor_map_, type));
  DCHECK(ContainsKey(requests_map_, type));

  if (sensor)
    sensor_map_[type] = sensor.get();

  // Inform subscribers about the sensor.
  // |sensor| can be nullptr here.
  auto it = requests_map_.find(type);
  for (auto& callback : it->second)
    callback.Run(sensor);

  requests_map_.erase(type);
}

}  // namespace device
