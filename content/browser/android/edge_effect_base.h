// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_EDGE_EFFECT_BASE_H_
#define CONTENT_BROWSER_ANDROID_EDGE_EFFECT_BASE_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform.h"

namespace cc {
class Layer;
}

namespace content {

// A base class for overscroll-related Android effects.
class EdgeEffectBase {
 public:
  enum State {
    STATE_IDLE = 0,
    STATE_PULL,
    STATE_ABSORB,
    STATE_RECEDE,
    STATE_PULL_DECAY
  };

  virtual ~EdgeEffectBase() {}

  virtual void Pull(base::TimeTicks current_time,
                    float delta_distance,
                    float displacement) = 0;
  virtual void Absorb(base::TimeTicks current_time, float velocity) = 0;
  virtual bool Update(base::TimeTicks current_time) = 0;
  virtual void Release(base::TimeTicks current_time) = 0;

  virtual void Finish() = 0;
  virtual bool IsFinished() const = 0;

  virtual void ApplyToLayers(const gfx::SizeF& size,
                             const gfx::Transform& transform) = 0;
  virtual void SetParent(cc::Layer* parent) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_EDGE_EFFECT_BASE_H_
