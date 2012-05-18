// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

SerialConnection::SerialConnection(const std::string& port,
                                   APIResourceEventNotifier* event_notifier)
    : APIResource(APIResource::SerialConnectionResource, event_notifier),
      port_(port), fd_(0) {
}

SerialConnection::~SerialConnection() {
}

bool SerialConnection::Open() {
  fd_ = open(port_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
  return (fd_ > 0);
}

void SerialConnection::Close() {
  if (fd_ > 0) {
    close(fd_);
    fd_ = 0;
  }
}

int SerialConnection::Read(uint8* byte) {
  return read(fd_, byte, 1);
}

int SerialConnection::Write(scoped_refptr<net::IOBuffer> io_buffer,
                            int byte_count) {
  return write(fd_, io_buffer.get(), byte_count);
}

}  // namespace extensions
