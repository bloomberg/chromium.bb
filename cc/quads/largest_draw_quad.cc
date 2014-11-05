// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/largest_draw_quad.h"

#include <algorithm>

#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"

namespace cc {

size_t LargestDrawQuadSize() {
  // The largest quad is either a RenderPassDrawQuad or a StreamVideoDrawQuad
  // depends on hardware structure.
  return std::max(sizeof(RenderPassDrawQuad), sizeof(StreamVideoDrawQuad));
}

}  // namespace cc
