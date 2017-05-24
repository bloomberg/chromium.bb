// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_FACTORY_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_FACTORY_H_

#include <memory>

#include "ash/wm/overview/overview_animation_type.h"

namespace aura {
class Window;
}

namespace ash {

class ScopedOverviewAnimationSettings;

// Factory for creating ScopedOverviewAnimationSettings.
class ScopedOverviewAnimationSettingsFactory {
 public:
  static ScopedOverviewAnimationSettingsFactory* Get();

  virtual std::unique_ptr<ScopedOverviewAnimationSettings>
  CreateOverviewAnimationSettings(OverviewAnimationType animation_type,
                                  aura::Window* window) = 0;

 protected:
  ScopedOverviewAnimationSettingsFactory();
  virtual ~ScopedOverviewAnimationSettingsFactory();

 private:
  static ScopedOverviewAnimationSettingsFactory* instance_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_FACTORY_H_
