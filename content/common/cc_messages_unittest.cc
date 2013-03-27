// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cc_messages.h"

#include <string.h>

#include "cc/output/compositor_frame.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

using cc::CheckerboardDrawQuad;
using cc::DelegatedFrameData;
using cc::DebugBorderDrawQuad;
using cc::DrawQuad;
using cc::IOSurfaceDrawQuad;
using cc::PictureDrawQuad;
using cc::RenderPass;
using cc::RenderPassDrawQuad;
using cc::ResourceProvider;
using cc::SharedQuadState;
using cc::SolidColorDrawQuad;
using cc::TextureDrawQuad;
using cc::TileDrawQuad;
using cc::TransferableResource;
using cc::StreamVideoDrawQuad;
using cc::VideoLayerImpl;
using cc::YUVVideoDrawQuad;
using gfx::Transform;
using WebKit::WebFilterOperation;
using WebKit::WebFilterOperations;

namespace content {
namespace {

class CCMessagesTest : public testing::Test {
 protected:
  void Compare(const RenderPass* a, const RenderPass* b) {
    EXPECT_EQ(a->id, b->id);
    EXPECT_EQ(a->output_rect.ToString(), b->output_rect.ToString());
    EXPECT_EQ(a->damage_rect.ToString(), b->damage_rect.ToString());
    EXPECT_EQ(a->transform_to_root_target, b->transform_to_root_target);
    EXPECT_EQ(a->has_transparent_background, b->has_transparent_background);
    EXPECT_EQ(a->has_occlusion_from_outside_target_surface,
              b->has_occlusion_from_outside_target_surface);
  }

  void Compare(const SharedQuadState* a, const SharedQuadState* b) {
    EXPECT_EQ(a->content_to_target_transform, b->content_to_target_transform);
    EXPECT_EQ(a->content_bounds, b->content_bounds);
    EXPECT_EQ(a->visible_content_rect, b->visible_content_rect);
    EXPECT_EQ(a->clip_rect, b->clip_rect);
    EXPECT_EQ(a->is_clipped, b->is_clipped);
    EXPECT_EQ(a->opacity, b->opacity);
  }

  void Compare(const DrawQuad* a, const DrawQuad* b) {
    ASSERT_NE(DrawQuad::INVALID, a->material);
    ASSERT_EQ(a->material, b->material);
    EXPECT_EQ(a->rect.ToString(), b->rect.ToString());
    EXPECT_EQ(a->opaque_rect.ToString(), b->opaque_rect.ToString());
    EXPECT_EQ(a->visible_rect.ToString(), b->visible_rect.ToString());
    EXPECT_EQ(a->needs_blending, b->needs_blending);

    Compare(a->shared_quad_state, b->shared_quad_state);

    switch (a->material) {
      case DrawQuad::CHECKERBOARD:
        Compare(CheckerboardDrawQuad::MaterialCast(a),
                CheckerboardDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::DEBUG_BORDER:
        Compare(DebugBorderDrawQuad::MaterialCast(a),
                DebugBorderDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::IO_SURFACE_CONTENT:
        Compare(IOSurfaceDrawQuad::MaterialCast(a),
                IOSurfaceDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::PICTURE_CONTENT:
        Compare(PictureDrawQuad::MaterialCast(a),
                PictureDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::RENDER_PASS:
        Compare(RenderPassDrawQuad::MaterialCast(a),
                RenderPassDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::TEXTURE_CONTENT:
        Compare(TextureDrawQuad::MaterialCast(a),
                TextureDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::TILED_CONTENT:
        Compare(TileDrawQuad::MaterialCast(a),
                TileDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::SOLID_COLOR:
        Compare(SolidColorDrawQuad::MaterialCast(a),
                SolidColorDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::STREAM_VIDEO_CONTENT:
        Compare(StreamVideoDrawQuad::MaterialCast(a),
                StreamVideoDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::YUV_VIDEO_CONTENT:
        Compare(YUVVideoDrawQuad::MaterialCast(a),
                YUVVideoDrawQuad::MaterialCast(b));
        break;
      case DrawQuad::INVALID:
        break;
    }
  }

  void Compare(const CheckerboardDrawQuad* a, const CheckerboardDrawQuad* b) {
    EXPECT_EQ(a->color, b->color);
  }

  void Compare(const DebugBorderDrawQuad* a, const DebugBorderDrawQuad* b) {
    EXPECT_EQ(a->color, b->color);
    EXPECT_EQ(a->width, b->width);
  }

  void Compare(const IOSurfaceDrawQuad* a, const IOSurfaceDrawQuad* b) {
    EXPECT_EQ(a->io_surface_size.ToString(), b->io_surface_size.ToString());
    EXPECT_EQ(a->io_surface_texture_id, b->io_surface_texture_id);
    EXPECT_EQ(a->orientation, b->orientation);
  }

  void Compare(const RenderPassDrawQuad* a, const RenderPassDrawQuad* b) {
    EXPECT_EQ(a->is_replica, b->is_replica);
    EXPECT_EQ(a->mask_resource_id, b->mask_resource_id);
    EXPECT_EQ(a->contents_changed_since_last_frame,
              b->contents_changed_since_last_frame);
    EXPECT_EQ(a->mask_uv_rect.ToString(), b->mask_uv_rect.ToString());
    EXPECT_EQ(a->filters, b->filters);
    EXPECT_EQ(a->filter, b->filter);
    EXPECT_EQ(a->background_filters, b->background_filters);
  }

  void Compare(const SolidColorDrawQuad* a, const SolidColorDrawQuad* b) {
    EXPECT_EQ(a->color, b->color);
  }

  void Compare(const StreamVideoDrawQuad* a, const StreamVideoDrawQuad* b) {
    EXPECT_EQ(a->texture_id, b->texture_id);
    EXPECT_EQ(a->matrix, b->matrix);
  }

  void Compare(const TextureDrawQuad* a, const TextureDrawQuad* b) {
    EXPECT_EQ(a->resource_id, b->resource_id);
    EXPECT_EQ(a->premultiplied_alpha, b->premultiplied_alpha);
    EXPECT_EQ(a->uv_top_left, b->uv_top_left);
    EXPECT_EQ(a->uv_bottom_right, b->uv_bottom_right);
    EXPECT_EQ(a->vertex_opacity[0], b->vertex_opacity[0]);
    EXPECT_EQ(a->vertex_opacity[1], b->vertex_opacity[1]);
    EXPECT_EQ(a->vertex_opacity[2], b->vertex_opacity[2]);
    EXPECT_EQ(a->vertex_opacity[3], b->vertex_opacity[3]);
    EXPECT_EQ(a->flipped, b->flipped);
  }

  void Compare(const TileDrawQuad* a, const TileDrawQuad* b) {
    EXPECT_EQ(a->resource_id, b->resource_id);
    EXPECT_EQ(a->tex_coord_rect, b->tex_coord_rect);
    EXPECT_EQ(a->texture_size, b->texture_size);
    EXPECT_EQ(a->swizzle_contents, b->swizzle_contents);
  }

  void Compare(const YUVVideoDrawQuad* a, const YUVVideoDrawQuad* b) {
    EXPECT_EQ(a->tex_scale, b->tex_scale);
    EXPECT_EQ(a->y_plane.resource_id, b->y_plane.resource_id);
    EXPECT_EQ(a->y_plane.size.ToString(), b->y_plane.size.ToString());
    EXPECT_EQ(a->y_plane.format, b->y_plane.format);
    EXPECT_EQ(a->u_plane.resource_id, b->u_plane.resource_id);
    EXPECT_EQ(a->u_plane.size.ToString(), b->u_plane.size.ToString());
    EXPECT_EQ(a->u_plane.format, b->u_plane.format);
    EXPECT_EQ(a->v_plane.resource_id, b->v_plane.resource_id);
    EXPECT_EQ(a->v_plane.size.ToString(), b->v_plane.size.ToString());
    EXPECT_EQ(a->v_plane.format, b->v_plane.format);
  }

  void Compare(const TransferableResource& a, const TransferableResource& b) {
    EXPECT_EQ(a.id, b.id);
    EXPECT_EQ(a.sync_point, b.sync_point);
    EXPECT_EQ(a.format, b.format);
    EXPECT_EQ(a.filter, b.filter);
    EXPECT_EQ(a.size.ToString(), b.size.ToString());
    for (size_t i = 0; i < arraysize(a.mailbox.name); ++i)
      EXPECT_EQ(a.mailbox.name[i], b.mailbox.name[i]);
  }
};

TEST_F(CCMessagesTest, AllQuads) {
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);

  Transform arbitrary_matrix;
  arbitrary_matrix.Scale(3, 3);
  arbitrary_matrix.Translate(-5, 20);
  arbitrary_matrix.Rotate(15);
  gfx::Rect arbitrary_rect1(-5, 9, 3, 15);
  gfx::Rect arbitrary_rect2(40, 23, 11, 7);
  gfx::Rect arbitrary_rect3(7, -53, 22, 19);
  gfx::Size arbitrary_size1(15, 19);
  gfx::Size arbitrary_size2(3, 99);
  gfx::Size arbitrary_size3(75, 1281);
  gfx::RectF arbitrary_rectf1(4.2f, -922.1f, 15.6f, 29.5f);
  gfx::SizeF arbitrary_sizef1(15.2f, 104.6f);
  gfx::PointF arbitrary_pointf1(31.4f, 15.9f);
  gfx::PointF arbitrary_pointf2(26.5f, -35.8f);
  float arbitrary_float1 = 0.7f;
  float arbitrary_float2 = 0.3f;
  float arbitrary_float3 = 0.9f;
  float arbitrary_float_array[4] = {3.5f, 6.2f, 9.3f, 12.3f};
  bool arbitrary_bool1 = true;
  bool arbitrary_bool2 = false;
  bool arbitrary_bool3 = true;
  int arbitrary_int = 5;
  SkColor arbitrary_color = SkColorSetARGB(25, 36, 47, 58);
  IOSurfaceDrawQuad::Orientation arbitrary_orientation =
      IOSurfaceDrawQuad::UNFLIPPED;
  RenderPass::Id arbitrary_id(10, 14);
  ResourceProvider::ResourceId arbitrary_resourceid = 55;

  VideoLayerImpl::FramePlane arbitrary_plane1;
  arbitrary_plane1.resource_id = arbitrary_resourceid;
  arbitrary_plane1.size = arbitrary_size1;
  arbitrary_plane1.format = arbitrary_int;

  VideoLayerImpl::FramePlane arbitrary_plane2;
  arbitrary_plane2.resource_id = arbitrary_resourceid;
  arbitrary_plane2.size = arbitrary_size2;
  arbitrary_plane2.format = arbitrary_int;

  VideoLayerImpl::FramePlane arbitrary_plane3;
  arbitrary_plane3.resource_id = arbitrary_resourceid;
  arbitrary_plane3.size = arbitrary_size3;
  arbitrary_plane3.format = arbitrary_int;

  WebFilterOperations arbitrary_filters1;
  arbitrary_filters1.append(WebFilterOperation::createGrayscaleFilter(
      arbitrary_float1));

  WebFilterOperations arbitrary_filters2;
  arbitrary_filters2.append(WebFilterOperation::createBrightnessFilter(
      arbitrary_float2));

  // TODO(danakj): filter is not serialized.
  skia::RefPtr<SkImageFilter> arbitrary_filter;

  scoped_ptr<SharedQuadState> shared_state1_in = SharedQuadState::Create();
  shared_state1_in->SetAll(arbitrary_matrix,
                           arbitrary_size1,
                           arbitrary_rect1,
                           arbitrary_rect2,
                           arbitrary_bool1,
                           arbitrary_float1);
  scoped_ptr<SharedQuadState> shared_state1_cmp = shared_state1_in->Copy();

  scoped_ptr<CheckerboardDrawQuad> checkerboard_in =
      CheckerboardDrawQuad::Create();
  checkerboard_in->SetAll(shared_state1_in.get(),
                          arbitrary_rect1,
                          arbitrary_rect2,
                          arbitrary_rect3,
                          arbitrary_bool1,
                          arbitrary_color);
  scoped_ptr<DrawQuad> checkerboard_cmp = checkerboard_in->Copy(
      checkerboard_in->shared_quad_state);

  scoped_ptr<DebugBorderDrawQuad> debugborder_in =
      DebugBorderDrawQuad::Create();
  debugborder_in->SetAll(shared_state1_in.get(),
                         arbitrary_rect3,
                         arbitrary_rect1,
                         arbitrary_rect2,
                         arbitrary_bool1,
                         arbitrary_color,
                         arbitrary_int);
  scoped_ptr<DrawQuad> debugborder_cmp = debugborder_in->Copy(
      debugborder_in->shared_quad_state);

  scoped_ptr<IOSurfaceDrawQuad> iosurface_in =
      IOSurfaceDrawQuad::Create();
  iosurface_in->SetAll(shared_state1_in.get(),
                       arbitrary_rect2,
                       arbitrary_rect3,
                       arbitrary_rect1,
                       arbitrary_bool1,
                       arbitrary_size1,
                       arbitrary_int,
                       arbitrary_orientation);
  scoped_ptr<DrawQuad> iosurface_cmp = iosurface_in->Copy(
      iosurface_in->shared_quad_state);

  scoped_ptr<RenderPassDrawQuad> renderpass_in =
      RenderPassDrawQuad::Create();
  renderpass_in->SetAll(shared_state1_in.get(),
                        arbitrary_rect1,
                        arbitrary_rect2,
                        arbitrary_rect3,
                        arbitrary_bool1,
                        arbitrary_id,
                        arbitrary_bool2,
                        arbitrary_resourceid,
                        arbitrary_rect1,
                        arbitrary_rectf1,
                        arbitrary_filters1,
                        arbitrary_filter, // TODO(piman): not serialized.
                        arbitrary_filters2);
  scoped_ptr<RenderPassDrawQuad> renderpass_cmp = renderpass_in->Copy(
      renderpass_in->shared_quad_state, renderpass_in->render_pass_id);

  scoped_ptr<SharedQuadState> shared_state2_in = SharedQuadState::Create();
  shared_state2_in->SetAll(arbitrary_matrix,
                           arbitrary_size2,
                           arbitrary_rect2,
                           arbitrary_rect3,
                           arbitrary_bool1,
                           arbitrary_float2);
  scoped_ptr<SharedQuadState> shared_state2_cmp = shared_state2_in->Copy();

  scoped_ptr<SharedQuadState> shared_state3_in = SharedQuadState::Create();
  shared_state3_in->SetAll(arbitrary_matrix,
                           arbitrary_size3,
                           arbitrary_rect3,
                           arbitrary_rect1,
                           arbitrary_bool1,
                           arbitrary_float3);
  scoped_ptr<SharedQuadState> shared_state3_cmp = shared_state3_in->Copy();

  scoped_ptr<SolidColorDrawQuad> solidcolor_in =
      SolidColorDrawQuad::Create();
  solidcolor_in->SetAll(shared_state1_in.get(),
                        arbitrary_rect3,
                        arbitrary_rect1,
                        arbitrary_rect2,
                        arbitrary_bool1,
                        arbitrary_color);
  scoped_ptr<DrawQuad> solidcolor_cmp = solidcolor_in->Copy(
      solidcolor_in->shared_quad_state);

  scoped_ptr<StreamVideoDrawQuad> streamvideo_in =
      StreamVideoDrawQuad::Create();
  streamvideo_in->SetAll(shared_state1_in.get(),
                         arbitrary_rect2,
                         arbitrary_rect3,
                         arbitrary_rect1,
                         arbitrary_bool1,
                         arbitrary_int,
                         arbitrary_matrix);
  scoped_ptr<DrawQuad> streamvideo_cmp = streamvideo_in->Copy(
      streamvideo_in->shared_quad_state);

  scoped_ptr<TextureDrawQuad> texture_in = TextureDrawQuad::Create();
  texture_in->SetAll(shared_state1_in.get(),
                     arbitrary_rect2,
                     arbitrary_rect3,
                     arbitrary_rect1,
                     arbitrary_bool1,
                     arbitrary_resourceid,
                     arbitrary_bool2,
                     arbitrary_pointf1,
                     arbitrary_pointf2,
                     arbitrary_float_array,
                     arbitrary_bool3);
  scoped_ptr<DrawQuad> texture_cmp = texture_in->Copy(
      texture_in->shared_quad_state);

  scoped_ptr<TileDrawQuad> tile_in = TileDrawQuad::Create();
  tile_in->SetAll(shared_state1_in.get(),
                  arbitrary_rect2,
                  arbitrary_rect3,
                  arbitrary_rect1,
                  arbitrary_bool1,
                  arbitrary_resourceid,
                  arbitrary_rectf1,
                  arbitrary_size1,
                  arbitrary_bool2);
  scoped_ptr<DrawQuad> tile_cmp = tile_in->Copy(
      tile_in->shared_quad_state);

  scoped_ptr<YUVVideoDrawQuad> yuvvideo_in =
      YUVVideoDrawQuad::Create();
  yuvvideo_in->SetAll(shared_state1_in.get(),
                      arbitrary_rect1,
                      arbitrary_rect2,
                      arbitrary_rect3,
                      arbitrary_bool1,
                      arbitrary_sizef1,
                      arbitrary_plane1,
                      arbitrary_plane2,
                      arbitrary_plane3);
  scoped_ptr<DrawQuad> yuvvideo_cmp = yuvvideo_in->Copy(
      yuvvideo_in->shared_quad_state);

  scoped_ptr<RenderPass> pass_in = RenderPass::Create();
  pass_in->SetAll(arbitrary_id,
                  arbitrary_rect1,
                  arbitrary_rectf1,
                  arbitrary_matrix,
                  arbitrary_bool1,
                  arbitrary_bool2);

  pass_in->shared_quad_state_list.push_back(shared_state1_in.Pass());
  pass_in->quad_list.push_back(checkerboard_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(debugborder_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(iosurface_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(renderpass_in.PassAs<DrawQuad>());
  pass_in->shared_quad_state_list.push_back(shared_state2_in.Pass());
  pass_in->shared_quad_state_list.push_back(shared_state3_in.Pass());
  pass_in->quad_list.push_back(solidcolor_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(streamvideo_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(texture_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(tile_in.PassAs<DrawQuad>());
  pass_in->quad_list.push_back(yuvvideo_in.PassAs<DrawQuad>());

  scoped_ptr<RenderPass> pass_cmp = RenderPass::Create();
  pass_cmp->SetAll(arbitrary_id,
                   arbitrary_rect1,
                   arbitrary_rectf1,
                   arbitrary_matrix,
                   arbitrary_bool1,
                   arbitrary_bool2);

  pass_cmp->shared_quad_state_list.push_back(shared_state1_cmp.Pass());
  pass_cmp->quad_list.push_back(checkerboard_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(debugborder_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(iosurface_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(renderpass_cmp.PassAs<DrawQuad>());
  pass_cmp->shared_quad_state_list.push_back(shared_state2_cmp.Pass());
  pass_cmp->shared_quad_state_list.push_back(shared_state3_cmp.Pass());
  pass_cmp->quad_list.push_back(solidcolor_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(streamvideo_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(texture_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(tile_cmp.PassAs<DrawQuad>());
  pass_cmp->quad_list.push_back(yuvvideo_cmp.PassAs<DrawQuad>());

  // Make sure the in and cmp RenderPasses match.
  Compare(pass_cmp.get(), pass_in.get());
  ASSERT_EQ(3u, pass_in->shared_quad_state_list.size());
  ASSERT_EQ(9u, pass_in->quad_list.size());
  for (size_t i = 0; i < 3; ++i) {
    Compare(pass_cmp->shared_quad_state_list[i],
            pass_in->shared_quad_state_list[i]);
  }
  for (size_t i = 0; i < pass_in->quad_list.size(); ++i)
    Compare(pass_cmp->quad_list[i], pass_in->quad_list[i]);
  for (size_t i = 1; i < pass_in->quad_list.size(); ++i) {
    bool same_shared_quad_state_cmp =
        pass_cmp->quad_list[i]->shared_quad_state ==
        pass_cmp->quad_list[i - 1]->shared_quad_state;
    bool same_shared_quad_state_in =
        pass_in->quad_list[i]->shared_quad_state ==
        pass_in->quad_list[i - 1]->shared_quad_state;
    EXPECT_EQ(same_shared_quad_state_cmp, same_shared_quad_state_in);
  }

  DelegatedFrameData frame_in;
  frame_in.render_pass_list.push_back(pass_in.Pass());

  IPC::ParamTraits<DelegatedFrameData>::Write(&msg, frame_in);

  DelegatedFrameData frame_out;
  PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<DelegatedFrameData>::Read(&msg,
      &iter, &frame_out));

  // Make sure the out and cmp RenderPasses match.
  scoped_ptr<RenderPass> pass_out = frame_out.render_pass_list.take(
      frame_out.render_pass_list.begin());
  Compare(pass_cmp.get(), pass_out.get());
  ASSERT_EQ(3u, pass_out->shared_quad_state_list.size());
  ASSERT_EQ(9u, pass_out->quad_list.size());
  for (size_t i = 0; i < 3; ++i) {
    Compare(pass_cmp->shared_quad_state_list[i],
            pass_out->shared_quad_state_list[i]);
  }
  for (size_t i = 0; i < pass_out->quad_list.size(); ++i)
    Compare(pass_cmp->quad_list[i], pass_out->quad_list[i]);
  for (size_t i = 1; i < pass_out->quad_list.size(); ++i) {
    bool same_shared_quad_state_cmp =
        pass_cmp->quad_list[i]->shared_quad_state ==
        pass_cmp->quad_list[i - 1]->shared_quad_state;
    bool same_shared_quad_state_out =
        pass_out->quad_list[i]->shared_quad_state ==
        pass_out->quad_list[i - 1]->shared_quad_state;
    EXPECT_EQ(same_shared_quad_state_cmp, same_shared_quad_state_out);
  }
}

TEST_F(CCMessagesTest, Resources) {
  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  gfx::Size arbitrary_size(757, 1281);
  unsigned int arbitrary_uint1 = 71234838;
  unsigned int arbitrary_uint2 = 53589793;

  GLbyte arbitrary_mailbox1[64] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
    1, 2, 3, 4
  };

  GLbyte arbitrary_mailbox2[64] = {
    0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0,
    0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0,
    0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0,
    0, 9, 8, 7
  };

  TransferableResource arbitrary_resource1;
  arbitrary_resource1.id = 2178312;
  arbitrary_resource1.sync_point = arbitrary_uint1;
  arbitrary_resource1.format = 7;
  arbitrary_resource1.filter = 53;
  arbitrary_resource1.size = gfx::Size(37189, 123123);
  arbitrary_resource1.mailbox.SetName(arbitrary_mailbox1);

  TransferableResource arbitrary_resource2;
  arbitrary_resource2.id = 789132;
  arbitrary_resource1.sync_point = arbitrary_uint2;
  arbitrary_resource2.format = 30;
  arbitrary_resource1.filter = 47;
  arbitrary_resource2.size = gfx::Size(89123, 23789);
  arbitrary_resource2.mailbox.SetName(arbitrary_mailbox2);

  DelegatedFrameData frame_in;
  frame_in.resource_list.push_back(arbitrary_resource1);
  frame_in.resource_list.push_back(arbitrary_resource2);

  IPC::ParamTraits<DelegatedFrameData>::Write(&msg, frame_in);

  DelegatedFrameData frame_out;
  PickleIterator iter(msg);
  EXPECT_TRUE(IPC::ParamTraits<DelegatedFrameData>::Read(&msg,
      &iter, &frame_out));

  ASSERT_EQ(2u, frame_out.resource_list.size());
  Compare(arbitrary_resource1, frame_out.resource_list[0]);
  Compare(arbitrary_resource2, frame_out.resource_list[1]);
}

}  // namespace
}  // namespace content
