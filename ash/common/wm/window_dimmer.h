// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WINDOW_DIMMER_H_
#define ASH_COMMON_WINDOW_DIMMER_H_

#include "ash/ash_export.h"
#include "ash/common/wm_window_observer.h"
#include "base/macros.h"

namespace ash {

// WindowDimmer creates a window whose opacity is animated by way of
// SetDimOpacity() and whose size matches that of its parent. WindowDimmer is
// intended to be used in cases where a certain set of windows need to appear
// partially obscured. This is achieved by creating WindowDimmer, setting the
// opacity, and then stacking window() above the windows that are to appear
// obscured. The window created by WindowDimmer is owned by the parent, but also
// deleted if WindowDimmer is deleted. It is expected that WindowDimmer is
// deleted when the parent window is deleted (such as happens with
// WmWindowUserData).
class ASH_EXPORT WindowDimmer : public WmWindowObserver {
 public:
  // Creates a new WindowDimmer. The window() created by WindowDimmer is added
  // to |parent| and stacked above all other child windows.
  explicit WindowDimmer(WmWindow* parent);
  ~WindowDimmer() override;

  void SetDimOpacity(float target_opacity);

  WmWindow* parent() { return parent_; }
  WmWindow* window() { return window_; }

  // NOTE: WindowDimmer is an observer for both |parent_| and |window_|.
  // WmWindowObserver:
  void OnWindowBoundsChanged(WmWindow* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(WmWindow* window) override;
  void OnWindowTreeChanging(WmWindow* window,
                            const TreeChangeParams& params) override;

 private:
  WmWindow* parent_;
  WmWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(WindowDimmer);
};

}  // namespace ash

#endif  // ASH_COMMON_WINDOW_DIMMER_H_
