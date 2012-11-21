// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SHARED_QUAD_STATE_H_
#define CC_SHARED_QUAD_STATE_H_

#include "base/memory/scoped_ptr.h"
#include "ui/gfx/rect.h"
#include <public/WebTransformationMatrix.h>
#include "cc/cc_export.h"

namespace cc {

class CC_EXPORT SharedQuadState {
 public:
  static scoped_ptr<SharedQuadState> Create();

  scoped_ptr<SharedQuadState> Copy() const;

  void SetAll(const WebKit::WebTransformationMatrix& content_to_target_transform,
              const gfx::Rect& visible_content_rect,
              const gfx::Rect& clipped_rect_in_target,
              float opacity);

  // Transforms from quad's original content space to its target content space.
  WebKit::WebTransformationMatrix content_to_target_transform;
  // This rect lives in the content space for the quad's originating layer.
  gfx::Rect visible_content_rect;
  gfx::Rect clipped_rect_in_target;
  float opacity;

 private:
  SharedQuadState();
};

}

#endif  // CC_SHARED_QUAD_STATE_H_
