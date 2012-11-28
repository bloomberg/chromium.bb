// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_CONTENTS_SCALING_LAYER_H_
#define CC_CONTENTS_SCALING_LAYER_H_

#include "cc/cc_export.h"
#include "cc/layer.h"

namespace cc {

// Base class for layers that need contents scale.
// The content bounds are determined by bounds and scale of the contents.
class CC_EXPORT ContentsScalingLayer : public Layer {
 public:
  virtual gfx::Size contentBounds() const OVERRIDE;
  virtual float contentsScaleX() const OVERRIDE;
  virtual float contentsScaleY() const OVERRIDE;
  virtual void setContentsScale(float contentsScale) OVERRIDE;

 protected:
  ContentsScalingLayer();
  virtual ~ContentsScalingLayer();

  gfx::Size computeContentBoundsForScale(float scaleX, float scaleY) const;

 private:
  float m_contentsScale;
};

}  // namespace cc

#endif  // CC_CONTENTS_SCALING_LAYER_H__
