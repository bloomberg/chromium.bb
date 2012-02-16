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

int SerialConnection::Read(unsigned char* byte) {
  return read(fd_, byte, 1);
}

int SerialConnection::Write(const std::string& data) {
  return write(fd_, data.c_str(), data.length());
}

}  // namespace extensions
