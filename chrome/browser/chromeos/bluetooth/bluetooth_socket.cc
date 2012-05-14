// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_socket.h"

#include <vector>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>

#include "chrome/browser/chromeos/bluetooth/bluetooth_service_record.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_utils.h"

namespace chromeos {

BluetoothSocket::BluetoothSocket(const std::string& address, int fd)
  : address_(address),
    fd_(fd) {
}

BluetoothSocket::~BluetoothSocket() {
  close(fd_);
}

// static
scoped_refptr<BluetoothSocket> BluetoothSocket::CreateBluetoothSocket(
    const BluetoothServiceRecord& service_record) {
  BluetoothSocket* bluetooth_socket = NULL;
  if (service_record.SupportsRfcomm()) {
    int socket_fd = socket(
        AF_BLUETOOTH, SOCK_STREAM | SOCK_NONBLOCK, BTPROTO_RFCOMM);
    struct sockaddr_rc socket_address = { 0 };
    socket_address.rc_family = AF_BLUETOOTH;
    socket_address.rc_channel = service_record.rfcomm_channel();
    bluetooth_utils::str2ba(service_record.address(),
        &socket_address.rc_bdaddr);

    int status = connect(socket_fd, (struct sockaddr *)&socket_address,
        sizeof(socket_address));
    int errsv = errno;
    if (status == 0 || errno == EINPROGRESS) {
      bluetooth_socket = new BluetoothSocket(service_record.address(),
          socket_fd);
    } else {
      LOG(ERROR) << "Failed to connect bluetooth socket "
          << "(" << service_record.address() << "): "
          << "(" << errsv << ") " << strerror(errsv);
      close(socket_fd);
    }
  }
  // TODO(bryeung): add support for L2CAP sockets as well.

  return scoped_refptr<BluetoothSocket>(bluetooth_socket);
}

}  // namespace chromeos
