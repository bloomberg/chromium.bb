// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SHADOW_CONTROLLER_H_
#define ASH_MUS_SHADOW_CONTROLLER_H_

#include "base/macros.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_client_observer.h"

namespace mus {
class WindowTreeClient;
}

namespace ash {
namespace mus {

class ShadowController : public ::mus::WindowTreeClientObserver,
                         public ::mus::WindowObserver {
 public:
  explicit ShadowController(::mus::WindowTreeClient* window_tree);
  ~ShadowController() override;

 private:
  void SetActiveWindow(::mus::Window* window);

  // mus::WindowTreeClientObserver:
  void OnWindowTreeFocusChanged(::mus::Window* gained_focus,
                                ::mus::Window* lost_focus) override;

  // mus::WindowObserver:
  void OnWindowDestroying(::mus::Window* window) override;

  ::mus::WindowTreeClient* window_tree_;
  ::mus::Window* active_window_;

  DISALLOW_COPY_AND_ASSIGN(ShadowController);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_SHADOW_CONTROLLER_H_
