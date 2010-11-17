// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_ANIMATION_DELEGATE_H_
#define APP_ANIMATION_DELEGATE_H_
#pragma once

class Animation;

// AnimationDelegate
//
//  Implement this interface when you want to receive notifications about the
//  state of an animation.
class AnimationDelegate {
 public:
  // Called when an animation has completed.
  virtual void AnimationEnded(const Animation* animation) {}

  // Called when an animation has progressed.
  virtual void AnimationProgressed(const Animation* animation) {}

  // Called when an animation has been canceled.
  virtual void AnimationCanceled(const Animation* animation) {}

 protected:
  virtual ~AnimationDelegate() {}
};

#endif  // APP_ANIMATION_DELEGATE_H_
