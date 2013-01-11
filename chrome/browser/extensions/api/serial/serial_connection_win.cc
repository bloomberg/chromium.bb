// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <string>
#include <windows.h>

namespace extensions {

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

  if (bitrate_ >= 0) {
    bool speed_found = true;
    DWORD speed = CBR_9600;
    switch (bitrate_) {
      case 110: speed = CBR_110; break;
      case 300: speed = CBR_300; break;
      case 600: speed = CBR_600; break;
      case 1200: speed = CBR_1200; break;
      case 2400: speed = CBR_2400; break;
      case 4800: speed = CBR_4800; break;
      case 9600: speed = CBR_9600; break;
      case 14400: speed = CBR_14400; break;
      case 19200: speed = CBR_19200; break;
      case 38400: speed = CBR_38400; break;
      case 57600: speed = CBR_57600; break;
      case 115200: speed = CBR_115200; break;
      case 128000: speed = CBR_128000; break;
      case 256000: speed = CBR_256000; break;
      default: speed_found = false; break;
    }
    if (speed_found)
      dcb.BaudRate = speed;
  }
  dcb.ByteSize = 8;
  dcb.StopBits = ONESTOPBIT;
  dcb.Parity = NOPARITY;
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

bool SerialConnection::
SetControlSignals(const ControlSignals &control_signals) {
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
