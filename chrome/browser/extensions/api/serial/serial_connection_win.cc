// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <windows.h>

#include <string>

namespace extensions {

namespace {
int getBaudRate(int bitrate_) {
  switch (bitrate_) {
    case 110: return CBR_110;
    case 300: return CBR_300;
    case 600: return CBR_600;
    case 1200: return CBR_1200;
    case 2400: return CBR_2400;
    case 4800: return CBR_4800;
    case 9600: return CBR_9600;
    case 14400: return CBR_14400;
    case 19200: return CBR_19200;
    case 38400: return CBR_38400;
    case 57600: return CBR_57600;
    case 115200: return CBR_115200;
    case 128000: return CBR_128000;
    case 256000: return CBR_256000;
    default: return CBR_9600;
  }
}

int getDataBit(serial::DataBit databit) {
  switch (databit) {
    case serial::DATA_BIT_SEVENBIT:
      return 7;
    case serial::DATA_BIT_EIGHTBIT:
    default:
      return 8;
  }
}

int getParity(serial::ParityBit parity) {
  switch (parity) {
    case serial::PARITY_BIT_EVENPARITY:
      return EVENPARITY;
    case serial::PARITY_BIT_ODDPARITY:
      return SPACEPARITY;
    case serial::PARITY_BIT_NOPARITY:
    default:
      return NOPARITY;
  }
}

int getStopBit(serial::StopBit stopbit) {
  switch (stopbit) {
    case serial::STOP_BIT_TWOSTOPBIT:
      return TWOSTOPBITS;
    case serial::STOP_BIT_ONESTOPBIT:
    default:
      return ONESTOPBIT;
  }
}
}  // namespace

bool SerialConnection::PostOpen() {
  // Set timeouts so that reads return immediately with whatever could be read
  // without blocking.
  COMMTIMEOUTS timeouts = { 0 };
  timeouts.ReadIntervalTimeout = MAXDWORD;
  if (!::SetCommTimeouts(file_, &timeouts))
    return false;

  DCB dcb = { 0 };
  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(file_, &dcb))
    return false;

  dcb.BaudRate = getBaudRate(bitrate_);
  dcb.ByteSize = getDataBit(databit_);
  dcb.Parity = getParity(parity_);
  dcb.StopBits = getStopBit(stopbit_);

  if (!SetCommState(file_, &dcb))
    return false;

  return true;
}

bool SerialConnection::GetControlSignals(ControlSignals &control_signals) {
  DWORD dwModemStatus;
  if (!GetCommModemStatus(file_, &dwModemStatus))
    return false;
  control_signals.dcd = (MS_RLSD_ON & dwModemStatus) != 0;
  control_signals.cts = (MS_CTS_ON & dwModemStatus) != 0;
  return true;
}

bool SerialConnection::SetControlSignals(
    const ControlSignals &control_signals) {
  if (control_signals.should_set_dtr) {
    if (!EscapeCommFunction(file_, control_signals.dtr ? SETDTR : CLRDTR))
      return false;
  }
  if (control_signals.should_set_rts) {
    if (!EscapeCommFunction(file_, control_signals.rts ? SETRTS : CLRRTS))
      return false;
  }
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
