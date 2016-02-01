// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_WINDOW_MANAGER_H_
#define MASH_WM_WINDOW_MANAGER_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/mus/common/types.h"
#include "components/mus/public/cpp/window_manager_delegate.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"

namespace mash {
namespace wm {

class RootWindowController;

class WindowManager : public mus::WindowObserver,
                      public mus::WindowManagerDelegate {
 public:
  WindowManager();
  ~WindowManager() override;

  void Initialize(RootWindowController* root_controller);

 private:
  gfx::Rect CalculateDefaultBounds(mus::Window* window) const;
  gfx::Rect GetMaximizedWindowBounds() const;

  mus::Window* NewTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties);

  // mus::WindowObserver:
  void OnTreeChanging(const TreeChangeParams& params) override;
  void OnWindowEmbeddedAppDisconnected(mus::Window* window) override;

  // WindowManagerDelegate:
  void SetWindowManagerClient(mus::WindowManagerClient* client) override;
  bool OnWmSetBounds(mus::Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(mus::Window* window,
                       const std::string& name,
                       scoped_ptr<std::vector<uint8_t>>* new_data) override;
  mus::Window* OnWmCreateTopLevelWindow(
      std::map<std::string, std::vector<uint8_t>>* properties) override;

  RootWindowController* root_controller_;
  mus::WindowManagerClient* window_manager_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_WINDOW_MANAGER_H_
