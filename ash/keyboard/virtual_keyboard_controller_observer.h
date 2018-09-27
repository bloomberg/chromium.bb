// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTROLLER_OBSERVER_H_
#define ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTROLLER_OBSERVER_H_

#include "base/observer_list.h"

namespace ash {

class VirtualKeyboardControllerObserver : public base::CheckedObserver {
 public:
  virtual void OnVirtualKeyboardStateChanged(bool enabled) = 0;

 protected:
  ~VirtualKeyboardControllerObserver() override = default;
};

}  // namespace ash

#endif  // ASH_KEYBOARD_VIRTUAL_KEYBOARD_CONTROLLER_OBSERVER_H_
