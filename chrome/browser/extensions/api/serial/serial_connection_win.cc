// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <windows.h>

#include <string>

namespace extensions {

namespace {

int BitrateToSpeedConstant(int bitrate) {
#define BITRATE_TO_SPEED_CASE(x) case x: return CBR_ ## x;
  switch (bitrate) {
    BITRATE_TO_SPEED_CASE(110);
    BITRATE_TO_SPEED_CASE(300);
    BITRATE_TO_SPEED_CASE(600);
    BITRATE_TO_SPEED_CASE(1200);
    BITRATE_TO_SPEED_CASE(2400);
    BITRATE_TO_SPEED_CASE(4800);
    BITRATE_TO_SPEED_CASE(9600);
    BITRATE_TO_SPEED_CASE(14400);
    BITRATE_TO_SPEED_CASE(19200);
    BITRATE_TO_SPEED_CASE(38400);
    BITRATE_TO_SPEED_CASE(57600);
    BITRATE_TO_SPEED_CASE(115200);
    BITRATE_TO_SPEED_CASE(128000);
    BITRATE_TO_SPEED_CASE(256000);
    default:
      // If the bitrate doesn't match that of one of the standard
      // index constants, it may be provided as-is to the DCB
      // structure, according to MSDN.
      return bitrate;
  }
#undef BITRATE_TO_SPEED_CASE
}

int DataBitsEnumToConstant(api::serial::DataBits data_bits) {
  switch (data_bits) {
    case api::serial::DATA_BITS_SEVEN:
      return 7;
    case api::serial::DATA_BITS_EIGHT:
    default:
      return 8;
  }
}

int ParityBitEnumToConstant(api::serial::ParityBit parity_bit) {
  switch (parity_bit) {
    case api::serial::PARITY_BIT_EVEN:
      return EVENPARITY;
    case api::serial::PARITY_BIT_ODD:
      return SPACEPARITY;
    case api::serial::PARITY_BIT_NO:
    default:
      return NOPARITY;
  }
}

int StopBitsEnumToConstant(api::serial::StopBits stop_bits) {
  switch (stop_bits) {
    case api::serial::STOP_BITS_TWO:
      return TWOSTOPBITS;
    case api::serial::STOP_BITS_ONE:
    default:
      return ONESTOPBIT;
  }
}

int SpeedConstantToBitrate(int speed) {
#define SPEED_TO_BITRATE_CASE(x) case CBR_ ## x: return x;
  switch (speed) {
    SPEED_TO_BITRATE_CASE(110);
    SPEED_TO_BITRATE_CASE(300);
    SPEED_TO_BITRATE_CASE(600);
    SPEED_TO_BITRATE_CASE(1200);
    SPEED_TO_BITRATE_CASE(2400);
    SPEED_TO_BITRATE_CASE(4800);
    SPEED_TO_BITRATE_CASE(9600);
    SPEED_TO_BITRATE_CASE(14400);
    SPEED_TO_BITRATE_CASE(19200);
    SPEED_TO_BITRATE_CASE(38400);
    SPEED_TO_BITRATE_CASE(57600);
    SPEED_TO_BITRATE_CASE(115200);
    SPEED_TO_BITRATE_CASE(128000);
    SPEED_TO_BITRATE_CASE(256000);
    default:
      // If it's not one of the standard index constants,
      // it should be an integral baud rate, according to
      // MSDN.
      return speed;
  }
#undef SPEED_TO_BITRATE_CASE
}

api::serial::DataBits DataBitsConstantToEnum(int data_bits) {
  switch (data_bits) {
    case 7:
      return api::serial::DATA_BITS_SEVEN;
    case 8:
    default:
      return api::serial::DATA_BITS_EIGHT;
  }
}

api::serial::ParityBit ParityBitConstantToEnum(int parity_bit) {
  switch (parity_bit) {
    case EVENPARITY:
      return api::serial::PARITY_BIT_EVEN;
    case ODDPARITY:
      return api::serial::PARITY_BIT_ODD;
    case NOPARITY:
    default:
      return api::serial::PARITY_BIT_NO;
  }
}

api::serial::StopBits StopBitsConstantToEnum(int stop_bits) {
  switch (stop_bits) {
    case TWOSTOPBITS:
      return api::serial::STOP_BITS_TWO;
    case ONESTOPBIT:
    default:
      return api::serial::STOP_BITS_ONE;
  }
}

}  // namespace

bool SerialConnection::ConfigurePort(
    const api::serial::ConnectionOptions& options) {
  DCB config = { 0 };
  config.DCBlength = sizeof(config);
  if (!GetCommState(file_.GetPlatformFile(), &config)) {
    return false;
  }
  if (options.bitrate.get())
    config.BaudRate = BitrateToSpeedConstant(*options.bitrate);
  if (options.data_bits != api::serial::DATA_BITS_NONE)
    config.ByteSize = DataBitsEnumToConstant(options.data_bits);
  if (options.parity_bit != api::serial::PARITY_BIT_NONE)
    config.Parity = ParityBitEnumToConstant(options.parity_bit);
  if (options.stop_bits != api::serial::STOP_BITS_NONE)
    config.StopBits = StopBitsEnumToConstant(options.stop_bits);
  if (options.cts_flow_control.get()) {
    if (*options.cts_flow_control) {
      config.fOutxCtsFlow = TRUE;
      config.fRtsControl = RTS_CONTROL_HANDSHAKE;
    } else {
      config.fOutxCtsFlow = FALSE;
      config.fRtsControl = RTS_CONTROL_ENABLE;
    }
  }
  return SetCommState(file_.GetPlatformFile(), &config) != 0;
}

bool SerialConnection::PostOpen() {
  // A ReadIntervalTimeout of MAXDWORD will cause async reads to complete
  // immediately with any data that's available, even if there is none.
  // This is OK because we never issue a read request until WaitCommEvent
  // signals that data is available.
  COMMTIMEOUTS timeouts = { 0 };
  timeouts.ReadIntervalTimeout = MAXDWORD;
  if (!::SetCommTimeouts(file_.GetPlatformFile(), &timeouts)) {
    return false;
  }

  DCB config = { 0 };
  config.DCBlength = sizeof(config);
  if (!GetCommState(file_.GetPlatformFile(), &config)) {
    return false;
  }
  // Setup some sane default state.
  config.fBinary = TRUE;
  config.fParity = FALSE;
  config.fAbortOnError = TRUE;
  config.fOutxCtsFlow = FALSE;
  config.fOutxDsrFlow = FALSE;
  config.fRtsControl = RTS_CONTROL_ENABLE;
  config.fDtrControl = DTR_CONTROL_ENABLE;
  config.fDsrSensitivity = FALSE;
  config.fOutX = FALSE;
  config.fInX = FALSE;
  return SetCommState(file_.GetPlatformFile(), &config) != 0;
}

bool SerialConnection::Flush() const {
  return PurgeComm(file_.GetPlatformFile(), PURGE_RXCLEAR | PURGE_TXCLEAR) != 0;
}

bool SerialConnection::GetControlSignals(
    api::serial::DeviceControlSignals* signals) const {
  DWORD status;
  if (!GetCommModemStatus(file_.GetPlatformFile(), &status)) {
    return false;
  }
  signals->dcd = (status & MS_RLSD_ON) != 0;
  signals->cts = (status & MS_CTS_ON) != 0;
  signals->dsr = (status & MS_DSR_ON) != 0;
  signals->ri = (status & MS_RING_ON) != 0;
  return true;
}

bool SerialConnection::SetControlSignals(
    const api::serial::HostControlSignals& signals) {
  if (signals.dtr.get()) {
    if (!EscapeCommFunction(file_.GetPlatformFile(),
                            *signals.dtr ? SETDTR : CLRDTR)) {
      return false;
    }
  }
  if (signals.rts.get()) {
    if (!EscapeCommFunction(file_.GetPlatformFile(),
                            *signals.rts ? SETRTS : CLRRTS)) {
      return false;
    }
  }
  return true;
}

bool SerialConnection::GetPortInfo(api::serial::ConnectionInfo* info) const {
  DCB config = { 0 };
  config.DCBlength = sizeof(config);
  if (!GetCommState(file_.GetPlatformFile(), &config)) {
    return false;
  }
  info->bitrate.reset(new int(SpeedConstantToBitrate(config.BaudRate)));
  info->data_bits = DataBitsConstantToEnum(config.ByteSize);
  info->parity_bit = ParityBitConstantToEnum(config.Parity);
  info->stop_bits = StopBitsConstantToEnum(config.StopBits);
  info->cts_flow_control.reset(new bool(config.fOutxCtsFlow != 0));
  return true;
}

std::string SerialConnection::MaybeFixUpPortName(
    const std::string &port_name) {
  // For COM numbers less than 9, CreateFile is called with a string such as
  // "COM1". For numbers greater than 9, a prefix of "\\\\.\\" must be added.
  if (port_name.length() > std::string("COM9").length())
    return std::string("\\\\.\\").append(port_name);

  return port_name;
}

}  // namespace extensions
