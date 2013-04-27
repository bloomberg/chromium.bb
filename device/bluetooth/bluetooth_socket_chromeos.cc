// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_chromeos.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "base/logging.h"
#include "base/safe_strerror_posix.h"
#include "device/bluetooth/bluetooth_service_record_chromeos.h"
#include "device/bluetooth/bluetooth_utils.h"
#include "net/base/io_buffer.h"

using device::BluetoothServiceRecord;
using device::BluetoothSocket;

namespace chromeos {

BluetoothSocketChromeOS::BluetoothSocketChromeOS(int fd) : fd_(fd) {
}

BluetoothSocketChromeOS::~BluetoothSocketChromeOS() {
  close(fd_);
}

// static
scoped_refptr<BluetoothSocket> BluetoothSocketChromeOS::CreateBluetoothSocket(
    const BluetoothServiceRecord& service_record) {
  BluetoothSocketChromeOS* bluetooth_socket = NULL;
  if (service_record.SupportsRfcomm()) {
    int socket_fd = socket(
        AF_BLUETOOTH, SOCK_STREAM | SOCK_NONBLOCK, BTPROTO_RFCOMM);
    struct sockaddr_rc socket_address = { 0 };
    socket_address.rc_family = AF_BLUETOOTH;
    socket_address.rc_channel = service_record.rfcomm_channel();
    const BluetoothServiceRecordChromeOS* service_record_chromeos =
        static_cast<const BluetoothServiceRecordChromeOS*>(&service_record);
    service_record_chromeos->GetBluetoothAddress(&socket_address.rc_bdaddr);

    int status = connect(socket_fd, (struct sockaddr *)&socket_address,
        sizeof(socket_address));
    int errsv = errno;
    if (status == 0 || errno == EINPROGRESS) {
      bluetooth_socket = new BluetoothSocketChromeOS(socket_fd);
    } else {
      LOG(ERROR) << "Failed to connect bluetooth socket "
          << "(" << service_record.address() << "): "
          << "(" << errsv << ") " << strerror(errsv);
      close(socket_fd);
    }
  }
  // TODO(bryeung): add support for L2CAP sockets as well.

  return scoped_refptr<BluetoothSocketChromeOS>(bluetooth_socket);
}

bool BluetoothSocketChromeOS::Receive(net::GrowableIOBuffer* buffer) {
  buffer->SetCapacity(1024);
  ssize_t bytes_read;
  do {
    if (buffer->RemainingCapacity() == 0)
      buffer->SetCapacity(buffer->capacity() * 2);
    bytes_read = read(fd_, buffer->data(), buffer->RemainingCapacity());
    if (bytes_read > 0)
      buffer->set_offset(buffer->offset() + bytes_read);
  } while (bytes_read > 0);

  if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    error_message_ = safe_strerror(errno);
    return false;
  }
  return true;
}

bool BluetoothSocketChromeOS::Send(net::DrainableIOBuffer* buffer) {
  ssize_t bytes_written;
  do {
    bytes_written = write(fd_, buffer->data(), buffer->BytesRemaining());
    if (bytes_written > 0)
      buffer->DidConsume(bytes_written);
  } while (buffer->BytesRemaining() > 0 && bytes_written > 0);

  if (bytes_written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
    error_message_ = safe_strerror(errno);
    return false;
  }
  return true;
}

std::string BluetoothSocketChromeOS::GetLastErrorMessage() const {
  return error_message_;
}

}  // namespace chromeos
