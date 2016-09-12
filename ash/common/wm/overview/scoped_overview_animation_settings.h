// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_H_
#define ASH_COMMON_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_H_

namespace ui {
class ImplicitAnimationObserver;
}  // namespace ui

namespace ash {

// ScopedOverviewAnimationSettings correctly configures the animation
// settings for a WmWindow given an OverviewAnimationType.
class ScopedOverviewAnimationSettings {
 public:
  virtual ~ScopedOverviewAnimationSettings() {}
  virtual void AddObserver(ui::ImplicitAnimationObserver* observer) = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_WM_OVERVIEW_SCOPED_OVERVIEW_ANIMATION_SETTINGS_H_
