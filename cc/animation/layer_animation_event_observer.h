// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_LAYER_ANIMATION_EVENT_OBSERVER_H_
#define CC_ANIMATION_LAYER_ANIMATION_EVENT_OBSERVER_H_

namespace cc {

class CC_EXPORT LayerAnimationEventObserver {
 public:
  virtual void OnAnimationStarted(const AnimationEvent& event) = 0;
};

} // namespace cc

#endif  // CC_ANIMATION_LAYER_ANIMATION_EVENT_OBSERVER_H_

