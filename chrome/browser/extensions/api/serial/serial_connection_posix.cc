// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <sys/ioctl.h>
#include <termios.h>

namespace extensions {

namespace {
  int getBaudRate(int bitrate_) {
    switch (bitrate_) {
      case 0:
        return B0;
      case 50:
        return B50;
      case 75:
        return B75;
      case 110:
        return B110;
      case 134:
        return B134;
      case 150:
        return B150;
      case 200:
        return B200;
      case 300:
        return B300;
      case 600:
        return B600;
      case 1200:
        return B1200;
      case 1800:
        return B1800;
      case 2400:
        return B2400;
      case 4800:
        return B4800;
      case 9600:
        return B9600;
      case 19200:
        return B19200;
      case 38400:
        return B38400;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
      case 57600:
        return B57600;
      case 115200:
        return B115200;
      case 230400:
        return B230400;
      case 460800:
        return B460800;
      case 576000:
        return B576000;
      case 921600:
        return B921600;
      default:
        return B9600;
#else
// MACOSX doesn't define constants bigger than 38400.
// So if it is MACOSX and the value doesn't fit any of the defined constants
// It will setup the bitrate with 'bitrate_' (just forwarding the value)
      default:
        return bitrate_;
#endif
    }
  }
}  // namespace

bool SerialConnection::PostOpen() {
  struct termios options;

  // Start with existing options and modify.
  tcgetattr(file_, &options);

  // Bitrate (sometimes erroneously referred to as baud rate).
  if (bitrate_ >= 0) {
    int bitrate_opt_ = getBaudRate(bitrate_);

    cfsetispeed(&options, bitrate_opt_);
    cfsetospeed(&options, bitrate_opt_);
  }

  options.c_cflag &= ~CSIZE;
  switch (databit_) {
    case serial::DATA_BIT_SEVENBIT:
      options.c_cflag |= CS7;
      break;
    case serial::DATA_BIT_EIGHTBIT:
    default:
      options.c_cflag |= CS8;
      break;
  }
  switch (stopbit_) {
    case serial::STOP_BIT_TWOSTOPBIT:
      options.c_cflag |= CSTOPB;
      break;
    case serial::STOP_BIT_ONESTOPBIT:
    default:
      options.c_cflag &= ~CSTOPB;
      break;
  }
  switch (parity_) {
    case serial::PARITY_BIT_EVENPARITY:
      options.c_cflag |= PARENB;
      options.c_cflag &= ~PARODD;
      break;
    case serial::PARITY_BIT_ODDPARITY:
      options.c_cflag |= (PARENB | PARODD);
      break;
    case serial::PARITY_BIT_NOPARITY:
    default:
      options.c_cflag &= ~(PARENB | PARODD);
      break;
  }

  // Set flags for 'raw' operation
  // At least on Linux the flags are persistent and thus we cannot trust
  // the default values.
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);
  options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                       ICRNL | IXON);
  options.c_oflag &= ~OPOST;

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
