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
  class AcceleratorWrapper;

  // Creates an AcceleratorManager for global accelerators.
  // This is the one returned by AcceleratorManager::Get()
  static AcceleratorManagerImpl* CreateGlobalAcceleratorManager();

  // Creates an AcceleratorManager for focus manager.
  static scoped_ptr<AcceleratorManager> CreateForFocusManager(
      views::FocusManager* focus_manager);

  ~AcceleratorManagerImpl() override;

  void Init();

  void OnRootWindowCreated(aura::Window* root_window);

  bool Process(const ui::Accelerator& accelerator);

  // AcceleratorManager:
  // This is made public so that implementation classes can use this.
  virtual bool IsRegistered(const ui::Accelerator& accelerator,
                            int flags) const override;

 private:
  class InternalData;

  AcceleratorManagerImpl(AcceleratorWrapper* wrapper, bool global);

  // AcceleratorManager:
  virtual void RegisterAccelerators(const AcceleratorData accelerators[],
                                    size_t num_accelerators,
                                    AcceleratorHandler* handler) override;
  virtual void RegisterAccelerator(const AcceleratorData& accelerator_data,
                                   AcceleratorHandler* handler) override;
  virtual void UnregisterAccelerator(const AcceleratorData& accelerator_data,
                                     AcceleratorHandler* handler) override;
  virtual void SetDebugAcceleratorsEnabled(bool enabled) override;

  // ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  virtual bool CanHandleAccelerators() const override;

  std::map<ui::Accelerator, InternalData> accelerators_;
  scoped_ptr<AcceleratorWrapper> accelerator_wrapper_;
  scoped_ptr<wm::AcceleratorFilter> accelerator_filter_;
  scoped_ptr<wm::NestedAcceleratorController> nested_accelerator_controller_;
  bool debug_accelerators_enabled_;
  bool global_;

  DISALLOW_COPY_AND_ASSIGN(AcceleratorManagerImpl);
};

}  // namespace athena

#endif  // ATHENA_INPUT_ACCELERATOR_MANAGER_H_
