// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_connection.h"

#include "base/bind.h"
#include "device/serial/buffer.h"
#include "device/serial/data_sink_receiver.h"
#include "device/serial/data_source_sender.h"
#include "device/serial/serial_io_handler.h"

namespace device {

SerialConnection::SerialConnection(
    scoped_refptr<SerialIoHandler> io_handler,
    mojo::InterfaceRequest<serial::DataSink> sink,
    mojo::InterfaceRequest<serial::DataSource> source)
    : io_handler_(io_handler) {
  receiver_ = mojo::WeakBindToRequest(
      new DataSinkReceiver(base::Bind(&SerialConnection::OnSendPipeReady,
                                      base::Unretained(this)),
                           base::Bind(&SerialConnection::OnSendCancelled,
                                      base::Unretained(this)),
                           base::Bind(base::DoNothing)),
      &sink);
  sender_ = mojo::WeakBindToRequest(
      new DataSourceSender(base::Bind(&SerialConnection::OnReceivePipeReady,
                                      base::Unretained(this)),
                           base::Bind(base::DoNothing)),
      &source);
}

SerialConnection::~SerialConnection() {
  receiver_->ShutDown();
  sender_->ShutDown();
  io_handler_->CancelRead(serial::RECEIVE_ERROR_DISCONNECTED);
  io_handler_->CancelWrite(serial::SEND_ERROR_DISCONNECTED);
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

void SerialConnection::OnSendCancelled(int32_t error) {
  io_handler_->CancelWrite(static_cast<serial::SendError>(error));
}

void SerialConnection::OnSendPipeReady(scoped_ptr<ReadOnlyBuffer> buffer) {
  io_handler_->Write(buffer.Pass());
}

void SerialConnection::OnReceivePipeReady(scoped_ptr<WritableBuffer> buffer) {
  io_handler_->Read(buffer.Pass());
}

}  // namespace device
