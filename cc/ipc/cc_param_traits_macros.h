// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_CC_PARAM_TRAITS_MACROS_H_
#define CC_IPC_CC_PARAM_TRAITS_MACROS_H_

#include "cc/base/filter_operation.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/common/surfaces/surface_sequence.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
#include "ui/latency/ipc/latency_info_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CC_IPC_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(viz::DrawQuad::Material, viz::DrawQuad::MATERIAL_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(cc::FilterOperation::FilterType,
                          cc::FilterOperation::FILTER_TYPE_LAST)
// TODO(wutao): This trait belongs with skia code.
IPC_ENUM_TRAITS_MAX_VALUE(SkBlurImageFilter::TileMode,
                          SkBlurImageFilter::kMax_TileMode)
IPC_ENUM_TRAITS_MAX_VALUE(viz::ResourceFormat, viz::RESOURCE_FORMAT_MAX)

// TODO(fsamuel): This trait belongs with skia code.
IPC_ENUM_TRAITS_MAX_VALUE(SkBlendMode, SkBlendMode::kLastMode)

IPC_STRUCT_TRAITS_BEGIN(viz::SurfaceSequence)
  IPC_STRUCT_TRAITS_MEMBER(frame_sink_id)
  IPC_STRUCT_TRAITS_MEMBER(sequence)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(material)
  IPC_STRUCT_TRAITS_MEMBER(rect)
  IPC_STRUCT_TRAITS_MEMBER(visible_rect)
  IPC_STRUCT_TRAITS_MEMBER(needs_blending)
  IPC_STRUCT_TRAITS_MEMBER(resources)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::DebugBorderDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(viz::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(width)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::RenderPassDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(viz::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(render_pass_id)
  IPC_STRUCT_TRAITS_MEMBER(mask_uv_rect)
  IPC_STRUCT_TRAITS_MEMBER(mask_texture_size)
  IPC_STRUCT_TRAITS_MEMBER(filters_scale)
  IPC_STRUCT_TRAITS_MEMBER(filters_origin)
  IPC_STRUCT_TRAITS_MEMBER(tex_coord_rect)
  IPC_STRUCT_TRAITS_MEMBER(force_anti_aliasing_off)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::SolidColorDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(viz::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(force_anti_aliasing_off)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::StreamVideoDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(viz::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(overlay_resources)
  IPC_STRUCT_TRAITS_MEMBER(matrix)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::StreamVideoDrawQuad::OverlayResources)
  IPC_STRUCT_TRAITS_MEMBER(size_in_pixels)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::SurfaceDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(viz::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(primary_surface_id)
  IPC_STRUCT_TRAITS_MEMBER(fallback_surface_id)
  IPC_STRUCT_TRAITS_MEMBER(default_background_color)
  IPC_STRUCT_TRAITS_MEMBER(stretch_content_to_fill_bounds)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::TextureDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(viz::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(overlay_resources)
  IPC_STRUCT_TRAITS_MEMBER(premultiplied_alpha)
  IPC_STRUCT_TRAITS_MEMBER(uv_top_left)
  IPC_STRUCT_TRAITS_MEMBER(uv_bottom_right)
  IPC_STRUCT_TRAITS_MEMBER(background_color)
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[0])
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[1])
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[2])
  IPC_STRUCT_TRAITS_MEMBER(vertex_opacity[3])
  IPC_STRUCT_TRAITS_MEMBER(y_flipped)
  IPC_STRUCT_TRAITS_MEMBER(nearest_neighbor)
  IPC_STRUCT_TRAITS_MEMBER(secure_output_only)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::TextureDrawQuad::OverlayResources)
  IPC_STRUCT_TRAITS_MEMBER(size_in_pixels)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::TileDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(viz::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(tex_coord_rect)
  IPC_STRUCT_TRAITS_MEMBER(texture_size)
  IPC_STRUCT_TRAITS_MEMBER(swizzle_contents)
  IPC_STRUCT_TRAITS_MEMBER(nearest_neighbor)
  IPC_STRUCT_TRAITS_MEMBER(force_anti_aliasing_off)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::SharedQuadState)
  IPC_STRUCT_TRAITS_MEMBER(quad_to_target_transform)
  IPC_STRUCT_TRAITS_MEMBER(quad_layer_rect)
  IPC_STRUCT_TRAITS_MEMBER(visible_quad_layer_rect)
  IPC_STRUCT_TRAITS_MEMBER(clip_rect)
  IPC_STRUCT_TRAITS_MEMBER(is_clipped)
  IPC_STRUCT_TRAITS_MEMBER(opacity)
  IPC_STRUCT_TRAITS_MEMBER(blend_mode)
  IPC_STRUCT_TRAITS_MEMBER(sorting_context_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::TransferableResource)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(format)
  IPC_STRUCT_TRAITS_MEMBER(buffer_format)
  IPC_STRUCT_TRAITS_MEMBER(filter)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(mailbox_holder)
  IPC_STRUCT_TRAITS_MEMBER(read_lock_fences_enabled)
  IPC_STRUCT_TRAITS_MEMBER(is_software)
  IPC_STRUCT_TRAITS_MEMBER(shared_bitmap_sequence_number)
  IPC_STRUCT_TRAITS_MEMBER(is_overlay_candidate)
  IPC_STRUCT_TRAITS_MEMBER(color_space)
#if defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(is_backed_by_surface_texture)
  IPC_STRUCT_TRAITS_MEMBER(wants_promotion_hint)
#endif
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::ReturnedResource)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(sync_token)
  IPC_STRUCT_TRAITS_MEMBER(count)
  IPC_STRUCT_TRAITS_MEMBER(lost)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::Selection<gfx::SelectionBound>)
  IPC_STRUCT_TRAITS_MEMBER(start)
  IPC_STRUCT_TRAITS_MEMBER(end)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(viz::BeginFrameArgs::BeginFrameArgsType,
                          viz::BeginFrameArgs::BEGIN_FRAME_ARGS_TYPE_MAX - 1)

IPC_STRUCT_TRAITS_BEGIN(viz::BeginFrameArgs)
  IPC_STRUCT_TRAITS_MEMBER(frame_time)
  IPC_STRUCT_TRAITS_MEMBER(deadline)
  IPC_STRUCT_TRAITS_MEMBER(interval)
  IPC_STRUCT_TRAITS_MEMBER(sequence_number)
  IPC_STRUCT_TRAITS_MEMBER(source_id)
  IPC_STRUCT_TRAITS_MEMBER(type)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(viz::CompositorFrameMetadata)
  IPC_STRUCT_TRAITS_MEMBER(device_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(root_scroll_offset)
  IPC_STRUCT_TRAITS_MEMBER(page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(scrollable_viewport_size)
  IPC_STRUCT_TRAITS_MEMBER(root_layer_size)
  IPC_STRUCT_TRAITS_MEMBER(min_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(max_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(root_overflow_x_hidden)
  IPC_STRUCT_TRAITS_MEMBER(root_overflow_y_hidden)
  IPC_STRUCT_TRAITS_MEMBER(root_overflow_y_hidden)
  IPC_STRUCT_TRAITS_MEMBER(may_contain_video)
  IPC_STRUCT_TRAITS_MEMBER(
      is_resourceless_software_draw_with_scroll_or_animation)
  IPC_STRUCT_TRAITS_MEMBER(top_controls_height)
  IPC_STRUCT_TRAITS_MEMBER(top_controls_shown_ratio)
  IPC_STRUCT_TRAITS_MEMBER(bottom_controls_height)
  IPC_STRUCT_TRAITS_MEMBER(bottom_controls_shown_ratio)
  IPC_STRUCT_TRAITS_MEMBER(root_background_color)
  IPC_STRUCT_TRAITS_MEMBER(selection)
  IPC_STRUCT_TRAITS_MEMBER(latency_info)
  IPC_STRUCT_TRAITS_MEMBER(referenced_surfaces)
  IPC_STRUCT_TRAITS_MEMBER(activation_dependencies)
  IPC_STRUCT_TRAITS_MEMBER(content_source_id)
  IPC_STRUCT_TRAITS_MEMBER(begin_frame_ack)
  IPC_STRUCT_TRAITS_MEMBER(frame_token)
IPC_STRUCT_TRAITS_END()

#endif  // CC_IPC_CC_PARAM_TRAITS_MACROS_H_
