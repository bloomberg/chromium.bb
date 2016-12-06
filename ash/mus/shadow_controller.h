// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SHADOW_CONTROLLER_H_
#define ASH_MUS_SHADOW_CONTROLLER_H_

#include "base/macros.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace aura {
namespace client {
class FocusClient;
}
}

namespace ash {
namespace mus {

// TODO: use the wm::ShadowController. http://crbug.com/670840.
class ShadowController : public aura::EnvObserver,
                         public aura::WindowObserver,
                         public aura::client::FocusChangeObserver {
 public:
  ShadowController();
  ~ShadowController() override;

 private:
  void SetActiveWindow(aura::Window* window);
  void SetFocusClient(aura::client::FocusClient* focus_client);

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;
  void OnActiveFocusClientChanged(aura::client::FocusClient* focus_client,
                                  aura::Window* window) override;

  // aura::client::FocusChangeObserver:
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  // ui::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  aura::Window* active_window_ = nullptr;
  aura::client::FocusClient* active_focus_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShadowController);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_SHADOW_CONTROLLER_H_
