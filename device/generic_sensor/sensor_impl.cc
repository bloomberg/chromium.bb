// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/generic_sensor/sensor_impl.h"

#include <utility>

namespace device {

SensorImpl::SensorImpl(mojo::InterfaceRequest<Sensor> request,
                       scoped_refptr<PlatformSensor> sensor)
    : sensor_(std::move(sensor)),
      binding_(this, std::move(request)),
      suspended_(false) {
  sensor_->AddClient(this);
}

SensorImpl::~SensorImpl() {
  sensor_->RemoveClient(this);
}

mojom::SensorClientRequest SensorImpl::GetClient() {
  return mojo::GetProxy(&client_);
}

void SensorImpl::AddConfiguration(
    const PlatformSensorConfiguration& configuration,
    const AddConfigurationCallback& callback) {
  // TODO(Mikhail): To avoid overflowing browser by repeated AddConfigs
  // (maybe limit the number of configs per client).
  callback.Run(sensor_->StartListening(this, configuration));
}

void SensorImpl::RemoveConfiguration(
    const PlatformSensorConfiguration& configuration,
    const RemoveConfigurationCallback& callback) {
  callback.Run(sensor_->StopListening(this, configuration));
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
  if (client_)
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
