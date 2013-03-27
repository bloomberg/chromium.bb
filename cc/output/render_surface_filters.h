// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_OUTPUT_RENDER_SURFACE_FILTERS_H_
#define CC_OUTPUT_RENDER_SURFACE_FILTERS_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"

class GrContext;
class SkBitmap;

namespace gfx {
class SizeF;
}

namespace WebKit {
class WebFilterOperations;
}

namespace cc {

class CC_EXPORT RenderSurfaceFilters {
 public:
  static SkBitmap Apply(const WebKit::WebFilterOperations& filters,
                        unsigned texture_id,
                        gfx::SizeF size,
                        GrContext* gr_context);
  static WebKit::WebFilterOperations Optimize(
      const WebKit::WebFilterOperations& filters);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(RenderSurfaceFilters);
};

}
#endif  // CC_OUTPUT_RENDER_SURFACE_FILTERS_H_
