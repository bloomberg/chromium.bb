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
    : provider_(provider) {
  DCHECK(provider_);
}

SensorProviderImpl::~SensorProviderImpl() {}

void SensorProviderImpl::GetSensor(mojom::SensorType type,
                                   mojom::SensorRequest sensor_request,
                                   const GetSensorCallback& callback) {
  auto cloned_handle = provider_->CloneSharedBufferHandle();
  if (!cloned_handle.is_valid()) {
    callback.Run(nullptr, nullptr);
    return;
  }

  scoped_refptr<PlatformSensor> sensor = provider_->GetSensor(type);
  if (!sensor) {
    sensor = provider_->CreateSensor(
        type, mojom::SensorInitParams::kReadBufferSize, GetBufferOffset(type));
  }

  if (!sensor) {
    callback.Run(nullptr, nullptr);
    return;
  }

  auto sensor_impl = base::MakeUnique<SensorImpl>(sensor);

  auto init_params = mojom::SensorInitParams::New();
  init_params->memory = std::move(cloned_handle);
  init_params->buffer_offset = GetBufferOffset(type);
  init_params->mode = sensor->GetReportingMode();
  init_params->default_configuration = sensor->GetDefaultConfiguration();

  callback.Run(std::move(init_params), sensor_impl->GetClient());

  mojo::MakeStrongBinding(std::move(sensor_impl), std::move(sensor_request));
}

}  // namespace device
