// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

// A window used to dim the child windows of the given container.
class ASH_EXPORT DimWindow : public aura::Window, public aura::WindowObserver {
 public:
  // Return a dim window for the container if any, or nullptr.
  static DimWindow* Get(aura::Window* container);

  explicit DimWindow(aura::Window* parent);
  ~DimWindow() override;

  void SetDimOpacity(float target_opacity);

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  aura::Window* parent_;

  DISALLOW_COPY_AND_ASSIGN(DimWindow);
};

}  // namespace ash
