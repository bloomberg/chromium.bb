// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WM_SCOPED_OVERVIEW_ANIMATION_SETTINGS_AURA_H_
#define ASH_WM_OVERVIEW_WM_SCOPED_OVERVIEW_ANIMATION_SETTINGS_AURA_H_

#include <memory>

#include "ash/common/wm/overview/scoped_overview_animation_settings_factory.h"
#include "base/macros.h"

namespace ash {

// Aura implementation of ScopedOverviewAnimationSettingsFactory.
class ScopedOverviewAnimationSettingsFactoryAura
    : public ScopedOverviewAnimationSettingsFactory {
 public:
  ScopedOverviewAnimationSettingsFactoryAura();
  ~ScopedOverviewAnimationSettingsFactoryAura() override;

  // ScopedOverviewAnimationSettingsFactoryAura:
  std::unique_ptr<ScopedOverviewAnimationSettings>
  CreateOverviewAnimationSettings(OverviewAnimationType animation_type,
                                  WmWindow* window) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedOverviewAnimationSettingsFactoryAura);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WM_SCOPED_OVERVIEW_ANIMATION_SETTINGS_AURA_H_
