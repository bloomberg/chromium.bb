// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROOT_WINDOW_CONTROLLER_COMMON_H_
#define ASH_ROOT_WINDOW_CONTROLLER_COMMON_H_

#include "base/macros.h"

namespace ash {
namespace wm {
class RootWindowLayoutManager;
class WmWindow;
}

// This will eventually become what is RootWindowController. During the
// transition it contains code used by both the aura and mus implementations.
// It should *not* contain any aura specific code.
class RootWindowControllerCommon {
 public:
  explicit RootWindowControllerCommon(wm::WmWindow* root);
  ~RootWindowControllerCommon();

  // Creates the containers (WmWindows) used by the shell.
  void CreateContainers();

  // Creates the LayoutManagers for the windows created by CreateContainers().
  void CreateLayoutManagers();

  wm::RootWindowLayoutManager* root_window_layout() {
    return root_window_layout_;
  }

 private:
  wm::WmWindow* root_;

  wm::RootWindowLayoutManager* root_window_layout_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowControllerCommon);
};

}  // namespace ash

#endif  // ASH_ROOT_WINDOW_CONTROLLER_COMMON_H_
