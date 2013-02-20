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
    int bitrate_opt_;
    switch (bitrate_) {
      case 0:
        bitrate_opt_ = B0;
        break;
      case 50:
        bitrate_opt_ = B50;
        break;
      case 75:
        bitrate_opt_ = B75;
        break;
      case 110:
        bitrate_opt_ = B110;
        break;
      case 134:
        bitrate_opt_ = B134;
        break;
      case 150:
        bitrate_opt_ = B150;
        break;
      case 200:
        bitrate_opt_ = B200;
        break;
      case 300:
        bitrate_opt_ = B300;
        break;
      case 600:
        bitrate_opt_ = B600;
        break;
      case 1200:
        bitrate_opt_ = B1200;
        break;
      case 1800:
        bitrate_opt_ = B1800;
        break;
      case 2400:
        bitrate_opt_ = B2400;
        break;
      case 4800:
        bitrate_opt_ = B4800;
        break;
      case 9600:
        bitrate_opt_ = B9600;
        break;
      case 19200:
        bitrate_opt_ = B19200;
        break;
      case 38400:
        bitrate_opt_ = B38400;
        break;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
      case 57600:
        bitrate_opt_ = B57600;
        break;
      case 115200:
        bitrate_opt_ = B115200;
        break;
      case 230400:
        bitrate_opt_ = B230400;
        break;
      case 460800:
        bitrate_opt_ = B460800;
        break;
      case 576000:
        bitrate_opt_ = B576000;
        break;
      case 921600:
        bitrate_opt_ = B921600;
        break;
      default:
        bitrate_opt_ = B9600;
#else
// MACOSX doesn't define constants bigger than 38400.
// So if it is MACOSX and the value doesn't fit any of the defined constants
// It will setup the bitrate with 'bitrate_' (just forwarding the value)
      default:
        bitrate_opt_ = bitrate_;
#endif
    }

    cfsetispeed(&options, bitrate_opt_);
    cfsetospeed(&options, bitrate_opt_);
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

bool SerialConnection::SetControlSignals(
    const ControlSignals &control_signals) {
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
