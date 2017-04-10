// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_SHELL_PORT_MASH_TEST_API_H_
#define ASH_MUS_BRIDGE_SHELL_PORT_MASH_TEST_API_H_

#include "ash/mus/bridge/shell_port_mash.h"

namespace ash {
namespace mus {

// Use to get at internal state of ShellPortMash.
class ShellPortMashTestApi {
 public:
  ShellPortMashTestApi() {}
  ~ShellPortMashTestApi() {}

  AcceleratorControllerRegistrar* accelerator_controller_registrar() {
    return ShellPortMash::Get()->accelerator_controller_registrar_.get();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellPortMashTestApi);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_SHELL_PORT_MASH_TEST_API_H_
