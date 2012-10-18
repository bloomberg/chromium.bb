// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_chromeos.h"

#include <vector>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include "base/logging.h"
#include "device/bluetooth/bluetooth_service_record.h"
#include "device/bluetooth/bluetooth_utils.h"

using device::BluetoothServiceRecord;
using device::BluetoothSocket;

namespace chromeos {

BluetoothSocketChromeOs::BluetoothSocketChromeOs(
    const std::string& address, int fd)
  : address_(address),
    fd_(fd) {
}

BluetoothSocketChromeOs::~BluetoothSocketChromeOs() {
  close(fd_);
}

// static
scoped_refptr<BluetoothSocket> BluetoothSocketChromeOs::CreateBluetoothSocket(
    const BluetoothServiceRecord& service_record) {
  BluetoothSocketChromeOs* bluetooth_socket = NULL;
  if (service_record.SupportsRfcomm()) {
    int socket_fd = socket(
        AF_BLUETOOTH, SOCK_STREAM | SOCK_NONBLOCK, BTPROTO_RFCOMM);
    struct sockaddr_rc socket_address = { 0 };
    socket_address.rc_family = AF_BLUETOOTH;
    socket_address.rc_channel = service_record.rfcomm_channel();
    device::bluetooth_utils::str2ba(service_record.address(),
        &socket_address.rc_bdaddr);

    int status = connect(socket_fd, (struct sockaddr *)&socket_address,
        sizeof(socket_address));
    int errsv = errno;
    if (status == 0 || errno == EINPROGRESS) {
      bluetooth_socket = new BluetoothSocketChromeOs(service_record.address(),
          socket_fd);
    } else {
      LOG(ERROR) << "Failed to connect bluetooth socket "
          << "(" << service_record.address() << "): "
          << "(" << errsv << ") " << strerror(errsv);
      close(socket_fd);
    }
  }
  // TODO(bryeung): add support for L2CAP sockets as well.

  return scoped_refptr<BluetoothSocketChromeOs>(bluetooth_socket);
}

int BluetoothSocketChromeOs::fd() const {
  return fd_;
}

}  // namespace chromeos
