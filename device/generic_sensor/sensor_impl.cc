// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/sensor_impl.h"

#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

SensorImpl::SensorImpl(scoped_refptr<PlatformSensor> sensor)
    : sensor_(std::move(sensor)),
      suspended_(false),
      suppress_on_change_events_count_(0) {
  sensor_->AddClient(this);
}

SensorImpl::~SensorImpl() {
  sensor_->RemoveClient(this);
}

mojom::SensorClientRequest SensorImpl::GetClient() {
  return mojo::MakeRequest(&client_);
}

void SensorImpl::AddConfiguration(
    const PlatformSensorConfiguration& configuration,
    AddConfigurationCallback callback) {
  // TODO(Mikhail): To avoid overflowing browser by repeated AddConfigs
  // (maybe limit the number of configs per client).
  bool success = sensor_->StartListening(this, configuration);
  if (success && configuration.suppress_on_change_events())
    ++suppress_on_change_events_count_;
  std::move(callback).Run(success);
}

void SensorImpl::GetDefaultConfiguration(
    GetDefaultConfigurationCallback callback) {
  std::move(callback).Run(sensor_->GetDefaultConfiguration());
}

void SensorImpl::RemoveConfiguration(
    const PlatformSensorConfiguration& configuration,
    RemoveConfigurationCallback callback) {
  bool success = sensor_->StopListening(this, configuration);
  if (success && configuration.suppress_on_change_events())
    --suppress_on_change_events_count_;
  std::move(callback).Run(success);
}

void SensorImpl::Suspend() {
  suspended_ = true;
  sensor_->UpdateSensor();
}

void SensorImpl::Resume() {
  suspended_ = false;
  sensor_->UpdateSensor();
}

void SensorImpl::OnSensorReadingChanged() {
  DCHECK(!suspended_);
  if (client_ && suppress_on_change_events_count_ == 0)
    client_->SensorReadingChanged();
}

void SensorImpl::OnSensorError() {
  DCHECK(!suspended_);
  if (client_)
    client_->RaiseError();
}

bool SensorImpl::IsNotificationSuspended() {
  return suspended_;
}

}  // namespace device
