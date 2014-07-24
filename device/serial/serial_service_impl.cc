// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_service_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "device/serial/serial_io_handler.h"

namespace device {

SerialServiceImpl::SerialServiceImpl(
    scoped_refptr<SerialConnectionFactory> connection_factory)
    : connection_factory_(connection_factory) {
}

SerialServiceImpl::SerialServiceImpl(
    scoped_refptr<SerialConnectionFactory> connection_factory,
    scoped_ptr<SerialDeviceEnumerator> device_enumerator)
    : device_enumerator_(device_enumerator.Pass()),
      connection_factory_(connection_factory) {
}

SerialServiceImpl::~SerialServiceImpl() {
}

// static
void SerialServiceImpl::Create(
    scoped_refptr<base::MessageLoopProxy> io_message_loop,
    mojo::InterfaceRequest<serial::SerialService> request) {
  mojo::BindToRequest(new SerialServiceImpl(new SerialConnectionFactory(
                          base::Bind(SerialIoHandler::Create,
                                     base::MessageLoopProxy::current()),
                          io_message_loop)),
                      &request);
}

// static
void SerialServiceImpl::CreateOnMessageLoop(
    scoped_refptr<base::MessageLoopProxy> message_loop,
    scoped_refptr<base::MessageLoopProxy> io_message_loop,
    mojo::InterfaceRequest<serial::SerialService> request) {
  message_loop->PostTask(
      FROM_HERE,
      base::Bind(
          &SerialServiceImpl::Create, io_message_loop, base::Passed(&request)));
}

void SerialServiceImpl::GetDevices(
    const mojo::Callback<void(mojo::Array<serial::DeviceInfoPtr>)>& callback) {
  callback.Run(GetDeviceEnumerator()->GetDevices());
}

void SerialServiceImpl::Connect(
    const mojo::String& path,
    serial::ConnectionOptionsPtr options,
    mojo::InterfaceRequest<serial::Connection> connection_request) {
  if (!IsValidPath(path))
    return;
  connection_factory_->CreateConnection(
      path, options.Pass(), connection_request.Pass());
}

SerialDeviceEnumerator* SerialServiceImpl::GetDeviceEnumerator() {
  if (!device_enumerator_)
    device_enumerator_ = SerialDeviceEnumerator::Create();
  return device_enumerator_.get();
}

bool SerialServiceImpl::IsValidPath(const mojo::String& path) {
  mojo::Array<serial::DeviceInfoPtr> devices(
      GetDeviceEnumerator()->GetDevices());
  for (size_t i = 0; i < devices.size(); i++) {
    if (path == devices[i]->path)
      return true;
  }
  return false;
}

void SerialServiceImpl::OnConnected(
    const mojo::Callback<void(serial::ConnectionInfoPtr)>& callback,
    serial::ConnectionInfoPtr result) {
  callback.Run(result.Pass());
}

}  // namespace device
