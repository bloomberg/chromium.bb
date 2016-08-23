// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/sensor_provider_impl.h"

#include <utility>

#include "device/generic_sensor/platform_sensor_provider.h"
#include "device/generic_sensor/sensor_impl.h"

namespace device {

namespace {

uint64_t GetBufferOffset(mojom::SensorType type) {
  return (static_cast<uint64_t>(mojom::SensorType::LAST) -
          static_cast<uint64_t>(type)) *
         mojom::SensorReadBuffer::kReadBufferSize;
}

}  // namespace

// static
void SensorProviderImpl::Create(
    mojo::InterfaceRequest<mojom::SensorProvider> request) {
  PlatformSensorProvider* provider = PlatformSensorProvider::GetInstance();
  if (provider)
    new SensorProviderImpl(std::move(request), provider);
}

SensorProviderImpl::SensorProviderImpl(
    mojo::InterfaceRequest<mojom::SensorProvider> request,
    PlatformSensorProvider* provider)
    : binding_(this, std::move(request)), provider_(provider) {
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
        type, mojom::SensorReadBuffer::kReadBufferSize, GetBufferOffset(type));
  }

  if (!sensor) {
    callback.Run(nullptr, nullptr);
    return;
  }

  auto sensor_impl = new SensorImpl(std::move(sensor_request), sensor);

  auto sensor_read_buffer = mojom::SensorReadBuffer::New();
  sensor_read_buffer->memory = std::move(cloned_handle);
  sensor_read_buffer->offset = GetBufferOffset(type);
  sensor_read_buffer->mode = sensor->GetReportingMode();

  callback.Run(std::move(sensor_read_buffer), sensor_impl->GetClient());
}

}  // namespace device
