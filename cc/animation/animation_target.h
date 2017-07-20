// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_TARGET_H_
#define CC_ANIMATION_ANIMATION_TARGET_H_

#include "cc/animation/animation_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class ScrollOffset;
class SizeF;
}  // namespace gfx

namespace cc {

class Animation;
class FilterOperations;
class TransformOperations;

// An AnimationTarget is an entity that can be affected by a ticking
// cc:Animation. Any object that expects to have an opacity update, for
// example, should derive from this class.
class CC_ANIMATION_EXPORT AnimationTarget {
 public:
  virtual ~AnimationTarget() {}
  virtual void NotifyClientOpacityAnimated(float opacity,
                                           Animation* animation) {}
  virtual void NotifyClientFilterAnimated(const FilterOperations& filter,
                                          Animation* animation) {}
  virtual void NotifyClientBoundsAnimated(const gfx::SizeF& size,
                                          Animation* animation) {}
  virtual void NotifyClientBackgroundColorAnimated(SkColor color,
                                                   Animation* animation) {}
  virtual void NotifyClientVisibilityAnimated(bool visibility,
                                              Animation* animation) {}
  virtual void NotifyClientTransformOperationsAnimated(
      const TransformOperations& operations,
      Animation* animation) {}
  virtual void NotifyClientScrollOffsetAnimated(
      const gfx::ScrollOffset& scroll_offset,
      Animation* animation) {}
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_TARGET_H_
