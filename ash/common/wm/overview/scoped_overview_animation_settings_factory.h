// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_FACTORY_H_
#define ASH_COMMON_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_FACTORY_H_

#include <memory>

#include "ash/common/wm/overview/overview_animation_type.h"

namespace ash {

class ScopedOverviewAnimationSettings;
class WmWindow;

// Factory for creating ScopedOverviewAnimationSettings.
class ScopedOverviewAnimationSettingsFactory {
 public:
  static ScopedOverviewAnimationSettingsFactory* Get();

  virtual std::unique_ptr<ScopedOverviewAnimationSettings>
  CreateOverviewAnimationSettings(OverviewAnimationType animation_type,
                                  WmWindow* window) = 0;

 protected:
  ScopedOverviewAnimationSettingsFactory();
  virtual ~ScopedOverviewAnimationSettingsFactory();

 private:
  static ScopedOverviewAnimationSettingsFactory* instance_;
};

}  // namespace ash

#endif  // ASH_COMMON_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_FACTORY_H_
