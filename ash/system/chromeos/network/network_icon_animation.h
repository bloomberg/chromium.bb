// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_H_
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_H_

#include <set>
#include <string>

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/throb_animation.h"

namespace ash {
namespace network_icon {

class AnimationObserver;

// Single instance class to handle icon animations and keep them in sync.
class ASH_EXPORT NetworkIconAnimation : public ui::AnimationDelegate {
 public:
  NetworkIconAnimation();
  virtual ~NetworkIconAnimation();

  // Returns the current animation value, [0-1].
  double GetAnimation();

  // The animation stops when all observers have been removed.
  // Be sure to remove observers when no associated icons are animating.
  void AddObserver(AnimationObserver* observer);
  void RemoveObserver(AnimationObserver* observer);

  // ui::AnimationDelegate implementation.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  static NetworkIconAnimation* GetInstance();

 private:
  ui::ThrobAnimation animation_;
  ObserverList<AnimationObserver> observers_;
};

}  // namespace network_icon
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_H_
