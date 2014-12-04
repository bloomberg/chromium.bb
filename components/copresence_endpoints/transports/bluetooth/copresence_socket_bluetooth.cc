// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence_endpoints/transports/bluetooth/copresence_socket_bluetooth.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_piece.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "net/base/io_buffer.h"

namespace {

// TODO(rkc): This number is totally arbitrary. Figure out what this should be.
const int kMaxReceiveBytes = 4096;

}  // namespace

namespace copresence_endpoints {

CopresenceSocketBluetooth::CopresenceSocketBluetooth(
    const scoped_refptr<device::BluetoothSocket>& socket)
    : socket_(socket), receiving_(false), weak_ptr_factory_(this) {
}

CopresenceSocketBluetooth::~CopresenceSocketBluetooth() {
  receiving_ = false;
}

bool CopresenceSocketBluetooth::Send(const scoped_refptr<net::IOBuffer>& buffer,
                                     int buffer_size) {
  VLOG(3) << "Starting sending of data with size = " << buffer_size;
  socket_->Send(buffer, buffer_size,
                base::Bind(&CopresenceSocketBluetooth::OnSendComplete,
                           weak_ptr_factory_.GetWeakPtr()),
                base::Bind(&CopresenceSocketBluetooth::OnSendError,
                           weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void CopresenceSocketBluetooth::Receive(const ReceiveCallback& callback) {
  VLOG(3) << "Starting Receive.";
  receiving_ = true;
  receive_callback_ = callback;
  socket_->Receive(kMaxReceiveBytes,
                   base::Bind(&CopresenceSocketBluetooth::OnReceive,
                              weak_ptr_factory_.GetWeakPtr()),
                   base::Bind(&CopresenceSocketBluetooth::OnReceiveError,
                              weak_ptr_factory_.GetWeakPtr()));
}

void CopresenceSocketBluetooth::OnSendComplete(int status) {
  VLOG(3) << "Send Completed. Status = " << status;
}

void CopresenceSocketBluetooth::OnSendError(const std::string& message) {
  LOG(ERROR) << "Bluetooth send error: " << message;
}

void CopresenceSocketBluetooth::OnReceive(
    int size,
    scoped_refptr<net::IOBuffer> io_buffer) {
  VLOG(3) << "Data received with size = " << size
          << " and receiving_ = " << receiving_;
  // Dispatch the data to the callback and go back to listening for more data.
  receive_callback_.Run(io_buffer, size);

  // We cancelled receiving due to an error. Don't post more receive tasks.
  if (!receiving_)
    return;

  // Post a task to delay the read until the socket is available, as
  // calling Receive again at this point would error with ERR_IO_PENDING.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&device::BluetoothSocket::Receive, socket_, kMaxReceiveBytes,
                 base::Bind(&CopresenceSocketBluetooth::OnReceive,
                            weak_ptr_factory_.GetWeakPtr()),
                 base::Bind(&CopresenceSocketBluetooth::OnReceiveError,
                            weak_ptr_factory_.GetWeakPtr())));
}

void CopresenceSocketBluetooth::OnReceiveError(
    device::BluetoothSocket::ErrorReason reason,
    const std::string& message) {
  LOG(ERROR) << "Bluetooth receive error: " << message;
  if (reason == device::BluetoothSocket::kIOPending)
    return;

  receiving_ = false;
}

}  // namespace copresence_endpoints
