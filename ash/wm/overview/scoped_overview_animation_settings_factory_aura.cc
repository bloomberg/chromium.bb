// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_animation_settings_factory_aura.h"

#include "ash/common/wm_window.h"
#include "ash/wm/overview/scoped_overview_animation_settings_aura.h"
#include "base/memory/ptr_util.h"

namespace ash {

ScopedOverviewAnimationSettingsFactoryAura::
    ScopedOverviewAnimationSettingsFactoryAura() {}

ScopedOverviewAnimationSettingsFactoryAura::
    ~ScopedOverviewAnimationSettingsFactoryAura() {}

std::unique_ptr<ScopedOverviewAnimationSettings>
ScopedOverviewAnimationSettingsFactoryAura::CreateOverviewAnimationSettings(
    OverviewAnimationType animation_type,
    WmWindow* window) {
  return base::MakeUnique<ScopedOverviewAnimationSettingsAura>(
      animation_type, WmWindow::GetAuraWindow(window));
}

}  // namespace ash
