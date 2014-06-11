// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CONFIGURATOR_ANIMATION_H_
#define ASH_DISPLAY_DISPLAY_CONFIGURATOR_ANIMATION_H_

#include <map>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/display/chromeos/display_configurator.h"

namespace aura {
class RootWindow;
class Window;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

// DisplayConfiguratorAnimation provides the visual effects for
// ui::DisplayConfigurator, such like fade-out/in during changing
// the display mode.
class ASH_EXPORT DisplayConfiguratorAnimation
    : public ui::DisplayConfigurator::Observer {
 public:
  DisplayConfiguratorAnimation();
  virtual ~DisplayConfiguratorAnimation();

  // Starts the fade-out animation for the all root windows.  It will
  // call |callback| once all of the animations have finished.
  void StartFadeOutAnimation(base::Closure callback);

  // Starts the animation to clear the fade-out animation effect
  // for the all root windows.
  void StartFadeInAnimation();

 protected:
  // ui::DisplayConfigurator::Observer overrides:
  virtual void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& outputs) OVERRIDE;
  virtual void OnDisplayModeChangeFailed(
      ui::MultipleDisplayState failed_new_state) OVERRIDE;

 private:
  // Clears all hiding layers.  Note that in case that this method is called
  // during an animation, the method call will cancel all of the animations
  // and *not* call the registered callback.
  void ClearHidingLayers();

  std::map<aura::Window*, ui::Layer*> hiding_layers_;
  scoped_ptr<base::OneShotTimer<DisplayConfiguratorAnimation> > timer_;
  base::WeakPtrFactory<DisplayConfiguratorAnimation> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DisplayConfiguratorAnimation);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CONFIGURATION_CONTROLLER_H_
