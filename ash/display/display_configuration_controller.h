// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CONFIGURATION_CONTROLLER_H_
#define ASH_DISPLAY_DISPLAY_CONFIGURATION_CONTROLLER_H_

// This class controls Display related configuration. Specifically it:
// * Handles animated transitions where appropriate.
// * Limits the frequency of certain operations.
// * Provides a single interface for UI and API classes.
// * TODO: Forwards display configuration changed events to UI and API classes.

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/display.h"

namespace ash {

namespace test {
class ShellTestApi;
}  // namespace test

class DisplayAnimator;
class DisplayManager;
struct DisplayLayout;

class ASH_EXPORT DisplayConfigurationController
    : public WindowTreeHostManager::Observer {
 public:
  DisplayConfigurationController(
      DisplayManager* display_manager,
      WindowTreeHostManager* window_tree_host_manager);
  ~DisplayConfigurationController() override;

  // Sets the layout for the current displays with a fade in/out
  // animation. Currently |display_id| is assumed to be the secondary
  // display.  TODO(oshima/stevenjb): Support 3+ displays.
  void SetDisplayLayout(const DisplayLayout& layout, bool user_action);

  // Sets the mirror mode with a fade-in/fade-out animation. Affects all
  // displays.
  void SetMirrorMode(bool mirror, bool user_action);

  // Sets the display's rotation with animation if available.
  void SetDisplayRotation(int64_t display_id,
                          gfx::Display::Rotation rotation,
                          gfx::Display::RotationSource source,
                          bool user_action);

  // Sets the primary display id.
  void SetPrimaryDisplayId(int64_t display_id, bool user_action);

  // WindowTreeHostManager::Observer
  void OnDisplayConfigurationChanged() override;

 protected:
  friend class ash::test::ShellTestApi;

  // Allow tests to skip animations.
  void ResetAnimatorForTest();

 private:
  class DisplayChangeLimiter;

  bool IsLimited();
  void SetDisplayLayoutImpl(const DisplayLayout& layout);
  void SetMirrorModeImpl(bool mirror);
  void SetPrimaryDisplayIdImpl(int64_t display_id);

  DisplayManager* display_manager_;                  // weak ptr
  WindowTreeHostManager* window_tree_host_manager_;  // weak ptr
  scoped_ptr<DisplayAnimator> display_animator_;
  scoped_ptr<DisplayChangeLimiter> limiter_;

  base::WeakPtrFactory<DisplayConfigurationController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplayConfigurationController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CONFIGURATION_CONTROLLER_H_
