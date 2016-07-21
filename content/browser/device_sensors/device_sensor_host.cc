// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_sensors/device_sensor_host.h"

#include "content/browser/device_sensors/device_sensor_service.h"
#include "content/public/browser/browser_thread.h"

namespace content {

template <typename MojoInterface, ConsumerType consumer_type>
void DeviceSensorHost<MojoInterface, consumer_type>::Create(
    mojo::InterfaceRequest<MojoInterface> request) {
  new DeviceSensorHost(std::move(request));
}

template <typename MojoInterface, ConsumerType consumer_type>
DeviceSensorHost<MojoInterface, consumer_type>::DeviceSensorHost(
    mojo::InterfaceRequest<MojoInterface> request)
    : is_started_(false), binding_(this, std::move(request)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
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

template class DeviceSensorHost<device::mojom::LightSensor,
                                CONSUMER_TYPE_LIGHT>;
template class DeviceSensorHost<device::mojom::MotionSensor,
                                CONSUMER_TYPE_MOTION>;
template class DeviceSensorHost<device::mojom::OrientationSensor,
                                CONSUMER_TYPE_ORIENTATION>;
template class DeviceSensorHost<device::mojom::OrientationAbsoluteSensor,
                                CONSUMER_TYPE_ORIENTATION_ABSOLUTE>;

}  // namespace content
