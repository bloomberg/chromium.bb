// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/sensors/device_sensor_host.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "device/sensors/device_sensor_export.h"
#include "device/sensors/device_sensor_service.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace device {

template <typename MojoInterface, ConsumerType consumer_type>
void DeviceSensorHost<MojoInterface, consumer_type>::Create(
    mojo::InterfaceRequest<MojoInterface> request) {
  mojo::MakeStrongBinding(
      base::WrapUnique(new DeviceSensorHost<MojoInterface, consumer_type>),
      std::move(request));
}

template <typename MojoInterface, ConsumerType consumer_type>
DeviceSensorHost<MojoInterface, consumer_type>::DeviceSensorHost()
    : is_started_(false) {
#if defined(OS_ANDROID)
  DCHECK(base::MessageLoopForUI::IsCurrent());
#else
  DCHECK(base::MessageLoopForIO::IsCurrent());
#endif
}

template <typename MojoInterface, ConsumerType consumer_type>
DeviceSensorHost<MojoInterface, consumer_type>::~DeviceSensorHost() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (is_started_)
    DeviceSensorService::GetInstance()->RemoveConsumer(consumer_type);
}

template <typename MojoInterface, ConsumerType consumer_type>
void DeviceSensorHost<MojoInterface, consumer_type>::DeviceSensorHost::
    StartPolling(const typename MojoInterface::StartPollingCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_started_);
  if (is_started_)
    return;
  is_started_ = true;
  DeviceSensorService::GetInstance()->AddConsumer(consumer_type);
  callback.Run(
      DeviceSensorService::GetInstance()->GetSharedMemoryHandle(consumer_type));
}

template <typename MojoInterface, ConsumerType consumer_type>
void DeviceSensorHost<MojoInterface,
                      consumer_type>::DeviceSensorHost::StopPolling() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(is_started_);
  if (!is_started_)
    return;
  is_started_ = false;
  DeviceSensorService::GetInstance()->RemoveConsumer(consumer_type);
}

template class DEVICE_SENSOR_EXPORT
    DeviceSensorHost<device::mojom::LightSensor, CONSUMER_TYPE_LIGHT>;
template class DEVICE_SENSOR_EXPORT
    DeviceSensorHost<device::mojom::MotionSensor, CONSUMER_TYPE_MOTION>;
template class DEVICE_SENSOR_EXPORT
    DeviceSensorHost<device::mojom::OrientationSensor,
                     CONSUMER_TYPE_ORIENTATION>;
template class DEVICE_SENSOR_EXPORT
    DeviceSensorHost<device::mojom::OrientationAbsoluteSensor,
                     CONSUMER_TYPE_ORIENTATION_ABSOLUTE>;

}  // namespace device
