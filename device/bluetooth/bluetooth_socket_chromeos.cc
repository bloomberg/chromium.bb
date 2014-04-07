// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_socket_chromeos.h"

#include <errno.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/posix/eintr_wrapper.h"
#include "base/safe_strerror_posix.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/file_descriptor.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "net/base/io_buffer.h"

namespace chromeos {

BluetoothSocketChromeOS::BluetoothSocketChromeOS(int fd)
    : fd_(fd) {
  // Fetch the socket type so we read from it correctly.
  int optval;
  socklen_t opt_len = sizeof optval;
  if (getsockopt(fd_, SOL_SOCKET, SO_TYPE, &optval, &opt_len) < 0) {
    // Sequenced packet is the safest assumption since it won't result in
    // truncated packets.
    LOG(WARNING) << "Unable to get socket type: " << safe_strerror(errno);
    optval = SOCK_SEQPACKET;
  }

  if (optval == SOCK_DGRAM || optval == SOCK_SEQPACKET) {
    socket_type_ = L2CAP;
  } else {
    socket_type_ = RFCOMM;
  }
}

BluetoothSocketChromeOS::~BluetoothSocketChromeOS() {
  close(fd_);
}

bool BluetoothSocketChromeOS::Receive(net::GrowableIOBuffer *buffer) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (socket_type_ == L2CAP) {
    int count;
    if (ioctl(fd_, FIONREAD, &count) < 0) {
      error_message_ = safe_strerror(errno);
      LOG(WARNING) << "Unable to get waiting data size: " << error_message_;
      return true;
    }

    // No bytes waiting can mean either nothing to read, or the other end has
    // been closed, and reading zero bytes always returns zero.
    //
    // We can't do a short read for fear of a race where data arrives between
    // calls and we trunctate it. So use poll() to check for the POLLHUP flag.
    if (count == 0) {
      struct pollfd pollfd;

      pollfd.fd = fd_;
      pollfd.events = 0;
      pollfd.revents = 0;

      // Timeout parameter set to 0 so this call will not block.
      if (HANDLE_EINTR(poll(&pollfd, 1, 0)) < 0) {
        error_message_ = safe_strerror(errno);
        LOG(WARNING) << "Unable to check whether socket is closed: "
                     << error_message_;
        return false;
      }

      if (pollfd.revents & POLLHUP) {
        // TODO(keybuk, youngki): Agree a common way to flag disconnected.
        error_message_ = "Disconnected";
        return false;
      }
    }

    buffer->SetCapacity(count);
  } else {
    buffer->SetCapacity(1024);
  }

  ssize_t bytes_read;
  do {
    if (buffer->RemainingCapacity() == 0)
      buffer->SetCapacity(buffer->capacity() * 2);
    bytes_read =
        HANDLE_EINTR(read(fd_, buffer->data(), buffer->RemainingCapacity()));
    if (bytes_read > 0)
      buffer->set_offset(buffer->offset() + bytes_read);
  } while (socket_type_ == RFCOMM && bytes_read > 0);

  // Ignore an error if at least one read() call succeeded; it'll be returned
  // the next read() call.
  if (buffer->offset() > 0)
    return true;

  if (bytes_read < 0) {
    if (errno == ECONNRESET || errno == ENOTCONN) {
      // TODO(keybuk, youngki): Agree a common way to flag disconnected.
      error_message_ = "Disconnected";
      return false;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
      error_message_ = safe_strerror(errno);
      return false;
    }
  }

  if (bytes_read == 0 && socket_type_ == RFCOMM) {
    // TODO(keybuk, youngki): Agree a common way to flag disconnected.
    error_message_ = "Disconnected";
    return false;
  }

  return true;
}

bool BluetoothSocketChromeOS::Send(net::DrainableIOBuffer *buffer) {
  base::ThreadRestrictions::AssertIOAllowed();

  ssize_t bytes_written;
  do {
    bytes_written =
        HANDLE_EINTR(write(fd_, buffer->data(), buffer->BytesRemaining()));
    if (bytes_written > 0)
      buffer->DidConsume(bytes_written);
  } while (buffer->BytesRemaining() > 0 && bytes_written > 0);

  if (bytes_written < 0) {
    if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN) {
      // TODO(keybuk, youngki): Agree a common way to flag disconnected.
      error_message_ = "Disconnected";
      return false;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
      error_message_ = safe_strerror(errno);
      return false;
    }
  }

  return true;
}

std::string BluetoothSocketChromeOS::GetLastErrorMessage() const {
  return error_message_;
}

// static
scoped_refptr<device::BluetoothSocket> BluetoothSocketChromeOS::Create(
    dbus::FileDescriptor* fd) {
  DCHECK(fd->is_valid());

  BluetoothSocketChromeOS* bluetooth_socket =
      new BluetoothSocketChromeOS(fd->TakeValue());
  return scoped_refptr<BluetoothSocketChromeOS>(bluetooth_socket);
}

}  // namespace chromeos
