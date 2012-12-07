// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_REGISTRAR_H_
#define CC_ANIMATION_REGISTRAR_H_

namespace cc {

class LayerAnimationController;

// TODO(vollick): we need to rename:
//    AnimationRegistrar -> AnimationController
//    LayerAnimationController -> LayerAnimator
class CC_EXPORT AnimationRegistrar {
 public:
  // Registers the given animation controller as active. An active animation
  // controller is one that has a running animation that needs to be ticked.
  virtual void DidActivateAnimationController(LayerAnimationController*) = 0;

  // Unregisters the given animation controller. When this happens, the
  // animation controller will no longer be ticked (since it's not active). It
  // is not an error to call this function with a deactivated controller.
  virtual void DidDeactivateAnimationController(LayerAnimationController*) = 0;

  // Registers the given controller as alive.
  virtual void RegisterAnimationController(LayerAnimationController*) = 0;

  // Unregisters the given controller as alive.
  virtual void UnregisterAnimationController(LayerAnimationController*) = 0;
};

}  // namespace cc

#endif  // CC_ANIMATION_REGISTRAR_H_
