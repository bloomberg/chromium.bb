// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_AURA_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_AURA_H_

#include <memory>

#include "ash/common/wm/overview/overview_animation_type.h"
#include "ash/common/wm/overview/scoped_overview_animation_settings.h"
#include "base/macros.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class ScopedLayerAnimationSettings;
}  // namespace ui

namespace ash {

// ScopedOverviewAnimationSettingsfor aura.
class ScopedOverviewAnimationSettingsAura
    : public ScopedOverviewAnimationSettings {
 public:
  ScopedOverviewAnimationSettingsAura(OverviewAnimationType animation_type,
                                      aura::Window* window);
  ~ScopedOverviewAnimationSettingsAura() override;
  void AddObserver(ui::ImplicitAnimationObserver* observer) override;

 private:
  // The managed animation settings.
  std::unique_ptr<ui::ScopedLayerAnimationSettings> animation_settings_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOverviewAnimationSettingsAura);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_AURA_H_
