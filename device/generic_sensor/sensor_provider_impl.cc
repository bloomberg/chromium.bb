// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/sensor_provider_impl.h"

#include <utility>

#include "device/generic_sensor/platform_sensor_provider.h"
#include "device/generic_sensor/sensor_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

namespace {

uint64_t GetBufferOffset(mojom::SensorType type) {
  return (static_cast<uint64_t>(mojom::SensorType::LAST) -
          static_cast<uint64_t>(type)) *
         mojom::SensorInitParams::kReadBufferSize;
}

void RunCallback(mojom::SensorInitParamsPtr init_params,
                 SensorImpl* sensor,
                 const SensorProviderImpl::GetSensorCallback& callback) {
  callback.Run(std::move(init_params), sensor->GetClient());
}

void NotifySensorCreated(
    mojom::SensorInitParamsPtr init_params,
    SensorImpl* sensor,
    const SensorProviderImpl::GetSensorCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(RunCallback, base::Passed(&init_params), sensor, callback));
}

}  // namespace

// static
void SensorProviderImpl::Create(mojom::SensorProviderRequest request) {
  PlatformSensorProvider* provider = PlatformSensorProvider::GetInstance();
  if (provider) {
    mojo::MakeStrongBinding(base::WrapUnique(new SensorProviderImpl(provider)),
                            std::move(request));
  }
}

SensorProviderImpl::SensorProviderImpl(PlatformSensorProvider* provider)
    : provider_(provider), weak_ptr_factory_(this) {
  DCHECK(provider_);
}

SensorProviderImpl::~SensorProviderImpl() {}

void SensorProviderImpl::GetSensor(mojom::SensorType type,
                                   mojom::SensorRequest sensor_request,
                                   const GetSensorCallback& callback) {
  auto cloned_handle = provider_->CloneSharedBufferHandle();
  if (!cloned_handle.is_valid()) {
    NotifySensorCreated(nullptr, nullptr, callback);
    return;
  }

  scoped_refptr<PlatformSensor> sensor = provider_->GetSensor(type);
  if (!sensor) {
    PlatformSensorProviderBase::CreateSensorCallback cb = base::Bind(
        &SensorProviderImpl::SensorCreated, weak_ptr_factory_.GetWeakPtr(),
        type, base::Passed(&cloned_handle), base::Passed(&sensor_request),
        callback);
    provider_->CreateSensor(type, mojom::SensorInitParams::kReadBufferSize,
                            GetBufferOffset(type), cb);
    return;
  }

  SensorCreated(type, std::move(cloned_handle), std::move(sensor_request),
                callback, std::move(sensor));
}

void SensorProviderImpl::SensorCreated(
    mojom::SensorType type,
    mojo::ScopedSharedBufferHandle cloned_handle,
    mojom::SensorRequest sensor_request,
    const GetSensorCallback& callback,
    scoped_refptr<PlatformSensor> sensor) {
  if (!sensor) {
    NotifySensorCreated(nullptr, nullptr, callback);
    return;
  }

  auto sensor_impl = base::MakeUnique<SensorImpl>(sensor);

  auto init_params = mojom::SensorInitParams::New();
  init_params->memory = std::move(cloned_handle);
  init_params->buffer_offset = GetBufferOffset(type);
  init_params->mode = sensor->GetReportingMode();
  init_params->default_configuration = sensor->GetDefaultConfiguration();

  NotifySensorCreated(std::move(init_params), sensor_impl.get(), callback);

  mojo::MakeStrongBinding(std::move(sensor_impl), std::move(sensor_request));
}

}  // namespace device
