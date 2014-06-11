// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/input/public/accelerator_manager.h"

#include "base/macros.h"

namespace aura {
class Window;
}

namespace athena {

// Handles screen related accelerators.
class ScreenAcceleratorHandler : public AcceleratorHandler {
 public:
  explicit ScreenAcceleratorHandler(aura::Window* root_window);

 private:
  virtual ~ScreenAcceleratorHandler();

  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const OVERRIDE;
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) OVERRIDE;

  aura::Window* root_window_;

  DISALLOW_COPY_AND_ASSIGN(ScreenAcceleratorHandler);
};

}  // namespace athena
