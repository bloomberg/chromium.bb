// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <sys/ioctl.h>
#include <termios.h>

#if defined(OS_LINUX)
#include <linux/serial.h>
#endif

namespace extensions {

namespace {

// Convert an integral bit rate to a nominal one. Returns |true|
// if the conversion was successful and |false| otherwise.
bool BitrateToSpeedConstant(int bitrate, speed_t* speed) {
#define BITRATE_TO_SPEED_CASE(x) case x: *speed = B ## x; return true;
  switch (bitrate) {
    BITRATE_TO_SPEED_CASE(0)
    BITRATE_TO_SPEED_CASE(50)
    BITRATE_TO_SPEED_CASE(75)
    BITRATE_TO_SPEED_CASE(110)
    BITRATE_TO_SPEED_CASE(134)
    BITRATE_TO_SPEED_CASE(150)
    BITRATE_TO_SPEED_CASE(200)
    BITRATE_TO_SPEED_CASE(300)
    BITRATE_TO_SPEED_CASE(600)
    BITRATE_TO_SPEED_CASE(1200)
    BITRATE_TO_SPEED_CASE(1800)
    BITRATE_TO_SPEED_CASE(2400)
    BITRATE_TO_SPEED_CASE(4800)
    BITRATE_TO_SPEED_CASE(9600)
    BITRATE_TO_SPEED_CASE(19200)
    BITRATE_TO_SPEED_CASE(38400)
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    BITRATE_TO_SPEED_CASE(57600)
    BITRATE_TO_SPEED_CASE(115200)
    BITRATE_TO_SPEED_CASE(230400)
    BITRATE_TO_SPEED_CASE(460800)
    BITRATE_TO_SPEED_CASE(576000)
    BITRATE_TO_SPEED_CASE(921600)
#endif
    default:
      return false;
  }
#undef BITRATE_TO_SPEED_CASE
}

// Convert a known nominal speed into an integral bitrate. Returns |true|
// if the conversion was successful and |false| otherwise.
bool SpeedConstantToBitrate(speed_t speed, int* bitrate) {
#define SPEED_TO_BITRATE_CASE(x) case B ## x: *bitrate = x; return true;
  switch (speed) {
    SPEED_TO_BITRATE_CASE(0)
    SPEED_TO_BITRATE_CASE(50)
    SPEED_TO_BITRATE_CASE(75)
    SPEED_TO_BITRATE_CASE(110)
    SPEED_TO_BITRATE_CASE(134)
    SPEED_TO_BITRATE_CASE(150)
    SPEED_TO_BITRATE_CASE(200)
    SPEED_TO_BITRATE_CASE(300)
    SPEED_TO_BITRATE_CASE(600)
    SPEED_TO_BITRATE_CASE(1200)
    SPEED_TO_BITRATE_CASE(1800)
    SPEED_TO_BITRATE_CASE(2400)
    SPEED_TO_BITRATE_CASE(4800)
    SPEED_TO_BITRATE_CASE(9600)
    SPEED_TO_BITRATE_CASE(19200)
    SPEED_TO_BITRATE_CASE(38400)
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    SPEED_TO_BITRATE_CASE(57600)
    SPEED_TO_BITRATE_CASE(115200)
    SPEED_TO_BITRATE_CASE(230400)
    SPEED_TO_BITRATE_CASE(460800)
    SPEED_TO_BITRATE_CASE(576000)
    SPEED_TO_BITRATE_CASE(921600)
#endif
    default:
      return false;
  }
#undef SPEED_TO_BITRATE_CASE
}

bool SetCustomBitrate(base::PlatformFile file,
                      struct termios* config,
                      int bitrate) {
#if defined(OS_LINUX)
  struct serial_struct serial;
  if (ioctl(file, TIOCGSERIAL, &serial) < 0) {
    return false;
  }
  serial.flags = (serial.flags & ~ASYNC_SPD_MASK) | ASYNC_SPD_CUST;
  serial.custom_divisor = serial.baud_base / bitrate;
  if (serial.custom_divisor < 1) {
    serial.custom_divisor = 1;
  }
  cfsetispeed(config, B38400);
  cfsetospeed(config, B38400);
  return ioctl(file, TIOCSSERIAL, &serial) >= 0;
#elif defined(OS_MACOSX)
  speed_t speed = static_cast<speed_t>(bitrate);
  cfsetispeed(config, speed);
  cfsetospeed(config, speed);
  return true;
#else
  return false;
#endif
}

}  // namespace

bool SerialConnection::ConfigurePort(
    const api::serial::ConnectionOptions& options) {
  struct termios config;
  tcgetattr(file_, &config);
  if (options.bitrate.get()) {
    if (*options.bitrate >= 0) {
      speed_t bitrate_opt = B0;
      if (BitrateToSpeedConstant(*options.bitrate, &bitrate_opt)) {
        cfsetispeed(&config, bitrate_opt);
        cfsetospeed(&config, bitrate_opt);
      } else {
        // Attempt to set a custom speed.
        if (!SetCustomBitrate(file_, &config, *options.bitrate)) {
          return false;
        }
      }
    }
  }
  if (options.data_bits != api::serial::DATA_BITS_NONE) {
    config.c_cflag &= ~CSIZE;
    switch (options.data_bits) {
      case api::serial::DATA_BITS_SEVEN:
        config.c_cflag |= CS7;
        break;
      case api::serial::DATA_BITS_EIGHT:
      default:
        config.c_cflag |= CS8;
        break;
    }
  }
  if (options.parity_bit != api::serial::PARITY_BIT_NONE) {
    switch (options.parity_bit) {
      case api::serial::PARITY_BIT_EVEN:
        config.c_cflag |= PARENB;
        config.c_cflag &= ~PARODD;
        break;
      case api::serial::PARITY_BIT_ODD:
        config.c_cflag |= (PARODD | PARENB);
        break;
      case api::serial::PARITY_BIT_NO:
      default:
        config.c_cflag &= ~(PARODD | PARENB);
        break;
    }
  }
  if (options.stop_bits != api::serial::STOP_BITS_NONE) {
    switch (options.stop_bits) {
      case api::serial::STOP_BITS_TWO:
        config.c_cflag |= CSTOPB;
        break;
      case api::serial::STOP_BITS_ONE:
      default:
        config.c_cflag &= ~CSTOPB;
        break;
    }
  }
  if (options.cts_flow_control.get()) {
    if (*options.cts_flow_control){
      config.c_cflag |= CRTSCTS;
    } else {
      config.c_cflag &= ~CRTSCTS;
    }
  }
  return tcsetattr(file_, TCSANOW, &config) == 0;
}

bool SerialConnection::PostOpen() {
  struct termios config;
  tcgetattr(file_, &config);

  // Set flags for 'raw' operation
  config.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);
  config.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                      ICRNL | IXON);
  config.c_oflag &= ~OPOST;

  // CLOCAL causes the system to disregard the DCD signal state.
  // CREAD enables reading from the port.
  config.c_cflag |= (CLOCAL | CREAD);

  return tcsetattr(file_, TCSANOW, &config) == 0;
}

bool SerialConnection::Flush() const {
  return tcflush(file_, TCIOFLUSH) == 0;
}

bool SerialConnection::GetControlSignals(
    api::serial::DeviceControlSignals* signals) const {
  int status;
  if (ioctl(file_, TIOCMGET, &status) == -1) {
    return false;
  }

  signals->dcd = (status & TIOCM_CAR) != 0;
  signals->cts = (status & TIOCM_CTS) != 0;
  signals->dsr = (status & TIOCM_DSR) != 0;
  signals->ri = (status & TIOCM_RI) != 0;
  return true;
}

bool SerialConnection::SetControlSignals(
    const api::serial::HostControlSignals& signals) {
  int status;

  if (ioctl(file_, TIOCMGET, &status) == -1) {
    return false;
  }

  if (signals.dtr.get()) {
    if (*signals.dtr) {
      status |= TIOCM_DTR;
    } else {
      status &= ~TIOCM_DTR;
    }
  }

  if (signals.rts.get()) {
    if (*signals.rts){
      status |= TIOCM_RTS;
    } else{
      status &= ~TIOCM_RTS;
    }
  }

  return ioctl(file_, TIOCMSET, &status) == 0;
}

bool SerialConnection::GetPortInfo(api::serial::ConnectionInfo* info) const {
  struct termios config;
  if (tcgetattr(file_, &config) == -1) {
    return false;
  }
  speed_t ispeed = cfgetispeed(&config);
  speed_t ospeed = cfgetospeed(&config);
  if (ispeed == ospeed) {
    int bitrate = 0;
    if (SpeedConstantToBitrate(ispeed, &bitrate)) {
      info->bitrate.reset(new int(bitrate));
    } else if (ispeed > 0) {
      info->bitrate.reset(new int(static_cast<int>(ispeed)));
    }
  }
  if ((config.c_cflag & CSIZE) == CS7) {
    info->data_bits = api::serial::DATA_BITS_SEVEN;
  } else if ((config.c_cflag & CSIZE) == CS8) {
    info->data_bits = api::serial::DATA_BITS_EIGHT;
  } else {
    info->data_bits = api::serial::DATA_BITS_NONE;
  }
  if (config.c_cflag & PARENB) {
    info->parity_bit = (config.c_cflag & PARODD) ? api::serial::PARITY_BIT_ODD
                                                 : api::serial::PARITY_BIT_EVEN;
  } else {
    info->parity_bit = api::serial::PARITY_BIT_NO;
  }
  info->stop_bits = (config.c_cflag & CSTOPB) ? api::serial::STOP_BITS_TWO
                                              : api::serial::STOP_BITS_ONE;
  info->cts_flow_control.reset(new bool((config.c_cflag & CRTSCTS) != 0));
  return true;
}

std::string SerialConnection::MaybeFixUpPortName(
    const std::string &port_name) {
  return port_name;
}

}  // namespace extensions
