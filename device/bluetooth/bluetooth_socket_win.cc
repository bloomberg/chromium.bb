// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_win.h"

#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "device/bluetooth/bluetooth_init_win.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "net/base/io_buffer.h"
#include "net/base/winsock_init.h"

namespace {

std::string FormatErrorMessage(DWORD error_code) {
  TCHAR error_msg[1024];
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                0,
                error_code,
                0,
                error_msg,
                1024,
                NULL);
  return base::SysWideToUTF8(error_msg);
}

}  // namespace

namespace device {

BluetoothSocketWin::BluetoothSocketWin(SOCKET fd) : fd_(fd) {
}

BluetoothSocketWin::~BluetoothSocketWin() {
  closesocket(fd_);
}

// static
scoped_refptr<BluetoothSocket> BluetoothSocketWin::CreateBluetoothSocket(
    const BluetoothServiceRecord& service_record) {
  BluetoothSocketWin* bluetooth_socket = NULL;
  if (service_record.SupportsRfcomm()) {
    net::EnsureWinsockInit();
    SOCKET socket_fd = socket(AF_BTH, SOCK_STREAM, BTHPROTO_RFCOMM);
    SOCKADDR_BTH sa;
    ZeroMemory(&sa, sizeof(sa));
    sa.addressFamily = AF_BTH;
    sa.port = service_record.rfcomm_channel();
    const BluetoothServiceRecordWin* service_record_win =
        static_cast<const BluetoothServiceRecordWin*>(&service_record);
    sa.btAddr = service_record_win->bth_addr();

    int status = connect(socket_fd,
                         reinterpret_cast<SOCKADDR *>(&sa),
                         sizeof(sa));
    DWORD error_code = WSAGetLastError();
    if (status == 0 || error_code == WSAEINPROGRESS) {
      bluetooth_socket =
          new BluetoothSocketWin(socket_fd);
    } else {
      LOG(ERROR) << "Failed to connect bluetooth socket "
          << "(" << service_record.address() << "): "
          << "(" << error_code << ")" << FormatErrorMessage(error_code);
      closesocket(socket_fd);
    }
  }
  // TODO(youngki) add support for L2CAP sockets as well.

  return scoped_refptr<BluetoothSocketWin>(bluetooth_socket);
}

bool BluetoothSocketWin::Receive(net::GrowableIOBuffer* buffer) {
  buffer->SetCapacity(1024);
  int bytes_read;
  do {
    if (buffer->RemainingCapacity() == 0)
      buffer->SetCapacity(buffer->capacity() * 2);
    bytes_read = recv(fd_, buffer->data(), buffer->RemainingCapacity(), 0);
    if (bytes_read > 0)
      buffer->set_offset(buffer->offset() + bytes_read);
  } while (bytes_read > 0);

  DWORD error_code = WSAGetLastError();
  if (bytes_read < 0 && error_code != WSAEWOULDBLOCK) {
    error_message_ = FormatErrorMessage(error_code);
    return false;
  }
  return true;
}

bool BluetoothSocketWin::Send(net::DrainableIOBuffer* buffer) {
  int bytes_written;
  do {
    bytes_written = send(fd_, buffer->data(), buffer->BytesRemaining(), 0);
    if (bytes_written > 0)
      buffer->DidConsume(bytes_written);
  } while (buffer->BytesRemaining() > 0 && bytes_written > 0);

  DWORD error_code = WSAGetLastError();
  if (bytes_written < 0 && error_code != WSAEWOULDBLOCK) {
    error_message_ = FormatErrorMessage(error_code);
    return false;
  }
  return true;
}

std::string BluetoothSocketWin::GetLastErrorMessage() const {
  return error_message_;
}

}  // namespace device
