// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/converters/surfaces/surfaces_type_converters.h"

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
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/custom_surface_converter.h"
#include "mojo/converters/transform/transform_type_converters.h"

using mus::mojom::Color;
using mus::mojom::ColorPtr;
using mus::mojom::CommandBufferNamespace;
using mus::mojom::CompositorFrame;
using mus::mojom::CompositorFramePtr;
using mus::mojom::CompositorFrameMetadata;
using mus::mojom::CompositorFrameMetadataPtr;
using mus::mojom::DebugBorderQuadState;
using mus::mojom::DebugBorderQuadStatePtr;
using mus::mojom::Mailbox;
using mus::mojom::MailboxPtr;
using mus::mojom::MailboxHolder;
using mus::mojom::MailboxHolderPtr;
using mus::mojom::Pass;
using mus::mojom::PassPtr;
using mus::mojom::Quad;
using mus::mojom::QuadPtr;
using mus::mojom::RenderPassId;
using mus::mojom::RenderPassIdPtr;
using mus::mojom::RenderPassQuadState;
using mus::mojom::RenderPassQuadStatePtr;
using mus::mojom::ResourceFormat;
using mus::mojom::ReturnedResource;
using mus::mojom::ReturnedResourcePtr;
using mus::mojom::SharedQuadState;
using mus::mojom::SharedQuadStatePtr;
using mus::mojom::SolidColorQuadState;
using mus::mojom::SolidColorQuadStatePtr;
using mus::mojom::SurfaceId;
using mus::mojom::SurfaceIdPtr;
using mus::mojom::SurfaceQuadState;
using mus::mojom::SurfaceQuadStatePtr;
using mus::mojom::SyncToken;
using mus::mojom::SyncTokenPtr;
using mus::mojom::TextureQuadState;
using mus::mojom::TextureQuadStatePtr;
using mus::mojom::TileQuadState;
using mus::mojom::TileQuadStatePtr;
using mus::mojom::TransferableResource;
using mus::mojom::TransferableResourcePtr;
using mus::mojom::YUVColorSpace;
using mus::mojom::YUVVideoQuadState;
using mus::mojom::YUVVideoQuadStatePtr;

namespace mojo {

#define ASSERT_ENUM_VALUES_EQUAL(value)                                     \
  static_assert(cc::DrawQuad::value == static_cast<cc::DrawQuad::Material>( \
                                           mus::mojom::Material::value),    \
                #value " enum value must match")

ASSERT_ENUM_VALUES_EQUAL(DEBUG_BORDER);
ASSERT_ENUM_VALUES_EQUAL(IO_SURFACE_CONTENT);
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

cc::SharedQuadState* ConvertSharedQuadState(
    const mus::mojom::SharedQuadStatePtr& input,
    cc::RenderPass* render_pass) {
  cc::SharedQuadState* state = render_pass->CreateAndAppendSharedQuadState();
  state->SetAll(input->quad_to_target_transform.To<gfx::Transform>(),
                input->quad_layer_bounds.To<gfx::Size>(),
                input->visible_quad_layer_rect.To<gfx::Rect>(),
                input->clip_rect.To<gfx::Rect>(), input->is_clipped,
                input->opacity,
                static_cast<::SkXfermode::Mode>(input->blend_mode),
                input->sorting_context_id);
  return state;
}

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
          sqs,
          input->rect.To<gfx::Rect>(),
          input->opaque_rect.To<gfx::Rect>(),
          input->visible_rect.To<gfx::Rect>(),
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
      gfx::PointF mask_uv_scale_as_point =
          render_pass_quad_state->mask_uv_scale.To<gfx::PointF>();
      gfx::PointF filter_scale_as_point =
          render_pass_quad_state->filters_scale.To<gfx::PointF>();
      render_pass_quad->SetAll(
          sqs,
          input->rect.To<gfx::Rect>(),
          input->opaque_rect.To<gfx::Rect>(),
          input->visible_rect.To<gfx::Rect>(),
          input->needs_blending,
          render_pass_quad_state->render_pass_id.To<cc::RenderPassId>(),
          render_pass_quad_state->mask_resource_id,
          mask_uv_scale_as_point.OffsetFromOrigin(),
          render_pass_quad_state->mask_texture_size.To<gfx::Size>(),
          cc::FilterOperations(),  // TODO(jamesr): filters
          filter_scale_as_point.OffsetFromOrigin(),
          cc::FilterOperations());  // TODO(jamesr): background_filters
      break;
    }
    case mus::mojom::Material::SOLID_COLOR: {
      if (input->solid_color_quad_state.is_null())
        return false;
      cc::SolidColorDrawQuad* color_quad =
          render_pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
      color_quad->SetAll(
          sqs,
          input->rect.To<gfx::Rect>(),
          input->opaque_rect.To<gfx::Rect>(),
          input->visible_rect.To<gfx::Rect>(),
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
          sqs,
          input->rect.To<gfx::Rect>(),
          input->opaque_rect.To<gfx::Rect>(),
          input->visible_rect.To<gfx::Rect>(),
          input->needs_blending,
          input->surface_quad_state->surface.To<cc::SurfaceId>());
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
          sqs, input->rect.To<gfx::Rect>(), input->opaque_rect.To<gfx::Rect>(),
          input->visible_rect.To<gfx::Rect>(), input->needs_blending,
          texture_quad_state->resource_id, gfx::Size(),
          texture_quad_state->premultiplied_alpha,
          texture_quad_state->uv_top_left.To<gfx::PointF>(),
          texture_quad_state->uv_bottom_right.To<gfx::PointF>(),
          texture_quad_state->background_color.To<SkColor>(),
          &texture_quad_state->vertex_opacity.storage()[0],
          texture_quad_state->y_flipped, texture_quad_state->nearest_neighbor);
      break;
    }
    case mus::mojom::Material::TILED_CONTENT: {
      TileQuadStatePtr& tile_state = input->tile_quad_state;
      if (tile_state.is_null())
        return false;
      cc::TileDrawQuad* tile_quad =
          render_pass->CreateAndAppendDrawQuad<cc::TileDrawQuad>();
      tile_quad->SetAll(sqs,
                        input->rect.To<gfx::Rect>(),
                        input->opaque_rect.To<gfx::Rect>(),
                        input->visible_rect.To<gfx::Rect>(),
                        input->needs_blending,
                        tile_state->resource_id,
                        tile_state->tex_coord_rect.To<gfx::RectF>(),
                        tile_state->texture_size.To<gfx::Size>(),
                        tile_state->swizzle_contents,
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
          sqs, input->rect.To<gfx::Rect>(), input->opaque_rect.To<gfx::Rect>(),
          input->visible_rect.To<gfx::Rect>(), input->needs_blending,
          yuv_state->ya_tex_coord_rect.To<gfx::RectF>(),
          yuv_state->uv_tex_coord_rect.To<gfx::RectF>(),
          yuv_state->ya_tex_size.To<gfx::Size>(),
          yuv_state->uv_tex_size.To<gfx::Size>(),
          yuv_state->y_plane_resource_id, yuv_state->u_plane_resource_id,
          yuv_state->v_plane_resource_id, yuv_state->a_plane_resource_id,
          static_cast<cc::YUVVideoDrawQuad::ColorSpace>(
              yuv_state->color_space));
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
SurfaceIdPtr TypeConverter<SurfaceIdPtr, cc::SurfaceId>::Convert(
    const cc::SurfaceId& input) {
  SurfaceIdPtr id(SurfaceId::New());
  id->local = static_cast<uint32_t>(input.id);
  id->id_namespace = cc::SurfaceIdAllocator::NamespaceForId(input);
  return id;
}

// static
cc::SurfaceId TypeConverter<cc::SurfaceId, SurfaceIdPtr>::Convert(
    const SurfaceIdPtr& input) {
  uint64_t packed_id = input->id_namespace;
  packed_id <<= 32ull;
  packed_id |= input->local;
  return cc::SurfaceId(packed_id);
}

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
RenderPassIdPtr TypeConverter<RenderPassIdPtr, cc::RenderPassId>::Convert(
    const cc::RenderPassId& input) {
  RenderPassIdPtr pass_id(RenderPassId::New());
  pass_id->layer_id = input.layer_id;
  DCHECK_LE(input.index,
            static_cast<size_t>(std::numeric_limits<uint32_t>::max()));
  pass_id->index = static_cast<uint32_t>(input.index);
  return pass_id;
}

// static
cc::RenderPassId TypeConverter<cc::RenderPassId, RenderPassIdPtr>::Convert(
    const RenderPassIdPtr& input) {
  return cc::RenderPassId(input->layer_id, input->index);
}

// static
QuadPtr TypeConverter<QuadPtr, cc::DrawQuad>::Convert(
    const cc::DrawQuad& input) {
  QuadPtr quad = Quad::New();
  quad->material = static_cast<mus::mojom::Material>(input.material);
  quad->rect = Rect::From(input.rect);
  quad->opaque_rect = Rect::From(input.opaque_rect);
  quad->visible_rect = Rect::From(input.visible_rect);
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
      pass_state->render_pass_id =
          RenderPassId::From(render_pass_quad->render_pass_id);
      pass_state->mask_resource_id = render_pass_quad->mask_resource_id();
      pass_state->mask_uv_scale = PointF::From(
          gfx::PointAtOffsetFromOrigin(render_pass_quad->mask_uv_scale));
      pass_state->mask_texture_size =
          Size::From(render_pass_quad->mask_texture_size);
      // TODO(jamesr): pass_state->filters
      pass_state->filters_scale = PointF::From(
          gfx::PointAtOffsetFromOrigin(render_pass_quad->filters_scale));
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
      surface_state->surface = SurfaceId::From(surface_quad->surface_id);
      quad->surface_quad_state = std::move(surface_state);
      break;
    }
    case cc::DrawQuad::TEXTURE_CONTENT: {
      const cc::TextureDrawQuad* texture_quad =
          cc::TextureDrawQuad::MaterialCast(&input);
      TextureQuadStatePtr texture_state = TextureQuadState::New();
      texture_state->resource_id = texture_quad->resource_id();
      texture_state->premultiplied_alpha = texture_quad->premultiplied_alpha;
      texture_state->uv_top_left = PointF::From(texture_quad->uv_top_left);
      texture_state->uv_bottom_right =
          PointF::From(texture_quad->uv_bottom_right);
      texture_state->background_color =
          Color::From(texture_quad->background_color);
      Array<float> vertex_opacity(4);
      for (size_t i = 0; i < 4; ++i) {
        vertex_opacity[i] = texture_quad->vertex_opacity[i];
      }
      texture_state->vertex_opacity = std::move(vertex_opacity);
      texture_state->y_flipped = texture_quad->y_flipped;
      quad->texture_quad_state = std::move(texture_state);
      break;
    }
    case cc::DrawQuad::TILED_CONTENT: {
      const cc::TileDrawQuad* tile_quad =
          cc::TileDrawQuad::MaterialCast(&input);
      TileQuadStatePtr tile_state = TileQuadState::New();
      tile_state->tex_coord_rect = RectF::From(tile_quad->tex_coord_rect);
      tile_state->texture_size = Size::From(tile_quad->texture_size);
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
      yuv_state->ya_tex_coord_rect = RectF::From(yuv_quad->ya_tex_coord_rect);
      yuv_state->uv_tex_coord_rect = RectF::From(yuv_quad->uv_tex_coord_rect);
      yuv_state->ya_tex_size = Size::From(yuv_quad->ya_tex_size);
      yuv_state->uv_tex_size = Size::From(yuv_quad->uv_tex_size);
      yuv_state->y_plane_resource_id = yuv_quad->y_plane_resource_id();
      yuv_state->u_plane_resource_id = yuv_quad->u_plane_resource_id();
      yuv_state->v_plane_resource_id = yuv_quad->v_plane_resource_id();
      yuv_state->a_plane_resource_id = yuv_quad->a_plane_resource_id();
      yuv_state->color_space =
          static_cast<YUVColorSpace>(yuv_quad->color_space);
      quad->yuv_video_quad_state = std::move(yuv_state);
      break;
    }

    default:
      NOTREACHED() << "Unsupported material " << input.material;
  }
  return quad;
}

// static
mus::mojom::SharedQuadStatePtr
TypeConverter<mus::mojom::SharedQuadStatePtr, cc::SharedQuadState>::Convert(
    const cc::SharedQuadState& input) {
  mus::mojom::SharedQuadStatePtr state = SharedQuadState::New();
  state->quad_to_target_transform =
      Transform::From(input.quad_to_target_transform);
  state->quad_layer_bounds = Size::From(input.quad_layer_bounds);
  state->visible_quad_layer_rect = Rect::From(input.visible_quad_layer_rect);
  state->clip_rect = Rect::From(input.clip_rect);
  state->is_clipped = input.is_clipped;
  state->opacity = input.opacity;
  state->blend_mode = static_cast<mus::mojom::SkXfermode>(input.blend_mode);
  state->sorting_context_id = input.sorting_context_id;
  return state;
}

// static
PassPtr TypeConverter<PassPtr, cc::RenderPass>::Convert(
    const cc::RenderPass& input) {
  PassPtr pass = Pass::New();
  pass->id = RenderPassId::From(input.id);
  pass->output_rect = Rect::From(input.output_rect);
  pass->damage_rect = Rect::From(input.damage_rect);
  pass->transform_to_root_target =
      Transform::From(input.transform_to_root_target);
  pass->has_transparent_background = input.has_transparent_background;
  Array<QuadPtr> quads(input.quad_list.size());
  Array<mus::mojom::SharedQuadStatePtr> shared_quad_state(
      input.shared_quad_state_list.size());
  const cc::SharedQuadState* last_sqs = nullptr;
  cc::SharedQuadStateList::ConstIterator next_sqs_iter =
      input.shared_quad_state_list.begin();
  for (auto iter = input.quad_list.cbegin(); iter != input.quad_list.cend();
       ++iter) {
    const cc::DrawQuad& quad = **iter;
    quads[iter.index()] = Quad::From(quad);
    if (quad.shared_quad_state != last_sqs) {
      shared_quad_state[next_sqs_iter.index()] =
          SharedQuadState::From(**next_sqs_iter);
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
scoped_ptr<cc::RenderPass> ConvertToRenderPass(
    const PassPtr& input,
    const CompositorFrameMetadataPtr& metadata,
    CustomSurfaceConverter* custom_converter) {
  scoped_ptr<cc::RenderPass> pass = cc::RenderPass::Create(
      input->shared_quad_states.size(), input->quads.size());
  pass->SetAll(input->id.To<cc::RenderPassId>(),
               input->output_rect.To<gfx::Rect>(),
               input->damage_rect.To<gfx::Rect>(),
               input->transform_to_root_target.To<gfx::Transform>(),
               input->has_transparent_background);
  for (size_t i = 0; i < input->shared_quad_states.size(); ++i) {
    ConvertSharedQuadState(input->shared_quad_states[i], pass.get());
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
      return scoped_ptr<cc::RenderPass>();
  }
  return pass;
}

// static
scoped_ptr<cc::RenderPass>
TypeConverter<scoped_ptr<cc::RenderPass>, PassPtr>::Convert(
    const PassPtr& input) {
  mus::mojom::CompositorFrameMetadataPtr metadata;
  return ConvertToRenderPass(input, metadata,
                             nullptr /* CustomSurfaceConverter */);
}

// static
MailboxPtr TypeConverter<MailboxPtr, gpu::Mailbox>::Convert(
    const gpu::Mailbox& input) {
  Array<int8_t> name(64);
  for (int i = 0; i < 64; ++i) {
    name[i] = input.name[i];
  }
  MailboxPtr mailbox(Mailbox::New());
  mailbox->name = std::move(name);
  return mailbox;
}

// static
gpu::Mailbox TypeConverter<gpu::Mailbox, MailboxPtr>::Convert(
    const MailboxPtr& input) {
  gpu::Mailbox mailbox;
  if (!input->name.is_null())
    mailbox.SetName(&input->name.storage()[0]);
  return mailbox;
}

// static
SyncTokenPtr TypeConverter<SyncTokenPtr, gpu::SyncToken>::Convert(
    const gpu::SyncToken& input) {
  DCHECK(!input.HasData() || input.verified_flush());
  SyncTokenPtr sync_token(SyncToken::New());
  sync_token->verified_flush = input.verified_flush();
  sync_token->namespace_id =
      static_cast<CommandBufferNamespace>(input.namespace_id());
  sync_token->extra_data_field = input.extra_data_field();
  sync_token->command_buffer_id = input.command_buffer_id();
  sync_token->release_count = input.release_count();
  return sync_token;
}

// static
gpu::SyncToken TypeConverter<gpu::SyncToken, SyncTokenPtr>::Convert(
    const SyncTokenPtr& input) {
  const gpu::CommandBufferNamespace namespace_id =
      static_cast<gpu::CommandBufferNamespace>(input->namespace_id);
  gpu::SyncToken sync_token(namespace_id, input->extra_data_field,
                            input->command_buffer_id, input->release_count);
  if (input->verified_flush)
    sync_token.SetVerifyFlush();

  return sync_token;
}

// static
MailboxHolderPtr TypeConverter<MailboxHolderPtr, gpu::MailboxHolder>::Convert(
    const gpu::MailboxHolder& input) {
  MailboxHolderPtr holder(MailboxHolder::New());
  holder->mailbox = Mailbox::From<gpu::Mailbox>(input.mailbox);
  holder->sync_token = SyncToken::From<gpu::SyncToken>(input.sync_token);
  holder->texture_target = input.texture_target;
  return holder;
}

// static
gpu::MailboxHolder TypeConverter<gpu::MailboxHolder, MailboxHolderPtr>::Convert(
    const MailboxHolderPtr& input) {
  gpu::MailboxHolder holder;
  holder.mailbox = input->mailbox.To<gpu::Mailbox>();
  holder.sync_token = input->sync_token.To<gpu::SyncToken>();
  holder.texture_target = input->texture_target;
  return holder;
}

// static
TransferableResourcePtr
TypeConverter<TransferableResourcePtr, cc::TransferableResource>::Convert(
    const cc::TransferableResource& input) {
  TransferableResourcePtr transferable = TransferableResource::New();
  transferable->id = input.id;
  transferable->format = static_cast<ResourceFormat>(input.format);
  transferable->filter = input.filter;
  transferable->size = Size::From(input.size);
  transferable->mailbox_holder = MailboxHolder::From(input.mailbox_holder);
  transferable->read_lock_fences_enabled = input.read_lock_fences_enabled;
  transferable->is_software = input.is_software;
  transferable->is_overlay_candidate = input.is_overlay_candidate;
  return transferable;
}

// static
cc::TransferableResource
TypeConverter<cc::TransferableResource, TransferableResourcePtr>::Convert(
    const TransferableResourcePtr& input) {
  cc::TransferableResource transferable;
  transferable.id = input->id;
  transferable.format = static_cast<cc::ResourceFormat>(input->format);
  transferable.filter = input->filter;
  transferable.size = input->size.To<gfx::Size>();
  transferable.mailbox_holder = input->mailbox_holder.To<gpu::MailboxHolder>();
  transferable.read_lock_fences_enabled = input->read_lock_fences_enabled;
  transferable.is_software = input->is_software;
  transferable.is_overlay_candidate = input->is_overlay_candidate;
  return transferable;
}

// static
ReturnedResourcePtr
TypeConverter<ReturnedResourcePtr, cc::ReturnedResource>::Convert(
    const cc::ReturnedResource& input) {
  ReturnedResourcePtr returned = ReturnedResource::New();
  returned->id = input.id;
  returned->sync_token = SyncToken::From<gpu::SyncToken>(input.sync_token);
  returned->count = input.count;
  returned->lost = input.lost;
  return returned;
}

// static
cc::ReturnedResource
TypeConverter<cc::ReturnedResource, ReturnedResourcePtr>::Convert(
    const ReturnedResourcePtr& input) {
  cc::ReturnedResource returned;
  returned.id = input->id;
  returned.sync_token = input->sync_token.To<gpu::SyncToken>();
  returned.count = input->count;
  returned.lost = input->lost;
  return returned;
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
      Array<TransferableResourcePtr>::From(frame_data->resource_list);
  frame->metadata = CompositorFrameMetadata::From(input.metadata);
  const cc::RenderPassList& pass_list = frame_data->render_pass_list;
  frame->passes = Array<PassPtr>::New(pass_list.size());
  for (size_t i = 0; i < pass_list.size(); ++i) {
    frame->passes[i] = Pass::From(*pass_list[i]);
  }
  return frame;
}

// static
scoped_ptr<cc::CompositorFrame> ConvertToCompositorFrame(
    const CompositorFramePtr& input,
    CustomSurfaceConverter* custom_converter) {
  scoped_ptr<cc::DelegatedFrameData> frame_data(new cc::DelegatedFrameData);
  frame_data->device_scale_factor = 1.f;
  frame_data->resource_list =
      input->resources.To<cc::TransferableResourceArray>();
  frame_data->render_pass_list.reserve(input->passes.size());
  for (size_t i = 0; i < input->passes.size(); ++i) {
    scoped_ptr<cc::RenderPass> pass = ConvertToRenderPass(
        input->passes[i], input->metadata, custom_converter);
    if (!pass)
      return scoped_ptr<cc::CompositorFrame>();
    frame_data->render_pass_list.push_back(std::move(pass));
  }
  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  cc::CompositorFrameMetadata metadata =
      input->metadata.To<cc::CompositorFrameMetadata>();
  frame->delegated_frame_data = std::move(frame_data);
  return frame;
}

// static
scoped_ptr<cc::CompositorFrame>
TypeConverter<scoped_ptr<cc::CompositorFrame>, CompositorFramePtr>::Convert(
    const CompositorFramePtr& input) {
  return ConvertToCompositorFrame(input, nullptr);
}

}  // namespace mojo
