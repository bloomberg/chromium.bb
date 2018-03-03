// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_CHROMEVOX_LAYOUT_MANAGER_H_
#define ASH_ACCESSIBILITY_CHROMEVOX_LAYOUT_MANAGER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/layout_manager.h"

namespace ash {

// ChromeVoxLayoutManager manages the container window used for the ChromeVox
// spoken feedback panel, which sits at the top of the display. It insets the
// display work area bounds when ChromeVox is visible. The ChromeVox panel is
// created by Chrome because spoken feedback is implemented by an extension.
// Exported for test.
class ASH_EXPORT ChromeVoxLayoutManager : public aura::LayoutManager {
 public:
  ChromeVoxLayoutManager();
  ~ChromeVoxLayoutManager() override;

  // aura::LayoutManager:
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override;
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  aura::Window* chromevox_window_for_test() { return chromevox_window_; }

 private:
  // Updates the ChromeVox window bounds and the display work area.
  void UpdateLayout();

  aura::Window* chromevox_window_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ChromeVoxLayoutManager);
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_CHROMEVOX_LAYOUT_MANAGER_H_
