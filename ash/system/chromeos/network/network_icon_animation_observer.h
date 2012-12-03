// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_OBSERVER_H_
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_OBSERVER_H_

namespace ash {
namespace network_icon {

// Observer interface class for animating network icons.
class AnimationObserver {
 public:
  // Called when the image has changed due to animation. The callback should
  // trigger a call to GetImageForNetwork() to retrieve the image.
  virtual void NetworkIconChanged() = 0;

 protected:
  virtual ~AnimationObserver() {}
};

}  // namespace network_icon
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_OBSERVER_H_
