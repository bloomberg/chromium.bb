// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_ANIMATION_CONTAINER_OBSERVER_H_
#define APP_ANIMATION_CONTAINER_OBSERVER_H_
#pragma once

class AnimationContainer;

// The observer is notified after every update of the animations managed by
// the container.
class AnimationContainerObserver {
 public:
  // Invoked on every tick of the timer managed by the container and after
  // all the animations have updated.
  virtual void AnimationContainerProgressed(
      AnimationContainer* container) = 0;

  // Invoked when no more animations are being managed by this container.
  virtual void AnimationContainerEmpty(AnimationContainer* container) = 0;

 protected:
  virtual ~AnimationContainerObserver() {}
};

#endif  // APP_ANIMATION_CONTAINER_OBSERVER_H_
