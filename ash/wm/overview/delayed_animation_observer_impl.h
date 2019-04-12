// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_DELAYED_ANIMATION_OBSERVER_IMPL_H_
#define ASH_WM_OVERVIEW_DELAYED_ANIMATION_OBSERVER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/delayed_animation_observer.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ash {
class OverviewDelegate;

class ASH_EXPORT ForceDelayObserver : public DelayedAnimationObserver {
 public:
  explicit ForceDelayObserver(base::TimeDelta delay);
  ~ForceDelayObserver() override;

  // DelayedAnimationObserver:
  void SetOwner(OverviewDelegate* owner) override;
  void Shutdown() override;

 private:
  void Finish();

  OverviewDelegate* owner_ = nullptr;
  base::WeakPtrFactory<ForceDelayObserver> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ForceDelayObserver);
};

// An observer which watches a overview enter animation and signals its owner
// when the animation it is watching finishes.
class ASH_EXPORT StartAnimationObserver : public ui::ImplicitAnimationObserver,
                                          public DelayedAnimationObserver {
 public:
  StartAnimationObserver();
  ~StartAnimationObserver() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // DelayedAnimationObserver:
  void SetOwner(OverviewDelegate* owner) override;
  void Shutdown() override;

 private:
  OverviewDelegate* owner_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(StartAnimationObserver);
};

class ASH_EXPORT ExitAnimationObserver : public ui::ImplicitAnimationObserver,
                                         public DelayedAnimationObserver {
 public:
  ExitAnimationObserver();
  ~ExitAnimationObserver() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // DelayedAnimationObserver:
  void SetOwner(OverviewDelegate* owner) override;
  void Shutdown() override;

 private:
  OverviewDelegate* owner_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ExitAnimationObserver);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_DELAYED_ANIMATION_OBSERVER_IMPL_H_
