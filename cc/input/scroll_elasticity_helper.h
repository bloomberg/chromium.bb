// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_ELASTICITY_HELPER_H_
#define CC_INPUT_SCROLL_ELASTICITY_HELPER_H_

#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class LayerTreeHostImpl;

// ScrollElasticityHelper is based on
// WebKit/Source/platform/mac/ScrollElasticityController.h
/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

// Interface between a LayerTreeHostImpl and the ScrollElasticityController. It
// would be possible, in principle, for LayerTreeHostImpl to implement this
// interface itself. This artificial boundary is introduced to reduce the amount
// of logic and state held directly inside LayerTreeHostImpl.
class CC_EXPORT ScrollElasticityHelper {
 public:
  static ScrollElasticityHelper* CreateForLayerTreeHostImpl(
      LayerTreeHostImpl* layer_tree_host_impl);

  virtual ~ScrollElasticityHelper() {}

  // The amount that the view is stretched past the normal allowable bounds.
  virtual gfx::Vector2dF StretchAmount() = 0;
  virtual void SetStretchAmount(const gfx::Vector2dF& stretch_amount) = 0;
  // Returns true if either component of |direction| is pointing in a direction
  // in which it is not possible to scroll any farther. It is only in this
  // circumstance that an overscroll in that direction may begin.
  // TODO(ccameron): Note that it is possible for a scroll to occur against a
  // previous over-scroll. Such a scroll needs to first cancel out the
  // over-scroll, and only then may it start scrolling away from the edge.
  virtual bool PinnedInDirection(const gfx::Vector2dF& direction) = 0;
  // Whether or not the content of the page is scrollable in each direction.
  virtual bool CanScrollHorizontally() = 0;
  virtual bool CanScrollVertically() = 0;
  // Request that the controller have its Animate method called for the next
  // frame.
  virtual void RequestAnimate() = 0;
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_ELASTICITY_HELPER_H_
