// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_
#define CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_

#include "ui/gfx/geometry/point3_f.h"

namespace cc {

struct LayerImplTestProperties {
  LayerImplTestProperties()
      : transform_origin(gfx::Point3F()),
        double_sided(true),
        force_render_surface(false) {}
  gfx::Point3F transform_origin;
  bool double_sided;
  bool force_render_surface;
};

}  // namespace cc
#endif  // CC_LAYERS_LAYER_IMPL_TEST_PROPERTIES_H_
