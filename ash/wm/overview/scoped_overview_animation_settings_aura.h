// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_AURA_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_AURA_H_

#include "ash/common/wm/overview/overview_animation_type.h"
#include "ash/common/wm/overview/scoped_overview_animation_settings.h"
#include "base/macros.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// ScopedOverviewAnimationSettingsfor aura.
class ScopedOverviewAnimationSettingsAura
    : public ScopedOverviewAnimationSettings {
 public:
  ScopedOverviewAnimationSettingsAura(OverviewAnimationType animation_type,
                                      aura::Window* window);
  ~ScopedOverviewAnimationSettingsAura() override;

 private:
  // The managed animation settings.
  ui::ScopedLayerAnimationSettings animation_settings_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOverviewAnimationSettingsAura);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_AURA_H_
