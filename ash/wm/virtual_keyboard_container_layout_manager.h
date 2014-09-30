// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_
#define ASH_WM_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_

#include "ash/snap_to_pixel_layout_manager.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace ash {

class VirtualKeyboardContainerLayoutManager : public SnapToPixelLayoutManager {
 public:
  explicit VirtualKeyboardContainerLayoutManager(aura::Window* container);
  virtual ~VirtualKeyboardContainerLayoutManager();

  // Overridden from SnapToPixelLayoutManager:
  virtual void OnWindowResized() OVERRIDE;

 private:
  aura::Window* parent_container_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardContainerLayoutManager);
};

}  // namespace ash

#endif  // ASH_WM_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_