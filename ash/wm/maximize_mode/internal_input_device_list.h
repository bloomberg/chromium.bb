// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_INTERNAL_INPUT_DEVICE_LIST_H_
#define ASH_WM_MAXIMIZE_MODE_INTERNAL_INPUT_DEVICE_LIST_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ui {
class Event;
}

namespace ash {

// Identifies which input devices are internal and provides a helper function to
// test if an input event came from an internal device.
class ASH_EXPORT InternalInputDeviceList {
 public:
  InternalInputDeviceList() {}
  virtual ~InternalInputDeviceList() {}

  virtual bool IsEventFromInternalDevice(const ui::Event* event) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalInputDeviceList);
};

}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_INTERNAL_INPUT_DEVICE_LIST_H_
