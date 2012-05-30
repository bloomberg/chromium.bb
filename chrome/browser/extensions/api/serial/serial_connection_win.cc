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

  // TODO(miket): when we have a bit rate API, use it.
  dcb.BaudRate = CBR_57600;
  dcb.ByteSize = 8;
  dcb.StopBits = ONESTOPBIT;
  dcb.Parity = NOPARITY;
  if (!SetCommState(file_, &dcb))
    return false;

  return true;
}

}  // namespace extensions
