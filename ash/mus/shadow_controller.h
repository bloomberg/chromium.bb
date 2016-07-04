// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SHADOW_CONTROLLER_H_
#define ASH_MUS_SHADOW_CONTROLLER_H_

#include "base/macros.h"
#include "services/ui/public/cpp/window_observer.h"
#include "services/ui/public/cpp/window_tree_client_observer.h"

namespace ui {
class WindowTreeClient;
}

namespace ash {
namespace mus {

class ShadowController : public ::ui::WindowTreeClientObserver,
                         public ::ui::WindowObserver {
 public:
  explicit ShadowController(::ui::WindowTreeClient* window_tree);
  ~ShadowController() override;

 private:
  void SetActiveWindow(::ui::Window* window);

  // ui::WindowTreeClientObserver:
  void OnWindowTreeFocusChanged(::ui::Window* gained_focus,
                                ::ui::Window* lost_focus) override;

  // ui::WindowObserver:
  void OnWindowDestroying(::ui::Window* window) override;

  ::ui::WindowTreeClient* window_tree_;
  ::ui::Window* active_window_;

  DISALLOW_COPY_AND_ASSIGN(ShadowController);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_SHADOW_CONTROLLER_H_
