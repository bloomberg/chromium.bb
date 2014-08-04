// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_connection.h"

#include "device/serial/serial_io_handler.h"

namespace device {

SerialConnection::SerialConnection(scoped_refptr<SerialIoHandler> io_handler)
    : io_handler_(io_handler) {
}

SerialConnection::~SerialConnection() {
}

void SerialConnection::GetInfo(
    const mojo::Callback<void(serial::ConnectionInfoPtr)>& callback) {
  callback.Run(io_handler_->GetPortInfo());
}

void SerialConnection::SetOptions(serial::ConnectionOptionsPtr options,
                                  const mojo::Callback<void(bool)>& callback) {
  callback.Run(io_handler_->ConfigurePort(*options));
  io_handler_->CancelRead(device::serial::RECEIVE_ERROR_NONE);
}

void SerialConnection::SetControlSignals(
    serial::HostControlSignalsPtr signals,
    const mojo::Callback<void(bool)>& callback) {
  callback.Run(io_handler_->SetControlSignals(*signals));
}

void SerialConnection::GetControlSignals(
    const mojo::Callback<void(serial::DeviceControlSignalsPtr)>& callback) {
  callback.Run(io_handler_->GetControlSignals());
}

void SerialConnection::Flush(const mojo::Callback<void(bool)>& callback) {
  callback.Run(io_handler_->Flush());
}

}  // namespace device
