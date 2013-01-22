// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IPC Messages sent between compositor instances.

#include "cc/checkerboard_draw_quad.h"
#include "cc/compositor_frame.h"
#include "cc/compositor_frame_ack.h"
#include "cc/debug_border_draw_quad.h"
#include "cc/draw_quad.h"
#include "cc/io_surface_draw_quad.h"
#include "cc/render_pass.h"
#include "cc/render_pass_draw_quad.h"
#include "cc/shared_quad_state.h"
#include "cc/solid_color_draw_quad.h"
#include "cc/stream_video_draw_quad.h"
#include "cc/texture_draw_quad.h"
#include "cc/tile_draw_quad.h"
#include "cc/transferable_resource.h"
#include "cc/video_layer_impl.h"
#include "cc/yuv_video_draw_quad.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperation.h"

#ifndef CONTENT_COMMON_CC_MESSAGES_H_
#define CONTENT_COMMON_CC_MESSAGES_H_

namespace cc {
class CompositorFrame;
}

namespace gfx {
class Transform;
}

namespace WebKit {
class WebData;
class WebFilterOperations;
}

namespace IPC {

template <>
struct ParamTraits<WebKit::WebFilterOperation> {
  typedef WebKit::WebFilterOperation param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<WebKit::WebFilterOperations> {
  typedef WebKit::WebFilterOperations param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<gfx::Transform> {
  typedef gfx::Transform param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct CONTENT_EXPORT ParamTraits<cc::RenderPass> {
  typedef cc::RenderPass param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct CONTENT_EXPORT ParamTraits<cc::Mailbox> {
  typedef cc::Mailbox param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct CONTENT_EXPORT ParamTraits<cc::CompositorFrame> {
  typedef cc::CompositorFrame param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct CONTENT_EXPORT ParamTraits<cc::DelegatedFrameData> {
  typedef cc::DelegatedFrameData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CONTENT_COMMON_CC_MESSAGES_H_

// Multiply-included message file, hence no include guard.

#define IPC_MESSAGE_START CCMsgStart
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS(cc::DrawQuad::Material)
IPC_ENUM_TRAITS(cc::IOSurfaceDrawQuad::Orientation)
IPC_ENUM_TRAITS(WebKit::WebFilterOperation::FilterType)

IPC_STRUCT_TRAITS_BEGIN(cc::RenderPass::Id)
  IPC_STRUCT_TRAITS_MEMBER(layer_id)
  IPC_STRUCT_TRAITS_MEMBER(index)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::VideoLayerImpl::FramePlane)
  IPC_STRUCT_TRAITS_MEMBER(resourceId)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(format)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(material)
  IPC_STRUCT_TRAITS_MEMBER(rect)
  IPC_STRUCT_TRAITS_MEMBER(opaque_rect)
  IPC_STRUCT_TRAITS_MEMBER(visible_rect)
  IPC_STRUCT_TRAITS_MEMBER(needs_blending)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::CheckerboardDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(color)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::DebugBorderDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(width)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::IOSurfaceDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(io_surface_size)
  IPC_STRUCT_TRAITS_MEMBER(io_surface_texture_id)
  IPC_STRUCT_TRAITS_MEMBER(orientation)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::RenderPassDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(is_replica)
  IPC_STRUCT_TRAITS_MEMBER(mask_resource_id)
  IPC_STRUCT_TRAITS_MEMBER(contents_changed_since_last_frame)
  IPC_STRUCT_TRAITS_MEMBER(mask_uv_rect)
  IPC_STRUCT_TRAITS_MEMBER(filters)
  // TODO(piman): filter isn't being serialized.
  // IPC_STRUCT_TRAITS_MEMBER(filter)
  IPC_STRUCT_TRAITS_MEMBER(background_filters)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::SolidColorDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(color)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::StreamVideoDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(texture_id)
  IPC_STRUCT_TRAITS_MEMBER(matrix)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::TextureDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(resource_id)
  IPC_STRUCT_TRAITS_MEMBER(premultiplied_alpha)
  IPC_STRUCT_TRAITS_MEMBER(uv_top_left)
  IPC_STRUCT_TRAITS_MEMBER(uv_bottom_right)
  IPC_STRUCT_TRAITS_MEMBER(flipped)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::TileDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(resource_id)
  IPC_STRUCT_TRAITS_MEMBER(tex_coord_rect)
  IPC_STRUCT_TRAITS_MEMBER(texture_size)
  IPC_STRUCT_TRAITS_MEMBER(swizzle_contents)
  IPC_STRUCT_TRAITS_MEMBER(left_edge_aa)
  IPC_STRUCT_TRAITS_MEMBER(top_edge_aa)
  IPC_STRUCT_TRAITS_MEMBER(right_edge_aa)
  IPC_STRUCT_TRAITS_MEMBER(bottom_edge_aa)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::YUVVideoDrawQuad)
  IPC_STRUCT_TRAITS_PARENT(cc::DrawQuad)
  IPC_STRUCT_TRAITS_MEMBER(tex_scale)
  IPC_STRUCT_TRAITS_MEMBER(y_plane)
  IPC_STRUCT_TRAITS_MEMBER(u_plane)
  IPC_STRUCT_TRAITS_MEMBER(v_plane)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::SharedQuadState)
  IPC_STRUCT_TRAITS_MEMBER(content_to_target_transform)
  IPC_STRUCT_TRAITS_MEMBER(visible_content_rect)
  IPC_STRUCT_TRAITS_MEMBER(clip_rect)
  IPC_STRUCT_TRAITS_MEMBER(is_clipped)
  IPC_STRUCT_TRAITS_MEMBER(opacity)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::TransferableResource)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(format)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(mailbox)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::TransferableResourceList)
  IPC_STRUCT_TRAITS_MEMBER(sync_point)
  IPC_STRUCT_TRAITS_MEMBER(resources)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::CompositorFrameAck)
  IPC_STRUCT_TRAITS_MEMBER(resources)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::CompositorFrameMetadata)
  IPC_STRUCT_TRAITS_MEMBER(root_scroll_offset)
  IPC_STRUCT_TRAITS_MEMBER(page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(viewport_size)
  IPC_STRUCT_TRAITS_MEMBER(root_layer_size)
  IPC_STRUCT_TRAITS_MEMBER(min_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(max_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(location_bar_offset)
  IPC_STRUCT_TRAITS_MEMBER(location_bar_content_translation)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(cc::GLFrameData)
  IPC_STRUCT_TRAITS_MEMBER(mailbox)
  IPC_STRUCT_TRAITS_MEMBER(sync_point)
IPC_STRUCT_TRAITS_END()
