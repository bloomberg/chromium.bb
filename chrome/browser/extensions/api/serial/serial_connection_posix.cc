// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <sys/ioctl.h>
#include <termios.h>

namespace extensions {

bool SerialConnection::PostOpen() {
  struct termios options;

  // Start with existing options and modify.
  tcgetattr(file_, &options);

  // Bitrate (sometimes erroneously referred to as baud rate).
  if (bitrate_ >= 0) {
    cfsetispeed(&options, bitrate_);
    cfsetospeed(&options, bitrate_);
  }

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

bool SerialConnection::GetControlSignals(ControlSignals &control_signals) {
  int status;
  if (ioctl(file_, TIOCMGET, &status) == 0) {
    control_signals.dcd = (status & TIOCM_CAR) != 0;
    control_signals.cts = (status & TIOCM_CTS) != 0;
    return true;
  }
  return false;
}

bool SerialConnection::
SetControlSignals(const ControlSignals &control_signals) {
  int status;

  if (ioctl(file_, TIOCMGET, &status) != 0)
    return false;

  if (control_signals.should_set_dtr) {
    if (control_signals.dtr)
      status |= TIOCM_DTR;
    else
      status &= ~TIOCM_DTR;
  }
  if (control_signals.should_set_rts) {
    if (control_signals.rts)
      status |= TIOCM_RTS;
    else
      status &= ~TIOCM_RTS;
  }

  return ioctl(file_, TIOCMSET, &status) == 0;
}

std::string SerialConnection::MaybeFixUpPortName(
    const std::string &port_name) {
  return port_name;
}

}  // namespace extensions
