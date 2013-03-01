// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_OUTPUT_CONFIGURATOR_ANIMATION_H_
#define ASH_DISPLAY_OUTPUT_CONFIGURATOR_ANIMATION_H_

#include <map>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/timer.h"
#include "chromeos/display/output_configurator.h"

namespace aura {
class RootWindow;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace ash {
namespace internal {

// OutputConfiguratorAnimation provides the visual effects for
// chromeos::OutputConfigurator, such like fade-out/in during changing
// the display mode.
class ASH_EXPORT OutputConfiguratorAnimation
    : public chromeos::OutputConfigurator::Observer {
 public:
  OutputConfiguratorAnimation();
  virtual ~OutputConfiguratorAnimation();

  // Starts the fade-out animation for the all root windows.  It will
  // call |callback| once all of the animations have finished.
  void StartFadeOutAnimation(base::Closure callback);

  // Starts the animation to clear the fade-out animation effect
  // for the all root windows.
  void StartFadeInAnimation();

 protected:
  // chromeos::OutputConfigurator::Observer overrides:
  virtual void OnDisplayModeChanged() OVERRIDE;
  virtual void OnDisplayModeChangeFailed(
      chromeos::OutputState failed_new_state) OVERRIDE;

 private:
  // Clears all hiding layers.  Note that in case that this method is called
  // during an animation, the method call will cancel all of the animations
  // and *not* call the registered callback.
  void ClearHidingLayers();

  std::map<aura::RootWindow*, ui::Layer*> hiding_layers_;
  scoped_ptr<base::OneShotTimer<OutputConfiguratorAnimation> > timer_;

  DISALLOW_COPY_AND_ASSIGN(OutputConfiguratorAnimation);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_OUTPUT_CONFIGURATION_CONTROLLER_H_
