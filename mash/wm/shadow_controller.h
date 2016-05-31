// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_SHADOW_CONTROLLER_H_
#define MASH_WM_SHADOW_CONTROLLER_H_

#include "base/macros.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_client_observer.h"

namespace mus {
class WindowTreeClient;
}

namespace mash {
namespace wm {

class ShadowController : public mus::WindowTreeClientObserver,
                         public mus::WindowObserver {
 public:
  explicit ShadowController(mus::WindowTreeClient* window_tree);
  ~ShadowController() override;

 private:
  void SetActiveWindow(mus::Window* window);

  // mus::WindowTreeClientObserver:
  void OnWindowTreeFocusChanged(mus::Window* gained_focus,
                                mus::Window* lost_focus) override;

  // mus::WindowObserver:
  void OnWindowDestroying(mus::Window* window) override;

  mus::WindowTreeClient* window_tree_;
  mus::Window* active_window_;

  DISALLOW_COPY_AND_ASSIGN(ShadowController);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_SHADOW_CONTROLLER_H_
