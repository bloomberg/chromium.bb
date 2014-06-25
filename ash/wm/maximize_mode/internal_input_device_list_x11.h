// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_INTERNAL_INPUT_DEVICE_LIST_X11_H_
#define ASH_WM_MAXIMIZE_MODE_INTERNAL_INPUT_DEVICE_LIST_X11_H_

#include <set>

#include "ash/wm/maximize_mode/internal_input_device_list.h"
#include "base/macros.h"

namespace ui {
class Event;
}

namespace ash {

// Identifies which input devices are internal and provides a helper function to
// test if an input event came from an internal device.
class InternalInputDeviceListX11 : public InternalInputDeviceList {
 public:
  InternalInputDeviceListX11();
  virtual ~InternalInputDeviceListX11();

  // InternalInputDeviceList:
  virtual bool IsEventFromInternalDevice(const ui::Event* event) OVERRIDE;

 private:
  // Tracks the device ids of internal input devices.
  std::set<int> internal_device_ids_;

  DISALLOW_COPY_AND_ASSIGN(InternalInputDeviceListX11);
};

}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_INTERNAL_INPUT_DEVICE_LIST_X11_H_
