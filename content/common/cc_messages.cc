// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cc_messages.h"

#include "cc/compositor_frame.h"
#include "content/public/common/common_param_traits.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"
#include "ui/gfx/transform.h"

namespace IPC {

void ParamTraits<WebKit::WebFilterOperation>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.type());
  switch (p.type()) {
    case WebKit::WebFilterOperation::FilterTypeGrayscale:
    case WebKit::WebFilterOperation::FilterTypeSepia:
    case WebKit::WebFilterOperation::FilterTypeSaturate:
    case WebKit::WebFilterOperation::FilterTypeHueRotate:
    case WebKit::WebFilterOperation::FilterTypeInvert:
    case WebKit::WebFilterOperation::FilterTypeBrightness:
    case WebKit::WebFilterOperation::FilterTypeSaturatingBrightness:
    case WebKit::WebFilterOperation::FilterTypeContrast:
    case WebKit::WebFilterOperation::FilterTypeOpacity:
    case WebKit::WebFilterOperation::FilterTypeBlur:
      WriteParam(m, p.amount());
      break;
    case WebKit::WebFilterOperation::FilterTypeDropShadow:
      WriteParam(m, p.dropShadowOffset());
      WriteParam(m, p.amount());
      WriteParam(m, p.dropShadowColor());
      break;
    case WebKit::WebFilterOperation::FilterTypeColorMatrix:
      for (int i = 0; i < 20; ++i)
        WriteParam(m, p.matrix()[i]);
      break;
    case WebKit::WebFilterOperation::FilterTypeZoom:
      WriteParam(m, p.zoomRect());
      WriteParam(m, p.amount());
      break;
  }
}

bool ParamTraits<WebKit::WebFilterOperation>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  WebKit::WebFilterOperation::FilterType type;
  float amount;
  WebKit::WebPoint dropShadowOffset;
  WebKit::WebColor dropShadowColor;
  SkScalar matrix[20];
  WebKit::WebRect zoomRect;

  if (!ReadParam(m, iter, &type))
    return false;
  r->setType(type);

  bool success = false;
  switch (type) {
    case WebKit::WebFilterOperation::FilterTypeGrayscale:
    case WebKit::WebFilterOperation::FilterTypeSepia:
    case WebKit::WebFilterOperation::FilterTypeSaturate:
    case WebKit::WebFilterOperation::FilterTypeHueRotate:
    case WebKit::WebFilterOperation::FilterTypeInvert:
    case WebKit::WebFilterOperation::FilterTypeBrightness:
    case WebKit::WebFilterOperation::FilterTypeSaturatingBrightness:
    case WebKit::WebFilterOperation::FilterTypeContrast:
    case WebKit::WebFilterOperation::FilterTypeOpacity:
    case WebKit::WebFilterOperation::FilterTypeBlur:
      if (ReadParam(m, iter, &amount)) {
        r->setAmount(amount);
        success = true;
      }
      break;
    case WebKit::WebFilterOperation::FilterTypeDropShadow:
      if (ReadParam(m, iter, &dropShadowOffset) &&
          ReadParam(m, iter, &amount) &&
          ReadParam(m, iter, &dropShadowColor)) {
        r->setDropShadowOffset(dropShadowOffset);
        r->setAmount(amount);
        r->setDropShadowColor(dropShadowColor);
        success = true;
      }
      break;
    case WebKit::WebFilterOperation::FilterTypeColorMatrix: {
      int i;
      for (i = 0; i < 20; ++i) {
        if (!ReadParam(m, iter, &matrix[i]))
          break;
      }
      if (i == 20) {
        r->setMatrix(matrix);
        success = true;
      }
      break;
    }
    case WebKit::WebFilterOperation::FilterTypeZoom:
      if (ReadParam(m, iter, &zoomRect) &&
          ReadParam(m, iter, &amount)) {
        r->setZoomRect(zoomRect);
        r->setAmount(amount);
        success = true;
      }
      break;
  }
  return success;
}

void ParamTraits<WebKit::WebFilterOperation>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(static_cast<unsigned>(p.type()), l);
  l->append(", ");

  switch (p.type()) {
    case WebKit::WebFilterOperation::FilterTypeGrayscale:
    case WebKit::WebFilterOperation::FilterTypeSepia:
    case WebKit::WebFilterOperation::FilterTypeSaturate:
    case WebKit::WebFilterOperation::FilterTypeHueRotate:
    case WebKit::WebFilterOperation::FilterTypeInvert:
    case WebKit::WebFilterOperation::FilterTypeBrightness:
    case WebKit::WebFilterOperation::FilterTypeSaturatingBrightness:
    case WebKit::WebFilterOperation::FilterTypeContrast:
    case WebKit::WebFilterOperation::FilterTypeOpacity:
    case WebKit::WebFilterOperation::FilterTypeBlur:
      LogParam(p.amount(), l);
      break;
    case WebKit::WebFilterOperation::FilterTypeDropShadow:
      LogParam(p.dropShadowOffset(), l);
      l->append(", ");
      LogParam(p.amount(), l);
      l->append(", ");
      LogParam(p.dropShadowColor(), l);
      break;
    case WebKit::WebFilterOperation::FilterTypeColorMatrix:
      for (int i = 0; i < 20; ++i) {
        if (i)
          l->append(", ");
        LogParam(p.matrix()[i], l);
      }
      break;
    case WebKit::WebFilterOperation::FilterTypeZoom:
      LogParam(p.zoomRect(), l);
      l->append(", ");
      LogParam(p.amount(), l);
      break;
  }
  l->append(")");
}

void ParamTraits<WebKit::WebFilterOperations>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.size());
  for (std::size_t i = 0; i < p.size(); ++i) {
    WriteParam(m, p.at(i));
  }
}

bool ParamTraits<WebKit::WebFilterOperations>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  size_t count;
  if (!ReadParam(m, iter, &count))
    return false;

  for (std::size_t i = 0; i < count; ++i) {
    WebKit::WebFilterOperation op =
        WebKit::WebFilterOperation::createEmptyFilter();
    if (!ReadParam(m, iter, &op))
      return false;
    r->append(op);
  }
  return true;
}

void ParamTraits<WebKit::WebFilterOperations>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  for (std::size_t i = 0; i < p.size(); ++i) {
    if (i)
      l->append(", ");
    LogParam(p.at(i), l);
  }
  l->append(")");
}

void ParamTraits<gfx::Transform>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.matrix().getDouble(0, 0));
  WriteParam(m, p.matrix().getDouble(1, 0));
  WriteParam(m, p.matrix().getDouble(2, 0));
  WriteParam(m, p.matrix().getDouble(3, 0));
  WriteParam(m, p.matrix().getDouble(0, 1));
  WriteParam(m, p.matrix().getDouble(1, 1));
  WriteParam(m, p.matrix().getDouble(2, 1));
  WriteParam(m, p.matrix().getDouble(3, 1));
  WriteParam(m, p.matrix().getDouble(0, 2));
  WriteParam(m, p.matrix().getDouble(1, 2));
  WriteParam(m, p.matrix().getDouble(2, 2));
  WriteParam(m, p.matrix().getDouble(3, 2));
  WriteParam(m, p.matrix().getDouble(0, 3));
  WriteParam(m, p.matrix().getDouble(1, 3));
  WriteParam(m, p.matrix().getDouble(2, 3));
  WriteParam(m, p.matrix().getDouble(3, 3));
}

bool ParamTraits<gfx::Transform>::Read(
    const Message* m, PickleIterator* iter, param_type* r) {
  // Note: In this function, "m12" means 1st row, 2nd column of the matrix.
  // This is consistent with Skia's row-column notation, but backwards from
  // WebCore's column-row notation.
  double m11, m12, m13, m14, m21, m22, m23, m24, m31, m32, m33, m34,
         m41, m42, m43, m44;

  bool success =
      ReadParam(m, iter, &m11) &&
      ReadParam(m, iter, &m21) &&
      ReadParam(m, iter, &m31) &&
      ReadParam(m, iter, &m41) &&
      ReadParam(m, iter, &m12) &&
      ReadParam(m, iter, &m22) &&
      ReadParam(m, iter, &m32) &&
      ReadParam(m, iter, &m42) &&
      ReadParam(m, iter, &m13) &&
      ReadParam(m, iter, &m23) &&
      ReadParam(m, iter, &m33) &&
      ReadParam(m, iter, &m43) &&
      ReadParam(m, iter, &m14) &&
      ReadParam(m, iter, &m24) &&
      ReadParam(m, iter, &m34) &&
      ReadParam(m, iter, &m44);

  if (success) {
    r->matrix().setDouble(0, 0, m11);
    r->matrix().setDouble(1, 0, m21);
    r->matrix().setDouble(2, 0, m31);
    r->matrix().setDouble(3, 0, m41);
    r->matrix().setDouble(0, 1, m12);
    r->matrix().setDouble(1, 1, m22);
    r->matrix().setDouble(2, 1, m32);
    r->matrix().setDouble(3, 1, m42);
    r->matrix().setDouble(0, 2, m13);
    r->matrix().setDouble(1, 2, m23);
    r->matrix().setDouble(2, 2, m33);
    r->matrix().setDouble(3, 2, m43);
    r->matrix().setDouble(0, 3, m14);
    r->matrix().setDouble(1, 3, m24);
    r->matrix().setDouble(2, 3, m34);
    r->matrix().setDouble(3, 3, m44);
  }

  return success;
}

void ParamTraits<gfx::Transform>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.matrix().getDouble(0, 0), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(1, 0), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(2, 0), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(3, 0), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(0, 1), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(1, 1), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(2, 1), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(3, 1), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(0, 2), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(1, 2), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(2, 2), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(3, 2), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(0, 3), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(1, 3), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(2, 3), l);
  l->append(", ");
  LogParam(p.matrix().getDouble(3, 3), l);
  l->append(") ");
}

void ParamTraits<cc::RenderPass>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.id);
  WriteParam(m, p.output_rect);
  WriteParam(m, p.damage_rect);
  WriteParam(m, p.transform_to_root_target);
  WriteParam(m, p.has_transparent_background);
  WriteParam(m, p.has_occlusion_from_outside_target_surface);
  WriteParam(m, p.shared_quad_state_list.size());
  WriteParam(m, p.quad_list.size());

  for (size_t i = 0; i < p.shared_quad_state_list.size(); ++i)
    WriteParam(m, *p.shared_quad_state_list[i]);

  size_t shared_quad_state_index = 0;
  for (size_t i = 0; i < p.quad_list.size(); ++i) {
    const cc::DrawQuad* quad = p.quad_list[i];

    switch (quad->material) {
      case cc::DrawQuad::CHECKERBOARD:
        WriteParam(m, *cc::CheckerboardDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::DEBUG_BORDER:
        WriteParam(m, *cc::DebugBorderDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::IO_SURFACE_CONTENT:
        WriteParam(m, *cc::IOSurfaceDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::TEXTURE_CONTENT:
        WriteParam(m, *cc::TextureDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::RENDER_PASS:
        WriteParam(m, *cc::RenderPassDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::SOLID_COLOR:
        WriteParam(m, *cc::SolidColorDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::TILED_CONTENT:
        WriteParam(m, *cc::TileDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::STREAM_VIDEO_CONTENT:
        WriteParam(m, *cc::StreamVideoDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::YUV_VIDEO_CONTENT:
        WriteParam(m, *cc::YUVVideoDrawQuad::MaterialCast(quad));
        break;
      case cc::DrawQuad::INVALID:
        break;
    }

    const cc::ScopedPtrVector<cc::SharedQuadState>& sqs_list =
        p.shared_quad_state_list;

    // This is an invalid index.
    size_t bad_index = sqs_list.size();

    // Null shared quad states should not occur.
    DCHECK(quad->shared_quad_state);
    if (!quad->shared_quad_state) {
      WriteParam(m, bad_index);
      continue;
    }

    // SharedQuadStates should appear in the order they are used by DrawQuads.
    // Find the SharedQuadState for this DrawQuad.
    while (shared_quad_state_index < sqs_list.size() &&
           quad->shared_quad_state != sqs_list[shared_quad_state_index])
      ++shared_quad_state_index;

    DCHECK_LT(shared_quad_state_index, sqs_list.size());
    if (shared_quad_state_index >= sqs_list.size()) {
      WriteParam(m, bad_index);
      continue;
    }

    DCHECK_LT(shared_quad_state_index, p.shared_quad_state_list.size());
    WriteParam(m, shared_quad_state_index);
  }
}

template<typename QuadType>
static scoped_ptr<cc::DrawQuad> ReadDrawQuad(const Message* m,
                                             PickleIterator* iter) {
  scoped_ptr<QuadType> quad = QuadType::Create();
  if (!ReadParam(m, iter, quad.get()))
    return scoped_ptr<QuadType>(NULL).template PassAs<cc::DrawQuad>();
  return quad.template PassAs<cc::DrawQuad>();
}

bool ParamTraits<cc::RenderPass>::Read(
    const Message* m, PickleIterator* iter, param_type* p) {
  cc::RenderPass::Id id(-1, -1);
  gfx::Rect output_rect;
  gfx::RectF damage_rect;
  gfx::Transform transform_to_root_target;
  bool has_transparent_background;
  bool has_occlusion_from_outside_target_surface;
  size_t shared_quad_state_list_size;
  size_t quad_list_size;

  if (!ReadParam(m, iter, &id) ||
      !ReadParam(m, iter, &output_rect) ||
      !ReadParam(m, iter, &damage_rect) ||
      !ReadParam(m, iter, &transform_to_root_target) ||
      !ReadParam(m, iter, &has_transparent_background) ||
      !ReadParam(m, iter, &has_occlusion_from_outside_target_surface) ||
      !ReadParam(m, iter, &shared_quad_state_list_size) ||
      !ReadParam(m, iter, &quad_list_size))
    return false;

  p->SetAll(id,
            output_rect,
            damage_rect,
            transform_to_root_target,
            has_transparent_background,
            has_occlusion_from_outside_target_surface);

  for (size_t i = 0; i < shared_quad_state_list_size; ++i) {
    scoped_ptr<cc::SharedQuadState> state(cc::SharedQuadState::Create());
    if (!ReadParam(m, iter, state.get()))
      return false;
    p->shared_quad_state_list.push_back(state.Pass());
  }

  size_t last_shared_quad_state_index = 0;
  for (size_t i = 0; i < quad_list_size; ++i) {
    cc::DrawQuad::Material material;
    PickleIterator temp_iter = *iter;
    if (!ReadParam(m, &temp_iter, &material))
      return false;

    scoped_ptr<cc::DrawQuad> draw_quad;
    switch (material) {
      case cc::DrawQuad::CHECKERBOARD:
        draw_quad = ReadDrawQuad<cc::CheckerboardDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::DEBUG_BORDER:
        draw_quad = ReadDrawQuad<cc::DebugBorderDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::IO_SURFACE_CONTENT:
        draw_quad = ReadDrawQuad<cc::IOSurfaceDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::TEXTURE_CONTENT:
        draw_quad = ReadDrawQuad<cc::TextureDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::RENDER_PASS:
        draw_quad = ReadDrawQuad<cc::RenderPassDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::SOLID_COLOR:
        draw_quad = ReadDrawQuad<cc::SolidColorDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::TILED_CONTENT:
        draw_quad = ReadDrawQuad<cc::TileDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::STREAM_VIDEO_CONTENT:
        draw_quad = ReadDrawQuad<cc::StreamVideoDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::YUV_VIDEO_CONTENT:
        draw_quad = ReadDrawQuad<cc::YUVVideoDrawQuad>(m, iter);
        break;
      case cc::DrawQuad::INVALID:
        break;
    }
    if (!draw_quad)
      return false;

    size_t shared_quad_state_index;
    if (!ReadParam(m, iter, &shared_quad_state_index) ||
        shared_quad_state_index >= p->shared_quad_state_list.size())
      return false;
    // SharedQuadState indexes should be in ascending order.
    if (shared_quad_state_index < last_shared_quad_state_index)
      return false;
    last_shared_quad_state_index = shared_quad_state_index;

    draw_quad->shared_quad_state =
        p->shared_quad_state_list[shared_quad_state_index];

    p->quad_list.push_back(draw_quad.Pass());
  }

  return true;
}

void ParamTraits<cc::RenderPass>::Log(
    const param_type& p, std::string* l) {
  l->append("RenderPass((");
  LogParam(p.id, l);
  l->append("), ");
  LogParam(p.output_rect, l);
  l->append(", ");
  LogParam(p.damage_rect, l);
  l->append(", ");
  LogParam(p.transform_to_root_target, l);
  l->append(", ");
  LogParam(p.has_transparent_background, l);
  l->append(", ");
  LogParam(p.has_occlusion_from_outside_target_surface, l);
  l->append(", ");

  l->append("[");
  for (size_t i = 0; i < p.shared_quad_state_list.size(); ++i) {
    if (i)
      l->append(", ");
    LogParam(*p.shared_quad_state_list[i], l);
  }
  l->append("], [");
  for (size_t i = 0; i < p.quad_list.size(); ++i) {
    if (i)
      l->append(", ");
    const cc::DrawQuad* quad = p.quad_list[i];
    switch (quad->material) {
      case cc::DrawQuad::CHECKERBOARD:
        LogParam(*cc::CheckerboardDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::DEBUG_BORDER:
        LogParam(*cc::DebugBorderDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::IO_SURFACE_CONTENT:
        LogParam(*cc::IOSurfaceDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::TEXTURE_CONTENT:
        LogParam(*cc::TextureDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::RENDER_PASS:
        LogParam(*cc::RenderPassDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::SOLID_COLOR:
        LogParam(*cc::SolidColorDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::TILED_CONTENT:
        LogParam(*cc::TileDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::STREAM_VIDEO_CONTENT:
        LogParam(*cc::StreamVideoDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::YUV_VIDEO_CONTENT:
        LogParam(*cc::YUVVideoDrawQuad::MaterialCast(quad), l);
        break;
      case cc::DrawQuad::INVALID:
        break;
    }
  }
  l->append("])");
}

namespace {
  enum CompositorFrameType {
    NO_FRAME,
    DELEGATED_FRAME,
    GL_FRAME,
  };
}

void ParamTraits<cc::CompositorFrame>::Write(Message* m,
                                             const param_type& p) {
  WriteParam(m, p.metadata);
  if (p.delegated_frame_data) {
    DCHECK(!p.gl_frame_data);
    WriteParam(m, static_cast<int>(DELEGATED_FRAME));
    WriteParam(m, *p.delegated_frame_data);
  } else if (p.gl_frame_data) {
    WriteParam(m, static_cast<int>(GL_FRAME));
    WriteParam(m, *p.gl_frame_data);
  } else {
    WriteParam(m, static_cast<int>(NO_FRAME));
  }
}

bool ParamTraits<cc::CompositorFrame>::Read(const Message* m,
                                            PickleIterator* iter,
                                            param_type* p) {
  if (!ReadParam(m, iter, &p->metadata))
    return false;

  int compositor_frame_type;
  if (!ReadParam(m, iter, &compositor_frame_type))
    return false;

  switch (compositor_frame_type) {
    case DELEGATED_FRAME:
      p->delegated_frame_data.reset(new cc::DelegatedFrameData());
      if (!ReadParam(m, iter, p->delegated_frame_data.get()))
        return false;
      break;
    case GL_FRAME:
      p->gl_frame_data.reset(new cc::GLFrameData());
      if (!ReadParam(m, iter, p->gl_frame_data.get()))
        return false;
      break;
    case NO_FRAME:
      break;
    default:
      return false;
  }
  return true;
}

void ParamTraits<cc::CompositorFrame>::Log(const param_type& p,
                                           std::string* l) {
  l->append("CompositorFrame(");
  LogParam(p.metadata, l);
  l->append(", ");
  if (p.delegated_frame_data)
    LogParam(*p.delegated_frame_data, l);
  else if (p.gl_frame_data)
    LogParam(*p.gl_frame_data, l);
  l->append(")");
}

void ParamTraits<cc::CompositorFrameAck>::Write(Message* m,
                                                const param_type& p) {
  WriteParam(m, p.resources);
  if (p.gl_frame_data) {
    WriteParam(m, static_cast<int>(GL_FRAME));
    WriteParam(m, *p.gl_frame_data);
  } else {
    WriteParam(m, static_cast<int>(NO_FRAME));
  }
}

bool ParamTraits<cc::CompositorFrameAck>::Read(const Message* m,
                                               PickleIterator* iter,
                                               param_type* p) {
  if (!ReadParam(m, iter, &p->resources))
    return false;

  int compositor_frame_type;
  if (!ReadParam(m, iter, &compositor_frame_type))
    return false;

  switch (compositor_frame_type) {
    case NO_FRAME:
      break;
    case GL_FRAME:
      p->gl_frame_data.reset(new cc::GLFrameData());
      if (!ReadParam(m, iter, p->gl_frame_data.get()))
        return false;
      break;
    default:
      return false;
  }
  return true;
}

void ParamTraits<cc::CompositorFrameAck>::Log(const param_type& p,
                                              std::string* l) {
  l->append("CompositorFrameAck(");
  LogParam(p.resources, l);
  l->append(", ");
  if (p.gl_frame_data)
    LogParam(*p.gl_frame_data, l);
  l->append(")");
}

void ParamTraits<cc::DelegatedFrameData>::Write(Message* m,
                                                const param_type& p) {
  WriteParam(m, p.resource_list);
  WriteParam(m, p.render_pass_list.size());
  for (size_t i = 0; i < p.render_pass_list.size(); ++i)
    WriteParam(m, *p.render_pass_list[i]);
}

bool ParamTraits<cc::DelegatedFrameData>::Read(const Message* m,
                                               PickleIterator* iter,
                                               param_type* p) {
  const static size_t kMaxRenderPasses = 10000;

  size_t num_render_passes;
  if (!ReadParam(m, iter, &p->resource_list) ||
      !ReadParam(m, iter, &num_render_passes) ||
      num_render_passes > kMaxRenderPasses)
    return false;
  for (size_t i = 0; i < num_render_passes; ++i) {
    scoped_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
    if (!ReadParam(m, iter, render_pass.get()))
      return false;
    p->render_pass_list.push_back(render_pass.Pass());
  }
  return true;
}

void ParamTraits<cc::DelegatedFrameData>::Log(const param_type& p,
                                              std::string* l) {
  l->append("DelegatedFrameData(");
  LogParam(p.resource_list, l);
  l->append(", [");
  for (size_t i = 0; i < p.render_pass_list.size(); ++i) {
    if (i)
      l->append(", ");
    LogParam(*p.render_pass_list[i], l);
  }
  l->append("])");
}

}  // namespace IPC
