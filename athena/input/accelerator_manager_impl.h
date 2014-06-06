// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_INPUT_ACCELERATOR_MANAGER_H_
#define ATHENA_INPUT_ACCELERATOR_MANAGER_H_

#include "athena/input/public/accelerator_manager.h"

#include <map>

#include "base/macros.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_target_iterator.h"

namespace aura {
class Window;
}

namespace ui {
class AcceleratorManager;
}

namespace wm {
class AcceleratorFilter;
class NestedAcceleratorController;
}

namespace athena {

// AcceleratorManagerImpl provides a API to register accelerators
// for athena modules. It hides various details on accelerator handling
// such as how to handle accelerator in a nested loop, reserved accelerators
// and repeated accelerators.
class AcceleratorManagerImpl : public AcceleratorManager,
                               public ui::AcceleratorTarget {
 public:
  AcceleratorManagerImpl();
  virtual ~AcceleratorManagerImpl();

  void Init();

  void OnRootWindowCreated(aura::Window* root_window);

  bool IsReservedAccelerator(const ui::Accelerator& accelerator) const;
  bool ProcessAccelerator(const ui::Accelerator& accelerator);

 private:
  // AcceleratorManager:
  virtual void RegisterAccelerators(const AcceleratorData accelerators[],
                                    size_t num_accelerators,
                                    AcceleratorHandler* handler) OVERRIDE;
  virtual void EnableDebugAccelerators() OVERRIDE;

  // ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

  class InternalData;

  void RegisterAccelerator(const AcceleratorData& accelerator,
                           AcceleratorHandler* handler);

  bool debug_accelerators_enabled_;
  std::map<ui::Accelerator, InternalData> accelerators_;
  scoped_ptr<ui::AcceleratorManager> accelerator_manager_;

  scoped_ptr<wm::AcceleratorFilter> accelerator_filter_;
  scoped_ptr<wm::NestedAcceleratorController> nested_accelerator_controller_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorManagerImpl);
};

}  // namespace athena

#endif  // ATHENA_INPUT_ACCELERATOR_MANAGER_H_
