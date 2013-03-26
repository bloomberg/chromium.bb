// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_APPEND_QUADS_DATA_H_
#define CC_LAYERS_APPEND_QUADS_DATA_H_

#include "base/basictypes.h"
#include "cc/quads/render_pass.h"

namespace cc {

struct AppendQuadsData {
  AppendQuadsData()
      : had_occlusion_from_outside_target_surface(false),
        had_incomplete_tile(false),
        num_missing_tiles(0),
        render_pass_id(0, 0) {}

  explicit AppendQuadsData(RenderPass::Id render_pass_id)
      : had_occlusion_from_outside_target_surface(false),
        had_incomplete_tile(false),
        num_missing_tiles(0),
        render_pass_id(render_pass_id) {}

  // Set by the QuadCuller.
  bool had_occlusion_from_outside_target_surface;
  // Set by the layer appending quads.
  bool had_incomplete_tile;
  // Set by the layer appending quads.
  int64 num_missing_tiles;
  // Given to the layer appending quads.
  const RenderPass::Id render_pass_id;
};

}
#endif  // CC_LAYERS_APPEND_QUADS_DATA_H_
