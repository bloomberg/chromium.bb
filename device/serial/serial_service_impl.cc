// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_service_impl.h"

#include "base/bind.h"
#include "base/location.h"

namespace device {

SerialServiceImpl::SerialServiceImpl() {
}

SerialServiceImpl::~SerialServiceImpl() {
}

// static
void SerialServiceImpl::Create(
    mojo::InterfaceRequest<serial::SerialService> request) {
  mojo::BindToRequest(new SerialServiceImpl(), &request);
}

// static
void SerialServiceImpl::CreateOnMessageLoop(
    scoped_refptr<base::MessageLoopProxy> message_loop,
    mojo::InterfaceRequest<serial::SerialService> request) {
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(&SerialServiceImpl::Create, base::Passed(&request)));
}

void SerialServiceImpl::GetDevices(
    const mojo::Callback<void(mojo::Array<serial::DeviceInfoPtr>)>& callback) {
  if (!device_enumerator_)
    device_enumerator_ = SerialDeviceEnumerator::Create();
  callback.Run(device_enumerator_->GetDevices());
}

void SerialServiceImpl::OnConnectionError() {
  delete this;
}

}  // namespace device
