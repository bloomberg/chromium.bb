// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SCREEN_DIMMER_H_
#define ASH_WM_SCREEN_DIMMER_H_

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"

namespace ui {
class Layer;
}

namespace ash {
class DimWindow;

// ScreenDimmer displays a partially-opaque layer above everything
// else in the given container window to darken the display.  It shouldn't be
// used for long-term brightness adjustments due to performance
// considerations -- it's only intended for cases where we want to
// briefly dim the screen (e.g. to indicate to the user that we're
// about to suspend a machine that lacks an internal backlight that
// can be adjusted).
class ASH_EXPORT ScreenDimmer : ShellObserver {
 public:
  // Creates a screen dimmer for the containers given by |container_id|.
  // It's owned by the container in the primary root window and will be
  // destroyed when the container is destroyed.
  static ScreenDimmer* GetForContainer(int container_id);

  // Creates a dimmer a root window level. This is used for suspend animation.
  static ScreenDimmer* GetForRoot();

  ~ScreenDimmer() override;

  // Dim or undim the layers.
  void SetDimming(bool should_dim);

  void set_at_bottom(bool at_bottom) { at_bottom_ = at_bottom; }

  bool is_dimming() const { return is_dimming_; }

  // Find a ScreenDimmer in the container, or nullptr if it does not exist.
  static ScreenDimmer* FindForTest(int container_id);

 private:
  static aura::Window* FindContainer(int container_id);

  explicit ScreenDimmer(int container_id);

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;

  // Update the dimming state. This will also create a new DimWindow
  // if necessary. (Used when a new display is connected)
  void Update(bool should_dim);

  int container_id_;
  float target_opacity_;

  // Are we currently dimming the screen?
  bool is_dimming_;
  bool at_bottom_;

  DISALLOW_COPY_AND_ASSIGN(ScreenDimmer);
};

}  // namespace ash

#endif  // ASH_WM_SCREEN_DIMMER_H_
