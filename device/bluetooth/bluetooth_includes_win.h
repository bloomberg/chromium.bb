// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_INCLUDES_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_INCLUDES_WIN_H_

// windows.h needs to be included before BluetoothAPIs.h.
#include <windows.h>

#include <BluetoothAPIs.h>
#if defined(_WIN32_WINNT_WIN8) && _MSC_VER < 1700
// The Windows 8 SDK defines FACILITY_VISUALCPP in winerror.h.
#undef FACILITY_VISUALCPP
#endif
#include <delayimp.h>

#pragma comment(lib, "Bthprops.lib")

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_INCLUDES_WIN_H_