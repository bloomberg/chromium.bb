// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef ATHENA_MAIN_DEBUG_ACCELERATOR_HANDLER_H_
#define ATHENA_MAIN_DEBUG_ACCELERATOR_HANDLER_H_

#include "athena/input/public/accelerator_manager.h"

#include "base/macros.h"

namespace aura {
class Window;
}

namespace athena {

// Handles accelerators useful for debugging.
class DebugAcceleratorHandler : public AcceleratorHandler {
 public:
  explicit DebugAcceleratorHandler(aura::Window* root_window);
  ~DebugAcceleratorHandler() override;

  // AcceleratorHandler:
  bool IsCommandEnabled(int command_id) const override;
  bool OnAcceleratorFired(int command_id,
                          const ui::Accelerator& accelerator) override;

 private:
  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(DebugAcceleratorHandler);
};

}  // namespace athena

#endif  // ATHENA_MAIN_DEBUG_ACCELERATOR_HANDLER_H_
