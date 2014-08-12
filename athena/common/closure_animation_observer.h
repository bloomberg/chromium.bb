// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_COMMON_CLOSURE_ANIMATION_OBSERVER_H_
#define ATHENA_COMMON_CLOSURE_ANIMATION_OBSERVER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"

namespace athena {

// Runs a callback at the end of the animation. This observe also destroys
// itself afterwards.
class ClosureAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit ClosureAnimationObserver(const base::Closure& closure);

 private:
  virtual ~ClosureAnimationObserver();

  // ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  const base::Closure closure_;

  DISALLOW_COPY_AND_ASSIGN(ClosureAnimationObserver);
};
}

#endif  // ATHENA_COMMON_CLOSURE_ANIMATION_OBSERVER_H_
