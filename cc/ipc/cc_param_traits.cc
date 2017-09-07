// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/ipc/cc_param_traits.h"

#include <stddef.h>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "cc/base/filter_operations.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/largest_draw_quad.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/tile_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFlattenableSerialization.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

namespace IPC {

void ParamTraits<cc::FilterOperation>::Write(base::Pickle* m,
                                             const param_type& p) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::FilterOperation::Write");
  WriteParam(m, p.type());
  switch (p.type()) {
    case cc::FilterOperation::GRAYSCALE:
    case cc::FilterOperation::SEPIA:
    case cc::FilterOperation::SATURATE:
    case cc::FilterOperation::HUE_ROTATE:
    case cc::FilterOperation::INVERT:
    case cc::FilterOperation::BRIGHTNESS:
    case cc::FilterOperation::SATURATING_BRIGHTNESS:
    case cc::FilterOperation::CONTRAST:
    case cc::FilterOperation::OPACITY:
      WriteParam(m, p.amount());
      break;
    case cc::FilterOperation::BLUR:
      WriteParam(m, p.amount());
      WriteParam(m, p.blur_tile_mode());
      break;
    case cc::FilterOperation::DROP_SHADOW:
      WriteParam(m, p.drop_shadow_offset());
      WriteParam(m, p.amount());
      WriteParam(m, p.drop_shadow_color());
      break;
    case cc::FilterOperation::COLOR_MATRIX:
      for (int i = 0; i < 20; ++i)
        WriteParam(m, p.matrix()[i]);
      break;
    case cc::FilterOperation::ZOOM:
      WriteParam(m, p.amount());
      WriteParam(m, p.zoom_inset());
      break;
    case cc::FilterOperation::REFERENCE:
      WriteParam(m, p.image_filter());
      break;
    case cc::FilterOperation::ALPHA_THRESHOLD:
      WriteParam(m, p.amount());
      WriteParam(m, p.outer_threshold());
      WriteParam(m, p.shape());
      break;
  }
}

bool ParamTraits<cc::FilterOperation>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            param_type* r) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::FilterOperation::Read");
  cc::FilterOperation::FilterType type;
  float amount;
  float outer_threshold;
  gfx::Point drop_shadow_offset;
  SkColor drop_shadow_color;
  SkScalar matrix[20];
  cc::FilterOperation::ShapeRects shape;
  int zoom_inset;
  SkBlurImageFilter::TileMode tile_mode;

  if (!ReadParam(m, iter, &type))
    return false;
  r->set_type(type);

  bool success = false;
  switch (type) {
    case cc::FilterOperation::GRAYSCALE:
    case cc::FilterOperation::SEPIA:
    case cc::FilterOperation::SATURATE:
    case cc::FilterOperation::HUE_ROTATE:
    case cc::FilterOperation::INVERT:
    case cc::FilterOperation::BRIGHTNESS:
    case cc::FilterOperation::SATURATING_BRIGHTNESS:
    case cc::FilterOperation::CONTRAST:
    case cc::FilterOperation::OPACITY:
      if (ReadParam(m, iter, &amount)) {
        r->set_amount(amount);
        success = true;
      }
      break;
    case cc::FilterOperation::BLUR:
      if (ReadParam(m, iter, &amount) && ReadParam(m, iter, &tile_mode)) {
        r->set_amount(amount);
        r->set_blur_tile_mode(tile_mode);
        success = true;
      }
      break;
    case cc::FilterOperation::DROP_SHADOW:
      if (ReadParam(m, iter, &drop_shadow_offset) &&
          ReadParam(m, iter, &amount) &&
          ReadParam(m, iter, &drop_shadow_color)) {
        r->set_drop_shadow_offset(drop_shadow_offset);
        r->set_amount(amount);
        r->set_drop_shadow_color(drop_shadow_color);
        success = true;
      }
      break;
    case cc::FilterOperation::COLOR_MATRIX: {
      int i;
      for (i = 0; i < 20; ++i) {
        if (!ReadParam(m, iter, &matrix[i]))
          break;
      }
      if (i == 20) {
        r->set_matrix(matrix);
        success = true;
      }
      break;
    }
    case cc::FilterOperation::ZOOM:
      if (ReadParam(m, iter, &amount) && ReadParam(m, iter, &zoom_inset) &&
          amount >= 0.f && zoom_inset >= 0) {
        r->set_amount(amount);
        r->set_zoom_inset(zoom_inset);
        success = true;
      }
      break;
    case cc::FilterOperation::REFERENCE: {
      sk_sp<SkImageFilter> filter;
      if (!ReadParam(m, iter, &filter)) {
        success = false;
        break;
      }
      r->set_image_filter(std::move(filter));
      success = true;
      break;
    }
    case cc::FilterOperation::ALPHA_THRESHOLD:
      if (ReadParam(m, iter, &amount) && ReadParam(m, iter, &outer_threshold) &&
          ReadParam(m, iter, &shape) && amount >= 0.f &&
          outer_threshold >= 0.f) {
        r->set_amount(amount);
        r->set_outer_threshold(amount);
        r->set_shape(shape);
        success = true;
      }
      break;
  }
  return success;
}

void ParamTraits<cc::FilterOperation>::Log(const param_type& p,
                                           std::string* l) {
  l->append("(");
  LogParam(static_cast<unsigned>(p.type()), l);
  l->append(", ");

  switch (p.type()) {
    case cc::FilterOperation::GRAYSCALE:
    case cc::FilterOperation::SEPIA:
    case cc::FilterOperation::SATURATE:
    case cc::FilterOperation::HUE_ROTATE:
    case cc::FilterOperation::INVERT:
    case cc::FilterOperation::BRIGHTNESS:
    case cc::FilterOperation::SATURATING_BRIGHTNESS:
    case cc::FilterOperation::CONTRAST:
    case cc::FilterOperation::OPACITY:
      LogParam(p.amount(), l);
      break;
    case cc::FilterOperation::BLUR:
      LogParam(p.amount(), l);
      l->append(", ");
      LogParam(static_cast<int>(p.blur_tile_mode()), l);
      break;
    case cc::FilterOperation::DROP_SHADOW:
      LogParam(p.drop_shadow_offset(), l);
      l->append(", ");
      LogParam(p.amount(), l);
      l->append(", ");
      LogParam(p.drop_shadow_color(), l);
      break;
    case cc::FilterOperation::COLOR_MATRIX:
      for (int i = 0; i < 20; ++i) {
        if (i)
          l->append(", ");
        LogParam(p.matrix()[i], l);
      }
      break;
    case cc::FilterOperation::ZOOM:
      LogParam(p.amount(), l);
      l->append(", ");
      LogParam(p.zoom_inset(), l);
      break;
    case cc::FilterOperation::REFERENCE:
      LogParam(p.image_filter(), l);
      break;
    case cc::FilterOperation::ALPHA_THRESHOLD:
      LogParam(p.amount(), l);
      l->append(", ");
      LogParam(p.outer_threshold(), l);
      l->append(", ");
      LogParam(p.shape(), l);
      break;
  }
  l->append(")");
}

void ParamTraits<cc::FilterOperations>::Write(base::Pickle* m,
                                              const param_type& p) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::FilterOperations::Write");
  WriteParam(m, base::checked_cast<uint32_t>(p.size()));
  for (std::size_t i = 0; i < p.size(); ++i) {
    WriteParam(m, p.at(i));
  }
}

bool ParamTraits<cc::FilterOperations>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* r) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::FilterOperations::Read");
  uint32_t count;
  if (!ReadParam(m, iter, &count))
    return false;

  for (std::size_t i = 0; i < count; ++i) {
    cc::FilterOperation op = cc::FilterOperation::CreateEmptyFilter();
    if (!ReadParam(m, iter, &op))
      return false;
    r->Append(op);
  }
  return true;
}

void ParamTraits<cc::FilterOperations>::Log(const param_type& p,
                                            std::string* l) {
  l->append("(");
  for (std::size_t i = 0; i < p.size(); ++i) {
    if (i)
      l->append(", ");
    LogParam(p.at(i), l);
  }
  l->append(")");
}

void ParamTraits<sk_sp<SkImageFilter>>::Write(base::Pickle* m,
                                              const param_type& p) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::SkImageFilter::Write");
  SkImageFilter* filter = p.get();
  if (filter) {
    sk_sp<SkData> data(SkValidatingSerializeFlattenable(filter));
    m->WriteData(static_cast<const char*>(data->data()),
                 base::checked_cast<int>(data->size()));
  } else {
    m->WriteData(0, 0);
  }
}

bool ParamTraits<sk_sp<SkImageFilter>>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* r) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::SkImageFilter::Read");
  const char* data = 0;
  int length = 0;
  if (!iter->ReadData(&data, &length))
    return false;
  if (length > 0) {
    SkFlattenable* flattenable = SkValidatingDeserializeFlattenable(
        data, length, SkImageFilter::GetFlattenableType());
    *r = sk_sp<SkImageFilter>(static_cast<SkImageFilter*>(flattenable));
  } else {
    r->reset();
  }
  return true;
}

void ParamTraits<sk_sp<SkImageFilter>>::Log(const param_type& p,
                                            std::string* l) {
  l->append("(");
  LogParam(p.get() ? p->countInputs() : 0, l);
  l->append(")");
}

void ParamTraits<cc::RenderPass>::Write(base::Pickle* m, const param_type& p) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::RenderPass::Write");
  WriteParam(m, p.id);
  WriteParam(m, p.output_rect);
  WriteParam(m, p.damage_rect);
  WriteParam(m, p.transform_to_root_target);
  WriteParam(m, p.filters);
  WriteParam(m, p.background_filters);
  WriteParam(m, p.color_space);
  WriteParam(m, p.has_transparent_background);
  WriteParam(m, p.cache_render_pass);
  WriteParam(m, p.has_damage_from_contributing_content);
  WriteParam(m, base::checked_cast<uint32_t>(p.quad_list.size()));

  cc::SharedQuadStateList::ConstIterator shared_quad_state_iter =
      p.shared_quad_state_list.begin();
  cc::SharedQuadStateList::ConstIterator last_shared_quad_state_iter =
      p.shared_quad_state_list.end();
  for (auto* quad : p.quad_list) {
    DCHECK(quad->rect.Contains(quad->visible_rect))
        << quad->material << " rect: " << quad->rect.ToString()
        << " visible_rect: " << quad->visible_rect.ToString();

    switch (quad->material) {
      case viz::DrawQuad::DEBUG_BORDER:
        WriteParam(m, *cc::DebugBorderDrawQuad::MaterialCast(quad));
        break;
      case viz::DrawQuad::PICTURE_CONTENT:
        NOTREACHED();
        break;
      case viz::DrawQuad::TEXTURE_CONTENT:
        WriteParam(m, *cc::TextureDrawQuad::MaterialCast(quad));
        break;
      case viz::DrawQuad::RENDER_PASS:
        WriteParam(m, *cc::RenderPassDrawQuad::MaterialCast(quad));
        break;
      case viz::DrawQuad::SOLID_COLOR:
        WriteParam(m, *cc::SolidColorDrawQuad::MaterialCast(quad));
        break;
      case viz::DrawQuad::SURFACE_CONTENT:
        WriteParam(m, *cc::SurfaceDrawQuad::MaterialCast(quad));
        break;
      case viz::DrawQuad::TILED_CONTENT:
        WriteParam(m, *cc::TileDrawQuad::MaterialCast(quad));
        break;
      case viz::DrawQuad::STREAM_VIDEO_CONTENT:
        WriteParam(m, *cc::StreamVideoDrawQuad::MaterialCast(quad));
        break;
      case viz::DrawQuad::YUV_VIDEO_CONTENT:
        WriteParam(m, *cc::YUVVideoDrawQuad::MaterialCast(quad));
        break;
      case viz::DrawQuad::INVALID:
        break;
    }

    // Null shared quad states should not occur.
    DCHECK(quad->shared_quad_state);

    // SharedQuadStates should appear in the order they are used by DrawQuads.
    // Find the SharedQuadState for this DrawQuad.
    while (shared_quad_state_iter != p.shared_quad_state_list.end() &&
           quad->shared_quad_state != *shared_quad_state_iter) {
      ++shared_quad_state_iter;
    }

    DCHECK(shared_quad_state_iter != p.shared_quad_state_list.end());

    if (shared_quad_state_iter != last_shared_quad_state_iter) {
      WriteParam(m, true);
      WriteParam(m, **shared_quad_state_iter);
      last_shared_quad_state_iter = shared_quad_state_iter;
    } else {
      WriteParam(m, false);
    }
  }
}

static size_t ReserveSizeForRenderPassWrite(const cc::RenderPass& p) {
  size_t to_reserve = sizeof(cc::RenderPass);

  // Whether the quad points to a new shared quad state for each quad.
  to_reserve += p.quad_list.size() * sizeof(bool);

  // Shared quad state is only written when a quad contains a shared quad state
  // that has not been written.
  to_reserve += p.shared_quad_state_list.size() * sizeof(viz::SharedQuadState);

  // The largest quad type, verified by a unit test.
  to_reserve += p.quad_list.size() * cc::LargestDrawQuadSize();

  to_reserve +=
      sizeof(uint32_t) + p.filters.size() * sizeof(cc::FilterOperation);
  to_reserve += sizeof(uint32_t) +
                p.background_filters.size() * sizeof(cc::FilterOperation);

  return to_reserve;
}

template <typename QuadType>
static viz::DrawQuad* ReadDrawQuad(const base::Pickle* m,
                                   base::PickleIterator* iter,
                                   cc::RenderPass* render_pass) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::ReadDrawQuad");
  QuadType* quad = render_pass->CreateAndAppendDrawQuad<QuadType>();
  if (!ReadParam(m, iter, quad))
    return nullptr;
  return quad;
}

bool ParamTraits<cc::RenderPass>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::RenderPass::Read");
  uint64_t id;
  gfx::Rect output_rect;
  gfx::Rect damage_rect;
  gfx::Transform transform_to_root_target;
  cc::FilterOperations filters;
  cc::FilterOperations background_filters;
  gfx::ColorSpace color_space;
  bool has_transparent_background;
  bool cache_render_pass;
  bool has_damage_from_contributing_content;

  uint32_t quad_list_size;

  if (!ReadParam(m, iter, &id) || !ReadParam(m, iter, &output_rect) ||
      !ReadParam(m, iter, &damage_rect) ||
      !ReadParam(m, iter, &transform_to_root_target) ||
      !ReadParam(m, iter, &filters) ||
      !ReadParam(m, iter, &background_filters) ||
      !ReadParam(m, iter, &color_space) ||
      !ReadParam(m, iter, &has_transparent_background) ||
      !ReadParam(m, iter, &cache_render_pass) ||
      !ReadParam(m, iter, &has_damage_from_contributing_content) ||
      !ReadParam(m, iter, &quad_list_size))
    return false;

  p->SetAll(id, output_rect, damage_rect, transform_to_root_target, filters,
            background_filters, color_space, has_transparent_background,
            cache_render_pass, has_damage_from_contributing_content);

  viz::DrawQuad* last_draw_quad = nullptr;
  for (uint32_t i = 0; i < quad_list_size; ++i) {
    viz::DrawQuad::Material material;
    base::PickleIterator temp_iter = *iter;
    if (!ReadParam(m, &temp_iter, &material))
      return false;

    viz::DrawQuad* draw_quad = nullptr;
    switch (material) {
      case viz::DrawQuad::DEBUG_BORDER:
        draw_quad = ReadDrawQuad<cc::DebugBorderDrawQuad>(m, iter, p);
        break;
      case viz::DrawQuad::PICTURE_CONTENT:
        NOTREACHED();
        return false;
      case viz::DrawQuad::SURFACE_CONTENT:
        draw_quad = ReadDrawQuad<cc::SurfaceDrawQuad>(m, iter, p);
        break;
      case viz::DrawQuad::TEXTURE_CONTENT:
        draw_quad = ReadDrawQuad<cc::TextureDrawQuad>(m, iter, p);
        break;
      case viz::DrawQuad::RENDER_PASS:
        draw_quad = ReadDrawQuad<cc::RenderPassDrawQuad>(m, iter, p);
        break;
      case viz::DrawQuad::SOLID_COLOR:
        draw_quad = ReadDrawQuad<cc::SolidColorDrawQuad>(m, iter, p);
        break;
      case viz::DrawQuad::TILED_CONTENT:
        draw_quad = ReadDrawQuad<cc::TileDrawQuad>(m, iter, p);
        break;
      case viz::DrawQuad::STREAM_VIDEO_CONTENT:
        draw_quad = ReadDrawQuad<cc::StreamVideoDrawQuad>(m, iter, p);
        break;
      case viz::DrawQuad::YUV_VIDEO_CONTENT:
        draw_quad = ReadDrawQuad<cc::YUVVideoDrawQuad>(m, iter, p);
        break;
      case viz::DrawQuad::INVALID:
        break;
    }
    if (!draw_quad)
      return false;
    if (!draw_quad->rect.Contains(draw_quad->visible_rect)) {
      LOG(ERROR) << "Quad with invalid visible rect " << draw_quad->material
                 << " rect: " << draw_quad->rect.ToString()
                 << " visible_rect: " << draw_quad->visible_rect.ToString();
      return false;
    }

    bool has_new_shared_quad_state;
    if (!ReadParam(m, iter, &has_new_shared_quad_state))
      return false;

    // If the quad has a new shared quad state, read it in.
    if (has_new_shared_quad_state) {
      viz::SharedQuadState* state = p->CreateAndAppendSharedQuadState();
      if (!ReadParam(m, iter, state))
        return false;
    }

    draw_quad->shared_quad_state = p->shared_quad_state_list.back();
    // If this quad is a fallback SurfaceDrawQuad then update the previous
    // primary SurfaceDrawQuad to point to this quad.
    if (draw_quad->material == viz::DrawQuad::SURFACE_CONTENT) {
      const cc::SurfaceDrawQuad* surface_draw_quad =
          cc::SurfaceDrawQuad::MaterialCast(draw_quad);
      if (surface_draw_quad->surface_draw_quad_type ==
          cc::SurfaceDrawQuadType::FALLBACK) {
        // A fallback quad must immediately follow a primary SurfaceDrawQuad.
        if (!last_draw_quad ||
            last_draw_quad->material != viz::DrawQuad::SURFACE_CONTENT) {
          return false;
        }
        cc::SurfaceDrawQuad* last_surface_draw_quad =
            static_cast<cc::SurfaceDrawQuad*>(last_draw_quad);
        // Only one fallback quad is currently supported.
        if (last_surface_draw_quad->surface_draw_quad_type !=
            cc::SurfaceDrawQuadType::PRIMARY) {
          return false;
        }
        last_surface_draw_quad->fallback_quad = surface_draw_quad;
      }
    }
    last_draw_quad = draw_quad;
  }

  return true;
}

void ParamTraits<cc::RenderPass>::Log(const param_type& p, std::string* l) {
  l->append("RenderPass((");
  LogParam(p.id, l);
  l->append("), ");
  LogParam(p.output_rect, l);
  l->append(", ");
  LogParam(p.damage_rect, l);
  l->append(", ");
  LogParam(p.transform_to_root_target, l);
  l->append(", ");
  LogParam(p.filters, l);
  l->append(", ");
  LogParam(p.background_filters, l);
  l->append(", ");
  LogParam(p.color_space, l);
  l->append(", ");
  LogParam(p.has_transparent_background, l);
  l->append(", ");
  LogParam(p.cache_render_pass, l);
  l->append(", ");
  LogParam(p.has_damage_from_contributing_content, l);
  l->append(", ");

  l->append("[");
  for (auto* shared_quad_state : p.shared_quad_state_list) {
    if (shared_quad_state != p.shared_quad_state_list.front())
      l->append(", ");
    LogParam(*shared_quad_state, l);
  }
  l->append("], [");
  for (auto* quad : p.quad_list) {
    if (quad != p.quad_list.front())
      l->append(", ");
    switch (quad->material) {
      case viz::DrawQuad::DEBUG_BORDER:
        LogParam(*cc::DebugBorderDrawQuad::MaterialCast(quad), l);
        break;
      case viz::DrawQuad::PICTURE_CONTENT:
        NOTREACHED();
        break;
      case viz::DrawQuad::TEXTURE_CONTENT:
        LogParam(*cc::TextureDrawQuad::MaterialCast(quad), l);
        break;
      case viz::DrawQuad::RENDER_PASS:
        LogParam(*cc::RenderPassDrawQuad::MaterialCast(quad), l);
        break;
      case viz::DrawQuad::SOLID_COLOR:
        LogParam(*cc::SolidColorDrawQuad::MaterialCast(quad), l);
        break;
      case viz::DrawQuad::SURFACE_CONTENT:
        LogParam(*cc::SurfaceDrawQuad::MaterialCast(quad), l);
        break;
      case viz::DrawQuad::TILED_CONTENT:
        LogParam(*cc::TileDrawQuad::MaterialCast(quad), l);
        break;
      case viz::DrawQuad::STREAM_VIDEO_CONTENT:
        LogParam(*cc::StreamVideoDrawQuad::MaterialCast(quad), l);
        break;
      case viz::DrawQuad::YUV_VIDEO_CONTENT:
        LogParam(*cc::YUVVideoDrawQuad::MaterialCast(quad), l);
        break;
      case viz::DrawQuad::INVALID:
        break;
    }
  }
  l->append("])");
}

void ParamTraits<viz::FrameSinkId>::Write(base::Pickle* m,
                                          const param_type& p) {
  WriteParam(m, p.client_id());
  WriteParam(m, p.sink_id());
}

bool ParamTraits<viz::FrameSinkId>::Read(const base::Pickle* m,
                                         base::PickleIterator* iter,
                                         param_type* p) {
  uint32_t client_id;
  if (!ReadParam(m, iter, &client_id))
    return false;

  uint32_t sink_id;
  if (!ReadParam(m, iter, &sink_id))
    return false;

  *p = viz::FrameSinkId(client_id, sink_id);
  return true;
}

void ParamTraits<viz::FrameSinkId>::Log(const param_type& p, std::string* l) {
  l->append("viz::FrameSinkId(");
  LogParam(p.client_id(), l);
  l->append(", ");
  LogParam(p.sink_id(), l);
  l->append(")");
}

void ParamTraits<viz::LocalSurfaceId>::Write(base::Pickle* m,
                                             const param_type& p) {
  WriteParam(m, p.local_id());
  WriteParam(m, p.nonce());
}

bool ParamTraits<viz::LocalSurfaceId>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            param_type* p) {
  uint32_t local_id;
  if (!ReadParam(m, iter, &local_id))
    return false;

  base::UnguessableToken nonce;
  if (!ReadParam(m, iter, &nonce))
    return false;

  *p = viz::LocalSurfaceId(local_id, nonce);
  return true;
}

void ParamTraits<viz::LocalSurfaceId>::Log(const param_type& p,
                                           std::string* l) {
  l->append("viz::LocalSurfaceId(");
  LogParam(p.local_id(), l);
  l->append(", ");
  LogParam(p.nonce(), l);
  l->append(")");
}

void ParamTraits<viz::SurfaceId>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.frame_sink_id());
  WriteParam(m, p.local_surface_id());
}

bool ParamTraits<viz::SurfaceId>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  viz::FrameSinkId frame_sink_id;
  if (!ReadParam(m, iter, &frame_sink_id))
    return false;

  viz::LocalSurfaceId local_surface_id;
  if (!ReadParam(m, iter, &local_surface_id))
    return false;

  *p = viz::SurfaceId(frame_sink_id, local_surface_id);
  return true;
}

void ParamTraits<viz::SurfaceId>::Log(const param_type& p, std::string* l) {
  l->append("viz::SurfaceId(");
  LogParam(p.frame_sink_id(), l);
  l->append(", ");
  LogParam(p.local_surface_id(), l);
  l->append(")");
}

void ParamTraits<viz::SurfaceInfo>::Write(base::Pickle* m,
                                          const param_type& p) {
  WriteParam(m, p.id());
  WriteParam(m, p.device_scale_factor());
  WriteParam(m, p.size_in_pixels());
}

bool ParamTraits<viz::SurfaceInfo>::Read(const base::Pickle* m,
                                         base::PickleIterator* iter,
                                         param_type* p) {
  viz::SurfaceId surface_id;
  if (!ReadParam(m, iter, &surface_id))
    return false;

  float device_scale_factor;
  if (!ReadParam(m, iter, &device_scale_factor))
    return false;

  gfx::Size size_in_pixels;
  if (!ReadParam(m, iter, &size_in_pixels))
    return false;

  *p = viz::SurfaceInfo(surface_id, device_scale_factor, size_in_pixels);
  return p->is_valid();
}

void ParamTraits<viz::SurfaceInfo>::Log(const param_type& p, std::string* l) {
  l->append("viz::SurfaceInfo(");
  LogParam(p.id(), l);
  l->append(", ");
  LogParam(p.device_scale_factor(), l);
  l->append(", ");
  LogParam(p.size_in_pixels(), l);
  l->append(")");
}

void ParamTraits<cc::CompositorFrame>::Write(base::Pickle* m,
                                             const param_type& p) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::CompositorFrame::Write");
  WriteParam(m, p.metadata);
  size_t to_reserve = 0u;
  to_reserve += p.resource_list.size() * sizeof(viz::TransferableResource);
  for (const auto& pass : p.render_pass_list) {
    to_reserve += sizeof(size_t) * 2;
    to_reserve += ReserveSizeForRenderPassWrite(*pass);
  }
  m->Reserve(to_reserve);

  WriteParam(m, p.resource_list);
  WriteParam(m, base::checked_cast<uint32_t>(p.render_pass_list.size()));
  for (const auto& pass : p.render_pass_list) {
    WriteParam(m, base::checked_cast<uint32_t>(pass->quad_list.size()));
    WriteParam(
        m, base::checked_cast<uint32_t>(pass->shared_quad_state_list.size()));
    WriteParam(m, *pass);
  }
}

bool ParamTraits<cc::CompositorFrame>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            param_type* p) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::CompositorFrame::Read");
  if (!ReadParam(m, iter, &p->metadata))
    return false;

  const size_t kMaxRenderPasses = 10000;
  const size_t kMaxSharedQuadStateListSize = 100000;
  const size_t kMaxQuadListSize = 1000000;

  std::set<cc::RenderPassId> pass_id_set;

  uint32_t num_render_passes;
  if (!ReadParam(m, iter, &p->resource_list) ||
      !ReadParam(m, iter, &num_render_passes) || num_render_passes == 0 ||
      num_render_passes > kMaxRenderPasses)
    return false;
  for (uint32_t i = 0; i < num_render_passes; ++i) {
    uint32_t quad_list_size;
    uint32_t shared_quad_state_list_size;
    if (!ReadParam(m, iter, &quad_list_size) ||
        !ReadParam(m, iter, &shared_quad_state_list_size) ||
        quad_list_size > kMaxQuadListSize ||
        shared_quad_state_list_size > kMaxSharedQuadStateListSize)
      return false;
    std::unique_ptr<cc::RenderPass> render_pass =
        cc::RenderPass::Create(static_cast<size_t>(shared_quad_state_list_size),
                               static_cast<size_t>(quad_list_size));
    if (!ReadParam(m, iter, render_pass.get()))
      return false;
    // Validate that each RenderPassDrawQuad points at a valid RenderPass
    // earlier in the frame.
    for (const auto* quad : render_pass->quad_list) {
      if (quad->material != viz::DrawQuad::RENDER_PASS)
        continue;
      const cc::RenderPassDrawQuad* rpdq =
          cc::RenderPassDrawQuad::MaterialCast(quad);
      if (!pass_id_set.count(rpdq->render_pass_id))
        return false;
    }
    pass_id_set.insert(render_pass->id);
    p->render_pass_list.push_back(std::move(render_pass));
  }

  return true;
}

void ParamTraits<cc::CompositorFrame>::Log(const param_type& p,
                                           std::string* l) {
  l->append("CompositorFrame(");
  LogParam(p.metadata, l);
  l->append(", ");
  LogParam(p.resource_list, l);
  l->append(", [");
  for (size_t i = 0; i < p.render_pass_list.size(); ++i) {
    if (i)
      l->append(", ");
    LogParam(*p.render_pass_list[i], l);
  }
  l->append("])");
}

void ParamTraits<viz::DrawQuad::Resources>::Write(base::Pickle* m,
                                                  const param_type& p) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::DrawQuad::Resources::Write");
  DCHECK_LE(p.count, viz::DrawQuad::Resources::kMaxResourceIdCount);
  WriteParam(m, p.count);
  for (size_t i = 0; i < p.count; ++i)
    WriteParam(m, p.ids[i]);
}

bool ParamTraits<viz::DrawQuad::Resources>::Read(const base::Pickle* m,
                                                 base::PickleIterator* iter,
                                                 param_type* p) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "ParamTraits::DrawQuad::Resources::Read");
  if (!ReadParam(m, iter, &p->count))
    return false;
  if (p->count > viz::DrawQuad::Resources::kMaxResourceIdCount)
    return false;
  for (size_t i = 0; i < p->count; ++i) {
    if (!ReadParam(m, iter, &p->ids[i]))
      return false;
  }
  return true;
}

void ParamTraits<viz::DrawQuad::Resources>::Log(const param_type& p,
                                                std::string* l) {
  l->append("DrawQuad::Resources(");
  LogParam(p.count, l);
  l->append(", [");
  if (p.count > viz::DrawQuad::Resources::kMaxResourceIdCount) {
    l->append("])");
    return;
  }

  for (size_t i = 0; i < p.count; ++i) {
    LogParam(p.ids[i], l);
    if (i < (p.count - 1))
      l->append(", ");
  }
  l->append("])");
}

void ParamTraits<cc::YUVVideoDrawQuad>::Write(base::Pickle* m,
                                              const param_type& p) {
  ParamTraits<viz::DrawQuad>::Write(m, p);
  WriteParam(m, p.ya_tex_coord_rect);
  WriteParam(m, p.uv_tex_coord_rect);
  WriteParam(m, p.ya_tex_size);
  WriteParam(m, p.uv_tex_size);
  WriteParam(m, p.color_space);
  WriteParam(m, p.video_color_space);
  WriteParam(m, p.resource_offset);
  WriteParam(m, p.resource_multiplier);
  WriteParam(m, p.bits_per_channel);
}

bool ParamTraits<cc::YUVVideoDrawQuad>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* p) {
  return ParamTraits<viz::DrawQuad>::Read(m, iter, p) &&
         ReadParam(m, iter, &p->ya_tex_coord_rect) &&
         ReadParam(m, iter, &p->uv_tex_coord_rect) &&
         ReadParam(m, iter, &p->ya_tex_size) &&
         ReadParam(m, iter, &p->uv_tex_size) &&
         ReadParam(m, iter, &p->color_space) &&
         ReadParam(m, iter, &p->video_color_space) &&
         ReadParam(m, iter, &p->resource_offset) &&
         ReadParam(m, iter, &p->resource_multiplier) &&
         ReadParam(m, iter, &p->bits_per_channel) &&
         p->bits_per_channel >= cc::YUVVideoDrawQuad::kMinBitsPerChannel &&
         p->bits_per_channel <= cc::YUVVideoDrawQuad::kMaxBitsPerChannel;
}

void ParamTraits<cc::YUVVideoDrawQuad>::Log(const param_type& p,
                                            std::string* l) {
  l->append("(");
  ParamTraits<viz::DrawQuad>::Log(p, l);
  l->append(", ");
  LogParam(p.ya_tex_coord_rect, l);
  l->append(", ");
  LogParam(p.uv_tex_coord_rect, l);
  l->append(", ");
  LogParam(p.ya_tex_size, l);
  l->append(", ");
  LogParam(p.uv_tex_size, l);
  l->append(", ");
  LogParam(p.color_space, l);
  l->append(", ");
  LogParam(p.video_color_space, l);
  l->append(", ");
  LogParam(p.resource_offset, l);
  l->append(", ");
  LogParam(p.resource_multiplier, l);
  l->append(", ");
  LogParam(p.bits_per_channel, l);
  l->append("])");
}

void ParamTraits<viz::BeginFrameAck>::Write(base::Pickle* m,
                                            const param_type& p) {
  m->WriteUInt64(p.sequence_number);
  m->WriteUInt32(p.source_id);
  // |has_damage| is implicit through IPC message name, so not transmitted.
}

bool ParamTraits<viz::BeginFrameAck>::Read(const base::Pickle* m,
                                           base::PickleIterator* iter,
                                           param_type* p) {
  return iter->ReadUInt64(&p->sequence_number) &&
         p->sequence_number >= viz::BeginFrameArgs::kStartingFrameNumber &&
         iter->ReadUInt32(&p->source_id);
}

void ParamTraits<viz::BeginFrameAck>::Log(const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.sequence_number, l);
  l->append(", ");
  LogParam(p.source_id, l);
  l->append(")");
}

}  // namespace IPC

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef CC_IPC_CC_PARAM_TRAITS_MACROS_H_
#include "cc/ipc/cc_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef CC_IPC_CC_PARAM_TRAITS_MACROS_H_
#include "cc/ipc/cc_param_traits_macros.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#undef CC_IPC_CC_PARAM_TRAITS_MACROS_H_
#include "cc/ipc/cc_param_traits_macros.h"
}  // namespace IPC
