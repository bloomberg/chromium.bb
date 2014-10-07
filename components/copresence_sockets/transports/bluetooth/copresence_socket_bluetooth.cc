// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence_sockets/transports/bluetooth/copresence_socket_bluetooth.h"

#include "base/bind.h"
#include "base/strings/string_piece.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "net/base/io_buffer.h"

namespace {

// TODO(rkc): This number is totally arbitrary. Figure out what this should be.
const int kMaxReceiveBytes = 4096;

}  // namespace

namespace copresence_sockets {

CopresenceSocketBluetooth::CopresenceSocketBluetooth(
    const scoped_refptr<device::BluetoothSocket>& socket)
    : socket_(socket), weak_ptr_factory_(this) {
}

CopresenceSocketBluetooth::~CopresenceSocketBluetooth() {
}

bool CopresenceSocketBluetooth::Send(const scoped_refptr<net::IOBuffer>& buffer,
                                     int buffer_size) {
  socket_->Send(buffer,
                buffer_size,
                base::Bind(&CopresenceSocketBluetooth::OnSendComplete,
                           weak_ptr_factory_.GetWeakPtr()),
                base::Bind(&CopresenceSocketBluetooth::OnSendError,
                           weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void CopresenceSocketBluetooth::Receive(const ReceiveCallback& callback) {
  receive_callback_ = callback;
  socket_->Receive(kMaxReceiveBytes,
                   base::Bind(&CopresenceSocketBluetooth::OnReceive,
                              weak_ptr_factory_.GetWeakPtr()),
                   base::Bind(&CopresenceSocketBluetooth::OnReceiveError,
                              weak_ptr_factory_.GetWeakPtr()));
}

void CopresenceSocketBluetooth::OnSendComplete(int status) {
}

void CopresenceSocketBluetooth::OnSendError(const std::string& message) {
  LOG(ERROR) << "Bluetooth send error: " << message;
}

void CopresenceSocketBluetooth::OnReceive(
    int size,
    scoped_refptr<net::IOBuffer> io_buffer) {
  // Dispatch the data to the callback and go back to listening for more data.
  receive_callback_.Run(io_buffer, size);
  socket_->Receive(kMaxReceiveBytes,
                   base::Bind(&CopresenceSocketBluetooth::OnReceive,
                              weak_ptr_factory_.GetWeakPtr()),
                   base::Bind(&CopresenceSocketBluetooth::OnReceiveError,
                              weak_ptr_factory_.GetWeakPtr()));
}

void CopresenceSocketBluetooth::OnReceiveError(
    device::BluetoothSocket::ErrorReason /* reason */,
    const std::string& message) {
  LOG(ERROR) << "Bluetooth receive error: " << message;
}

}  // namespace copresence_sockets
