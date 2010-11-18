// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_ANIMATION_CONTAINER_ELEMENT_H_
#define APP_ANIMATION_CONTAINER_ELEMENT_H_
#pragma once

#include "base/time.h"

// Interface for the elements the AnimationContainer contains. This is
// implemented by Animation.
class AnimationContainerElement {
 public:
  // Sets the start of the animation. This is invoked from
  // AnimationContainer::Start.
  virtual void SetStartTime(base::TimeTicks start_time) = 0;

  // Invoked when the animation is to progress.
  virtual void Step(base::TimeTicks time_now) = 0;

  // Returns the time interval of the animation. If an Element needs to change
  // this it should first invoke Stop, then Start.
  virtual base::TimeDelta GetTimerInterval() const = 0;

 protected:
  virtual ~AnimationContainerElement() {}
};

#endif  // APP_ANIMATION_CONTAINER_ELEMENT_H_
