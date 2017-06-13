// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/quads/largest_draw_quad.h"

#include <stddef.h>

#include <algorithm>

#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/picture_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"

namespace {

template <typename...>
struct MaxSize {};
template <class T, class... Args>
struct MaxSize<T, Args...> {
  static constexpr size_t value = sizeof(T) > MaxSize<Args...>::value
                                      ? sizeof(T)
                                      : MaxSize<Args...>::value;
};
template <>
struct MaxSize<> {
  static constexpr size_t value = 0;
};

constexpr size_t kLargestDrawQuadSize = MaxSize<cc::DebugBorderDrawQuad,
                                                cc::PictureDrawQuad,
                                                cc::RenderPassDrawQuad,
                                                cc::SolidColorDrawQuad,
                                                cc::StreamVideoDrawQuad,
                                                cc::SurfaceDrawQuad,
                                                cc::TextureDrawQuad,
                                                cc::TileDrawQuad,
                                                cc::YUVVideoDrawQuad>::value;

template <typename...>
struct MaxAlign {};
template <class T, class... Args>
struct MaxAlign<T, Args...> {
  static constexpr size_t value = alignof(T) > MaxAlign<Args...>::value
                                      ? alignof(T)
                                      : MaxAlign<Args...>::value;
};
template <>
struct MaxAlign<> {
  static constexpr size_t value = 0;
};

constexpr size_t kLargestDrawQuadAlignment =
    MaxAlign<cc::DebugBorderDrawQuad,
             cc::PictureDrawQuad,
             cc::RenderPassDrawQuad,
             cc::SolidColorDrawQuad,
             cc::StreamVideoDrawQuad,
             cc::SurfaceDrawQuad,
             cc::TextureDrawQuad,
             cc::TileDrawQuad,
             cc::YUVVideoDrawQuad>::value;

}  // namespace

namespace cc {

size_t LargestDrawQuadSize() {
  return kLargestDrawQuadSize;
}

size_t LargestDrawQuadAlignment() {
  return kLargestDrawQuadAlignment;
}

}  // namespace cc
