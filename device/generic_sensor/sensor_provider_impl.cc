// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/sensor_provider_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/generic_sensor/platform_sensor_provider.h"
#include "device/generic_sensor/sensor_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/device/public/cpp/device_features.h"

namespace device {

namespace {

void RunCallback(mojom::SensorInitParamsPtr init_params,
                 mojom::SensorClientRequest client,
                 SensorProviderImpl::GetSensorCallback callback) {
  std::move(callback).Run(std::move(init_params), std::move(client));
}

void NotifySensorCreated(mojom::SensorInitParamsPtr init_params,
                         mojom::SensorClientRequest client,
                         SensorProviderImpl::GetSensorCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&RunCallback, std::move(init_params),
                                std::move(client), std::move(callback)));
}

}  // namespace

// static
void SensorProviderImpl::Create(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    mojom::SensorProviderRequest request) {
  PlatformSensorProvider* provider = PlatformSensorProvider::GetInstance();
  if (provider) {
    provider->SetFileTaskRunner(file_task_runner);
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
                                   GetSensorCallback callback) {
  // TODO(juncai): remove when the GenericSensor feature goes stable.
  // For sensors that are used by DeviceMotionEvent, don't check the
  // features::kGenericSensor flag.
  if (!base::FeatureList::IsEnabled(features::kGenericSensor) &&
      !(type == mojom::SensorType::ACCELEROMETER ||
        type == mojom::SensorType::LINEAR_ACCELERATION ||
        type == mojom::SensorType::GYROSCOPE)) {
    NotifySensorCreated(nullptr, nullptr, std::move(callback));
    return;
  }

  auto cloned_handle = provider_->CloneSharedBufferHandle();
  if (!cloned_handle.is_valid()) {
    NotifySensorCreated(nullptr, nullptr, std::move(callback));
    return;
  }

  scoped_refptr<PlatformSensor> sensor = provider_->GetSensor(type);
  if (!sensor) {
    PlatformSensorProviderBase::CreateSensorCallback cb = base::Bind(
        &SensorProviderImpl::SensorCreated, weak_ptr_factory_.GetWeakPtr(),
        type, base::Passed(&cloned_handle), base::Passed(&sensor_request),
        base::Passed(&callback));
    provider_->CreateSensor(type, cb);
    return;
  }

  SensorCreated(type, std::move(cloned_handle), std::move(sensor_request),
                std::move(callback), std::move(sensor));
}

void SensorProviderImpl::SensorCreated(
    mojom::SensorType type,
    mojo::ScopedSharedBufferHandle cloned_handle,
    mojom::SensorRequest sensor_request,
    GetSensorCallback callback,
    scoped_refptr<PlatformSensor> sensor) {
  if (!sensor) {
    NotifySensorCreated(nullptr, nullptr, std::move(callback));
    return;
  }

  auto sensor_impl = base::MakeUnique<SensorImpl>(sensor);

  auto init_params = mojom::SensorInitParams::New();
  init_params->memory = std::move(cloned_handle);
  init_params->buffer_offset = SensorReadingSharedBuffer::GetOffset(type);
  init_params->mode = sensor->GetReportingMode();
  init_params->default_configuration = sensor->GetDefaultConfiguration();

  double maximum_frequency = sensor->GetMaximumSupportedFrequency();
  DCHECK_GT(maximum_frequency, 0.0);
  if (maximum_frequency > mojom::SensorConfiguration::kMaxAllowedFrequency)
    maximum_frequency = mojom::SensorConfiguration::kMaxAllowedFrequency;

  init_params->maximum_frequency = maximum_frequency;
  init_params->minimum_frequency = sensor->GetMinimumSupportedFrequency();
  DCHECK_GT(init_params->minimum_frequency, 0.0);

  NotifySensorCreated(std::move(init_params), sensor_impl->GetClient(),
                      std::move(callback));

  mojo::MakeStrongBinding(std::move(sensor_impl), std::move(sensor_request));
}

}  // namespace device
