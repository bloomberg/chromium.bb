// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_SHELL_MUS_TEST_API_H_
#define ASH_MUS_BRIDGE_WM_SHELL_MUS_TEST_API_H_

#include "ash/mus/bridge/wm_shell_mus.h"

namespace ash {
namespace mus {

// Use to get at internal state of WmShellMus.
class WmShellMusTestApi {
 public:
  WmShellMusTestApi() {}
  ~WmShellMusTestApi() {}

  AcceleratorControllerRegistrar* accelerator_controller_registrar() {
    return WmShellMus::Get()->accelerator_controller_registrar_.get();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WmShellMusTestApi);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_SHELL_MUS_TEST_API_H_
