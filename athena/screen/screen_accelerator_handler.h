// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef ATHENA_SCREEN_SCREEN_ACCELERATOR_HANDLER_H_
#define ATHENA_SCREEN_SCREEN_ACCELERATOR_HANDLER_H_

#include "athena/input/public/accelerator_manager.h"

#include "base/macros.h"

namespace athena {

// Handles screen related accelerators.
class ScreenAcceleratorHandler : public AcceleratorHandler {
 public:
  ScreenAcceleratorHandler();

 private:
  ~ScreenAcceleratorHandler() override;

  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const override;
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) override;

  DISALLOW_COPY_AND_ASSIGN(ScreenAcceleratorHandler);
};

}  // namespace athena

#endif  // ATHENA_SCREEN_SCREEN_ACCELERATOR_HANDLER_H_
