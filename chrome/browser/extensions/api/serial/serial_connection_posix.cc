// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <termios.h>

namespace extensions {

bool SerialConnection::PostOpen() {
  struct termios options;

  // Start with existing options and modify.
  tcgetattr(file_, &options);

  // Bitrate 57,600
  cfsetispeed(&options, B57600);
  cfsetospeed(&options, B57600);

  // 8N1
  options.c_cflag &= ~PARENB;
  options.c_cflag &= ~CSTOPB;
  options.c_cflag &= ~CSIZE;
  options.c_cflag |= CS8;
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

  // Enable receiver and set local mode
  // See http://www.easysw.com/~mike/serial/serial.html to understand.
  options.c_cflag |= (CLOCAL | CREAD);

  // Write the options.
  tcsetattr(file_, TCSANOW, &options);

  return true;
}

}  // namespace extensions
