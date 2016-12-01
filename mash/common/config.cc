// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/common/config.h"

#include "base/command_line.h"

namespace mash {
namespace common {

const char kWindowManagerSwitch[] = "window-manager";

std::string GetWindowManagerServiceName() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kWindowManagerSwitch)) {
    std::string service_name =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kWindowManagerSwitch);
    return service_name;
  }
  // TODO(beng): move this constant to a mojom file in //ash.
  return "ash";
}

}  // namespace common
}  // namespace mash
