// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROOT_WINDOW_CONTROLLER_H_
#define ASH_ROOT_WINDOW_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class SkBitmap;

namespace aura {
class EventFilter;
class RootWindow;
class Window;
namespace shared {
class InputMethodEventFilter;
class RootWindowEventFilter;
}  // namespace shared
}  // namespace aura

namespace ash {
namespace internal {

class EventClientImpl;
class RootWindowLayoutManager;
class ScreenDimmer;
class WorkspaceController;

// This class maintains the per root window state for ash. This class
// owns the root window and other dependent objects that should be
// deleted upon the deletion of the root window.  The RootWindowController
// for particular root window is stored as a property and can be obtained
// using |GetRootWindowController(aura::RootWindow*)| function.
class RootWindowController {
 public:
  explicit RootWindowController(aura::RootWindow* root_window);
  ~RootWindowController();

  aura::RootWindow* root_window() {
    return root_window_.get();
  }

  internal::RootWindowLayoutManager* root_window_layout() {
    return root_window_layout_;
  }

  internal::WorkspaceController* workspace_controller() {
    return workspace_controller_.get();
  }

  internal::ScreenDimmer* screen_dimmer() {
    return screen_dimmer_.get();
  }

  aura::Window* GetContainer(int container_id);

  void CreateContainers();
  void InitLayoutManagers();

  // Deletes all child windows and performs necessary cleanup.
  void CloseChildWindows();

  // Returns true if the workspace has a maximized or fullscreen window.
  bool IsInMaximizedMode() const;

 private:
  scoped_ptr<aura::RootWindow> root_window_;
  internal::RootWindowLayoutManager* root_window_layout_;

  // An event filter that pre-handles all key events to send them to an IME.
  scoped_ptr<internal::EventClientImpl> event_client_;
  scoped_ptr<internal::ScreenDimmer> screen_dimmer_;
  scoped_ptr<internal::WorkspaceController> workspace_controller_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowController);
};

}  // namespace internal
}  // ash

#endif  //  ASH_ROOT_WINDOW_CONTROLLER_H_
