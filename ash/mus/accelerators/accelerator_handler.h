// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_ACCELERATORS_ACCELERATOR_HANDLER_H_
#define ASH_MUS_ACCELERATORS_ACCELERATOR_HANDLER_H_

#include "services/ui/public/interfaces/window_manager.mojom.h"

namespace ash {
namespace mus {

// Used by WindowManager for handling accelerators.
class AcceleratorHandler {
 public:
  virtual ui::mojom::EventResult OnAccelerator(uint32_t id,
                                               const ui::Event& event) = 0;

 protected:
  virtual ~AcceleratorHandler() {}
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_ACCELERATORS_ACCELERATOR_HANDLER_H_
