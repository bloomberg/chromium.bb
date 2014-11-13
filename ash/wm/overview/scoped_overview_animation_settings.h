// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_H_

#include "ash/wm/overview/overview_animation_type.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// The ScopedOveriewAnimationSettings class correctly configures the animation
// settings for an aura::Window for a given OverviewAnimationType.
class ScopedOverviewAnimationSettings {
 public:
  ScopedOverviewAnimationSettings(OverviewAnimationType animation_type,
                                  aura::Window* window);
  virtual ~ScopedOverviewAnimationSettings();

 private:
  // The managed animation settings.
  ui::ScopedLayerAnimationSettings animation_settings_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOverviewAnimationSettings);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_H_
