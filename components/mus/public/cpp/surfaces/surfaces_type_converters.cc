// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/surfaces/surfaces_type_converters.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "components/mus/public/cpp/surfaces/custom_surface_converter.h"

using mus::mojom::Color;
using mus::mojom::ColorPtr;
using mus::mojom::CompositorFrame;
using mus::mojom::CompositorFramePtr;
using mus::mojom::CompositorFrameMetadata;
using mus::mojom::CompositorFrameMetadataPtr;
using mus::mojom::DebugBorderQuadState;
using mus::mojom::DebugBorderQuadStatePtr;
using mus::mojom::Pass;
using mus::mojom::PassPtr;
using mus::mojom::Quad;
using mus::mojom::QuadPtr;
using mus::mojom::RenderPassQuadState;
using mus::mojom::RenderPassQuadStatePtr;
using mus::mojom::SolidColorQuadState;
using mus::mojom::SolidColorQuadStatePtr;
using mus::mojom::SurfaceQuadState;
using mus::mojom::SurfaceQuadStatePtr;
using mus::mojom::TextureQuadState;
using mus::mojom::TextureQuadStatePtr;
using mus::mojom::TileQuadState;
using mus::mojom::TileQuadStatePtr;
using mus::mojom::YUVColorSpace;
using mus::mojom::YUVVideoQuadState;
using mus::mojom::YUVVideoQuadStatePtr;

namespace mojo {

#define ASSERT_ENUM_VALUES_EQUAL(value)                                     \
  static_assert(cc::DrawQuad::value == static_cast<cc::DrawQuad::Material>( \
                                           mus::mojom::Material::value),    \
                #value " enum value must match")

ASSERT_ENUM_VALUES_EQUAL(DEBUG_BORDER);
ASSERT_ENUM_VALUES_EQUAL(PICTURE_CONTENT);
ASSERT_ENUM_VALUES_EQUAL(RENDER_PASS);
ASSERT_ENUM_VALUES_EQUAL(SOLID_COLOR);
ASSERT_ENUM_VALUES_EQUAL(STREAM_VIDEO_CONTENT);
ASSERT_ENUM_VALUES_EQUAL(SURFACE_CONTENT);
ASSERT_ENUM_VALUES_EQUAL(TEXTURE_CONTENT);
ASSERT_ENUM_VALUES_EQUAL(TILED_CONTENT);
ASSERT_ENUM_VALUES_EQUAL(YUV_VIDEO_CONTENT);

static_assert(cc::YUVVideoDrawQuad::REC_601 ==
                  static_cast<cc::YUVVideoDrawQuad::ColorSpace>(
                      mus::mojom::YUVColorSpace::REC_601),
              "REC_601 enum value must match");
// TODO(jamesr): Add REC_709 and JPEG to the YUVColorSpace enum upstream in
// mojo.

namespace {

bool ConvertDrawQuad(const QuadPtr& input,
                     const CompositorFrameMetadataPtr& metadata,
                     cc::SharedQuadState* sqs,
                     cc::RenderPass* render_pass,
                     CustomSurfaceConverter* custom_converter) {
  switch (input->material) {
    case mus::mojom::Material::DEBUG_BORDER: {
      cc::DebugBorderDrawQuad* debug_border_quad =
          render_pass->CreateAndAppendDrawQuad<cc::DebugBorderDrawQuad>();
      debug_border_quad->SetAll(
          sqs, input->rect, input->opaque_rect, input->visible_rect,
          input->needs_blending,
          input->debug_border_quad_state->color.To<SkColor>(),
          input->debug_border_quad_state->width);
      break;
    }
    case mus::mojom::Material::RENDER_PASS: {
      cc::RenderPassDrawQuad* render_pass_quad =
          render_pass->CreateAndAppendDrawQuad<cc::RenderPassDrawQuad>();
      RenderPassQuadState* render_pass_quad_state =
          input->render_pass_quad_state.get();
      render_pass_quad->SetAll(
          sqs, input->rect, input->opaque_rect, input->visible_rect,
          input->needs_blending, render_pass_quad_state->render_pass_id,
          render_pass_quad_state->mask_resource_id,
          render_pass_quad_state->mask_uv_scale.OffsetFromOrigin(),
          render_pass_quad_state->mask_texture_size,
          cc::FilterOperations(),  // TODO(jamesr): filters
          render_pass_quad_state->filters_scale.OffsetFromOrigin(),
          cc::FilterOperations());  // TODO(jamesr): background_filters
      break;
    }
    case mus::mojom::Material::SOLID_COLOR: {
      if (input->solid_color_quad_state.is_null())
        return false;
      cc::SolidColorDrawQuad* color_quad =
          render_pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
      color_quad->SetAll(
          sqs, input->rect, input->opaque_rect, input->visible_rect,
          input->needs_blending,
          input->solid_color_quad_state->color.To<SkColor>(),
          input->solid_color_quad_state->force_anti_aliasing_off);
      break;
    }
    case mus::mojom::Material::SURFACE_CONTENT: {
      if (input->surface_quad_state.is_null())
        return false;

      if (custom_converter) {
        return custom_converter->ConvertSurfaceDrawQuad(input, metadata, sqs,
                                                        render_pass);
      }
      cc::SurfaceDrawQuad* surface_quad =
          render_pass->CreateAndAppendDrawQuad<cc::SurfaceDrawQuad>();
      surface_quad->SetAll(
          sqs, input->rect, input->opaque_rect, input->visible_rect,
          input->needs_blending, input->surface_quad_state->surface);
      break;
    }
    case mus::mojom::Material::TEXTURE_CONTENT: {
      TextureQuadStatePtr& texture_quad_state =
          input->texture_quad_state;
      if (texture_quad_state.is_null() ||
          texture_quad_state->vertex_opacity.is_null() ||
          texture_quad_state->background_color.is_null())
        return false;
      cc::TextureDrawQuad* texture_quad =
          render_pass->CreateAndAppendDrawQuad<cc::TextureDrawQuad>();
      texture_quad->SetAll(
          sqs, input->rect, input->opaque_rect, input->visible_rect,
          input->needs_blending, texture_quad_state->resource_id, gfx::Size(),
          texture_quad_state->premultiplied_alpha,
          texture_quad_state->uv_top_left, texture_quad_state->uv_bottom_right,
          texture_quad_state->background_color.To<SkColor>(),
          &texture_quad_state->vertex_opacity.storage()[0],
          texture_quad_state->y_flipped, texture_quad_state->nearest_neighbor,
          texture_quad_state->secure_output_only);
      break;
    }
    case mus::mojom::Material::TILED_CONTENT: {
      TileQuadStatePtr& tile_state = input->tile_quad_state;
      if (tile_state.is_null())
        return false;
      cc::TileDrawQuad* tile_quad =
          render_pass->CreateAndAppendDrawQuad<cc::TileDrawQuad>();
      tile_quad->SetAll(sqs, input->rect, input->opaque_rect,
                        input->visible_rect, input->needs_blending,
                        tile_state->resource_id, tile_state->tex_coord_rect,
                        tile_state->texture_size, tile_state->swizzle_contents,
                        tile_state->nearest_neighbor);
      break;
    }
    case mus::mojom::Material::YUV_VIDEO_CONTENT: {
      YUVVideoQuadStatePtr& yuv_state = input->yuv_video_quad_state;
      if (yuv_state.is_null())
        return false;
      cc::YUVVideoDrawQuad* yuv_quad =
          render_pass->CreateAndAppendDrawQuad<cc::YUVVideoDrawQuad>();
      yuv_quad->SetAll(
          sqs, input->rect, input->opaque_rect, input->visible_rect,
          input->needs_blending, yuv_state->ya_tex_coord_rect,
          yuv_state->uv_tex_coord_rect, yuv_state->ya_tex_size,
          yuv_state->uv_tex_size, yuv_state->y_plane_resource_id,
          yuv_state->u_plane_resource_id, yuv_state->v_plane_resource_id,
          yuv_state->a_plane_resource_id,
          static_cast<cc::YUVVideoDrawQuad::ColorSpace>(yuv_state->color_space),
          yuv_state->resource_offset, yuv_state->resource_multiplier);
      break;
    }
    default:
      NOTREACHED() << "Unsupported material " << input->material;
      return false;
  }
  return true;
}

}  // namespace

// static
ColorPtr TypeConverter<ColorPtr, SkColor>::Convert(const SkColor& input) {
  ColorPtr color(Color::New());
  color->rgba = input;
  return color;
}

// static
SkColor TypeConverter<SkColor, ColorPtr>::Convert(const ColorPtr& input) {
  return input->rgba;
}

// static
QuadPtr TypeConverter<QuadPtr, cc::DrawQuad>::Convert(
    const cc::DrawQuad& input) {
  QuadPtr quad = Quad::New();
  quad->material = static_cast<mus::mojom::Material>(input.material);
  quad->rect = input.rect;
  quad->opaque_rect = input.opaque_rect;
  quad->visible_rect = input.visible_rect;
  quad->needs_blending = input.needs_blending;
  // This is intentionally left set to an invalid value here. It's set when
  // converting an entire pass since it's an index into the pass' shared quad
  // state list.
  quad->shared_quad_state_index = UINT32_MAX;
  switch (input.material) {
    case cc::DrawQuad::DEBUG_BORDER: {
      const cc::DebugBorderDrawQuad* debug_border_quad =
          cc::DebugBorderDrawQuad::MaterialCast(&input);
      DebugBorderQuadStatePtr debug_border_state =
          DebugBorderQuadState::New();
      debug_border_state->color = Color::From(debug_border_quad->color);
      debug_border_state->width = debug_border_quad->width;
      quad->debug_border_quad_state = std::move(debug_border_state);
      break;
    }
    case cc::DrawQuad::RENDER_PASS: {
      const cc::RenderPassDrawQuad* render_pass_quad =
          cc::RenderPassDrawQuad::MaterialCast(&input);
      RenderPassQuadStatePtr pass_state = RenderPassQuadState::New();
      pass_state->render_pass_id = render_pass_quad->render_pass_id;
      pass_state->mask_resource_id = render_pass_quad->mask_resource_id();
      pass_state->mask_uv_scale =
          gfx::PointAtOffsetFromOrigin(render_pass_quad->mask_uv_scale);
      pass_state->mask_texture_size = render_pass_quad->mask_texture_size;
      // TODO(jamesr): pass_state->filters
      pass_state->filters_scale =
          gfx::PointAtOffsetFromOrigin(render_pass_quad->filters_scale);
      // TODO(jamesr): pass_state->background_filters
      quad->render_pass_quad_state = std::move(pass_state);
      break;
    }
    case cc::DrawQuad::SOLID_COLOR: {
      const cc::SolidColorDrawQuad* color_quad =
          cc::SolidColorDrawQuad::MaterialCast(&input);
      SolidColorQuadStatePtr color_state = SolidColorQuadState::New();
      color_state->color = Color::From(color_quad->color);
      color_state->force_anti_aliasing_off =
          color_quad->force_anti_aliasing_off;
      quad->solid_color_quad_state = std::move(color_state);
      break;
    }
    case cc::DrawQuad::SURFACE_CONTENT: {
      const cc::SurfaceDrawQuad* surface_quad =
          cc::SurfaceDrawQuad::MaterialCast(&input);
      SurfaceQuadStatePtr surface_state =
          SurfaceQuadState::New();
      surface_state->surface = surface_quad->surface_id;
      quad->surface_quad_state = std::move(surface_state);
      break;
    }
    case cc::DrawQuad::TEXTURE_CONTENT: {
      const cc::TextureDrawQuad* texture_quad =
          cc::TextureDrawQuad::MaterialCast(&input);
      TextureQuadStatePtr texture_state = TextureQuadState::New();
      texture_state->resource_id = texture_quad->resource_id();
      texture_state->premultiplied_alpha = texture_quad->premultiplied_alpha;
      texture_state->uv_top_left = texture_quad->uv_top_left;
      texture_state->uv_bottom_right = texture_quad->uv_bottom_right;
      texture_state->background_color =
          Color::From(texture_quad->background_color);
      Array<float> vertex_opacity(4);
      for (size_t i = 0; i < 4; ++i) {
        vertex_opacity[i] = texture_quad->vertex_opacity[i];
      }
      texture_state->vertex_opacity = std::move(vertex_opacity);
      texture_state->y_flipped = texture_quad->y_flipped;
      texture_state->secure_output_only = texture_quad->secure_output_only;
      quad->texture_quad_state = std::move(texture_state);
      break;
    }
    case cc::DrawQuad::TILED_CONTENT: {
      const cc::TileDrawQuad* tile_quad =
          cc::TileDrawQuad::MaterialCast(&input);
      TileQuadStatePtr tile_state = TileQuadState::New();
      tile_state->tex_coord_rect = tile_quad->tex_coord_rect;
      tile_state->texture_size = tile_quad->texture_size;
      tile_state->swizzle_contents = tile_quad->swizzle_contents;
      tile_state->nearest_neighbor = tile_quad->nearest_neighbor;
      tile_state->resource_id = tile_quad->resource_id();
      quad->tile_quad_state = std::move(tile_state);
      break;
    }
    case cc::DrawQuad::YUV_VIDEO_CONTENT: {
      const cc::YUVVideoDrawQuad* yuv_quad =
          cc::YUVVideoDrawQuad::MaterialCast(&input);
      YUVVideoQuadStatePtr yuv_state = YUVVideoQuadState::New();
      yuv_state->ya_tex_coord_rect = yuv_quad->ya_tex_coord_rect;
      yuv_state->uv_tex_coord_rect = yuv_quad->uv_tex_coord_rect;
      yuv_state->ya_tex_size = yuv_quad->ya_tex_size;
      yuv_state->uv_tex_size = yuv_quad->uv_tex_size;
      yuv_state->y_plane_resource_id = yuv_quad->y_plane_resource_id();
      yuv_state->u_plane_resource_id = yuv_quad->u_plane_resource_id();
      yuv_state->v_plane_resource_id = yuv_quad->v_plane_resource_id();
      yuv_state->a_plane_resource_id = yuv_quad->a_plane_resource_id();
      yuv_state->color_space =
          static_cast<YUVColorSpace>(yuv_quad->color_space);
      yuv_state->resource_offset = yuv_quad->resource_offset;
      yuv_state->resource_multiplier = yuv_quad->resource_multiplier;
      quad->yuv_video_quad_state = std::move(yuv_state);
      break;
    }

    default:
      NOTREACHED() << "Unsupported material " << input.material;
  }
  return quad;
}

// static
PassPtr TypeConverter<PassPtr, cc::RenderPass>::Convert(
    const cc::RenderPass& input) {
  PassPtr pass = Pass::New();
  pass->id = input.id;
  pass->output_rect = input.output_rect;
  pass->damage_rect = input.damage_rect;
  pass->transform_to_root_target = input.transform_to_root_target;
  pass->has_transparent_background = input.has_transparent_background;
  Array<QuadPtr> quads(input.quad_list.size());
  Array<cc::SharedQuadState> shared_quad_state(
      input.shared_quad_state_list.size());
  const cc::SharedQuadState* last_sqs = nullptr;
  cc::SharedQuadStateList::ConstIterator next_sqs_iter =
      input.shared_quad_state_list.begin();
  for (auto iter = input.quad_list.cbegin(); iter != input.quad_list.cend();
       ++iter) {
    const cc::DrawQuad& quad = **iter;
    quads[iter.index()] = Quad::From(quad);
    if (quad.shared_quad_state != last_sqs) {
      shared_quad_state[next_sqs_iter.index()] = **next_sqs_iter;
      last_sqs = *next_sqs_iter;
      ++next_sqs_iter;
    }
    DCHECK_LE(next_sqs_iter.index() - 1, UINT32_MAX);
    quads[iter.index()]->shared_quad_state_index =
        static_cast<uint32_t>(next_sqs_iter.index() - 1);
  }
  // We should copy all shared quad states.
  DCHECK_EQ(next_sqs_iter.index(), shared_quad_state.size());
  pass->quads = std::move(quads);
  pass->shared_quad_states = std::move(shared_quad_state);
  return pass;
}

// static
std::unique_ptr<cc::RenderPass> ConvertToRenderPass(
    const PassPtr& input,
    const CompositorFrameMetadataPtr& metadata,
    CustomSurfaceConverter* custom_converter) {
  std::unique_ptr<cc::RenderPass> pass = cc::RenderPass::Create(
      input->shared_quad_states.size(), input->quads.size());
  pass->SetAll(input->id, input->output_rect, input->damage_rect,
               input->transform_to_root_target,
               input->has_transparent_background);
  for (size_t i = 0; i < input->shared_quad_states.size(); ++i) {
    cc::SharedQuadState* state = pass->CreateAndAppendSharedQuadState();
    *state = input->shared_quad_states[i];
  }
  cc::SharedQuadStateList::Iterator sqs_iter =
      pass->shared_quad_state_list.begin();
  for (size_t i = 0; i < input->quads.size(); ++i) {
    QuadPtr quad = std::move(input->quads[i]);
    while (quad->shared_quad_state_index > sqs_iter.index()) {
      ++sqs_iter;
    }
    if (!ConvertDrawQuad(quad, metadata, *sqs_iter, pass.get(),
                         custom_converter))
      return std::unique_ptr<cc::RenderPass>();
  }
  return pass;
}

// static
std::unique_ptr<cc::RenderPass>
TypeConverter<std::unique_ptr<cc::RenderPass>, PassPtr>::Convert(
    const PassPtr& input) {
  mus::mojom::CompositorFrameMetadataPtr metadata;
  return ConvertToRenderPass(input, metadata,
                             nullptr /* CustomSurfaceConverter */);
}

// static
CompositorFrameMetadataPtr
TypeConverter<CompositorFrameMetadataPtr, cc::CompositorFrameMetadata>::Convert(
    const cc::CompositorFrameMetadata& input) {
  CompositorFrameMetadataPtr metadata = CompositorFrameMetadata::New();
  metadata->device_scale_factor = input.device_scale_factor;
  return metadata;
}

// static
cc::CompositorFrameMetadata
TypeConverter<cc::CompositorFrameMetadata, CompositorFrameMetadataPtr>::Convert(
    const CompositorFrameMetadataPtr& input) {
  cc::CompositorFrameMetadata metadata;
  metadata.device_scale_factor = input->device_scale_factor;
  return metadata;
}

// static
CompositorFramePtr
TypeConverter<CompositorFramePtr, cc::CompositorFrame>::Convert(
    const cc::CompositorFrame& input) {
  CompositorFramePtr frame = CompositorFrame::New();
  DCHECK(input.delegated_frame_data);
  cc::DelegatedFrameData* frame_data = input.delegated_frame_data.get();
  frame->resources =
      mojo::Array<cc::TransferableResource>(frame_data->resource_list);
  frame->metadata = CompositorFrameMetadata::From(input.metadata);
  const cc::RenderPassList& pass_list = frame_data->render_pass_list;
  frame->passes = Array<PassPtr>::New(pass_list.size());
  for (size_t i = 0; i < pass_list.size(); ++i) {
    frame->passes[i] = Pass::From(*pass_list[i]);
  }
  return frame;
}

// static
std::unique_ptr<cc::CompositorFrame> ConvertToCompositorFrame(
    const CompositorFramePtr& input,
    CustomSurfaceConverter* custom_converter) {
  std::unique_ptr<cc::DelegatedFrameData> frame_data(
      new cc::DelegatedFrameData);
  frame_data->device_scale_factor = 1.f;
  frame_data->resource_list = input->resources.PassStorage();
  frame_data->render_pass_list.reserve(input->passes.size());
  for (size_t i = 0; i < input->passes.size(); ++i) {
    std::unique_ptr<cc::RenderPass> pass = ConvertToRenderPass(
        input->passes[i], input->metadata, custom_converter);
    if (!pass)
      return std::unique_ptr<cc::CompositorFrame>();
    frame_data->render_pass_list.push_back(std::move(pass));
  }
  std::unique_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  cc::CompositorFrameMetadata metadata =
      input->metadata.To<cc::CompositorFrameMetadata>();
  frame->delegated_frame_data = std::move(frame_data);
  return frame;
}

// static
std::unique_ptr<cc::CompositorFrame>
TypeConverter<std::unique_ptr<cc::CompositorFrame>,
              CompositorFramePtr>::Convert(const CompositorFramePtr& input) {
  return ConvertToCompositorFrame(input, nullptr);
}

}  // namespace mojo
