// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SHUTDOWN_OBSERVER_CHROMEOS_H_
#define ASH_DISPLAY_SHUTDOWN_OBSERVER_CHROMEOS_H_

#include "ash/common/shell_observer.h"
#include "base/macros.h"

namespace display {
class DisplayConfigurator;
}

namespace ash {

// Adds self as ShellObserver and listens for OnAppTerminating() on behalf of
// |display_configurator_|.
class ShutdownObserver : public ShellObserver {
 public:
  explicit ShutdownObserver(display::DisplayConfigurator* display_configurator);
  ~ShutdownObserver() override;

 private:
  // ShellObserver:
  void OnAppTerminating() override;

  display::DisplayConfigurator* display_configurator_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownObserver);
};

}  // namespace ash

#endif  // ASH_DISPLAY_SHUTDOWN_OBSERVER_CHROMEOS_H_
