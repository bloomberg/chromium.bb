// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_INPUT_INPUT_MANAGER_IMPL_H_
#define ATHENA_INPUT_INPUT_MANAGER_IMPL_H_

#include "athena/input/public/input_manager.h"

#include "athena/athena_export.h"
#include "athena/input/accelerator_manager_impl.h"
#include "ui/aura/client/event_client.h"
#include "ui/events/event_target.h"

namespace athena {
class PowerButtonController;

namespace test {
class ScopedPowerButtonTimeoutShortener;
}

class ATHENA_EXPORT InputManagerImpl : public InputManager,
                                       public ui::EventTarget,
                                       public aura::client::EventClient {
 public:
  InputManagerImpl();
  ~InputManagerImpl() override;

  void Init();
  void Shutdown();

 private:
  friend class test::ScopedPowerButtonTimeoutShortener;

  // InputManager:
  virtual void OnRootWindowCreated(aura::Window* root_window) override;
  virtual ui::EventTarget* GetTopmostEventTarget() override;
  virtual AcceleratorManager* GetAcceleratorManager() override;
  virtual void AddPowerButtonObserver(PowerButtonObserver* observer) override;
  virtual void RemovePowerButtonObserver(
      PowerButtonObserver* observer) override;

  // Overridden from aura::client::EventClient:
  virtual bool CanProcessEventsWithinSubtree(
      const aura::Window* window) const override;
  virtual ui::EventTarget* GetToplevelEventTarget() override;

  // ui::EventTarget:
  virtual bool CanAcceptEvent(const ui::Event& event) override;
  virtual ui::EventTarget* GetParentTarget() override;
  virtual scoped_ptr<ui::EventTargetIterator> GetChildIterator() const override;
  virtual ui::EventTargeter* GetEventTargeter() override;
  virtual void OnEvent(ui::Event* event) override;

  int SetPowerButtonTimeoutMsForTest(int timeout);

  scoped_ptr<AcceleratorManagerImpl> accelerator_manager_;
  scoped_ptr<PowerButtonController> power_button_controller_;

  DISALLOW_COPY_AND_ASSIGN(InputManagerImpl);
};

}  // namespace athena

#endif  // ATHENA_INPUT_INPUT_MANAGER_IMPL_H_
