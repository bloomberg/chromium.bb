// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEFS_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEFS_WIN_H_

#include <windows.h>
#include <cfg.h>
#include <devpkey.h>
#include <setupapi.h>

#pragma warning(push)
// bthledef.h is buggy and contains
//   #pragma pop
// which should be
//   #pragma warning(pop)
// So, we disable the "unknown pragma" warning, then actually pop, and then pop
// our disabling of 4068.
#pragma warning(disable: 4068)
#include <bthledef.h>
#pragma warning(pop)
#pragma warning(pop)

#include <bluetoothapis.h>
#include <bluetoothleapis.h>

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_DEFS_WIN_H_
