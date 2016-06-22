// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "cc/input/selection.h"
#include "cc/ipc/traits_test_service.mojom.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/render_pass_id.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"

namespace cc {

namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() {}

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // TraitsTestService:
  void EchoBeginFrameArgs(const BeginFrameArgs& b,
                          const EchoBeginFrameArgsCallback& callback) override {
    callback.Run(b);
  }

  void EchoCompositorFrameMetadata(
      const CompositorFrameMetadata& c,
      const EchoCompositorFrameMetadataCallback& callback) override {
    callback.Run(c);
  }

  void EchoFilterOperation(
      const FilterOperation& f,
      const EchoFilterOperationCallback& callback) override {
    callback.Run(f);
  }

  void EchoFilterOperations(
      const FilterOperations& f,
      const EchoFilterOperationsCallback& callback) override {
    callback.Run(f);
  }

  void EchoQuadList(const QuadList& q,
                    const EchoQuadListCallback& callback) override {
    callback.Run(q);
  }

  void EchoRenderPassId(const RenderPassId& r,
                        const EchoRenderPassIdCallback& callback) override {
    callback.Run(r);
  }

  void EchoReturnedResource(
      const ReturnedResource& r,
      const EchoReturnedResourceCallback& callback) override {
    callback.Run(r);
  }

  void EchoSelection(const Selection<gfx::SelectionBound>& s,
                     const EchoSelectionCallback& callback) override {
    callback.Run(s);
  }

  void EchoSharedQuadState(
      const SharedQuadState& s,
      const EchoSharedQuadStateCallback& callback) override {
    callback.Run(s);
  }

  void EchoSurfaceId(const SurfaceId& s,
                     const EchoSurfaceIdCallback& callback) override {
    callback.Run(s);
  }

  void EchoSurfaceSequence(
      const SurfaceSequence& s,
      const EchoSurfaceSequenceCallback& callback) override {
    callback.Run(s);
  }

  void EchoTransferableResource(
      const TransferableResource& t,
      const EchoTransferableResourceCallback& callback) override {
    callback.Run(t);
  }

  mojo::BindingSet<TraitsTestService> traits_test_bindings_;
  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

}  // namespace

TEST_F(StructTraitsTest, BeginFrameArgs) {
  const base::TimeTicks frame_time = base::TimeTicks::Now();
  const base::TimeTicks deadline = base::TimeTicks::Now();
  const base::TimeDelta interval = base::TimeDelta::FromMilliseconds(1337);
  const BeginFrameArgs::BeginFrameArgsType type = BeginFrameArgs::NORMAL;
  const bool on_critical_path = true;
  BeginFrameArgs input;
  input.frame_time = frame_time;
  input.deadline = deadline;
  input.interval = interval;
  input.type = type;
  input.on_critical_path = on_critical_path;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  BeginFrameArgs output;
  proxy->EchoBeginFrameArgs(input, &output);
  EXPECT_EQ(frame_time, output.frame_time);
  EXPECT_EQ(deadline, output.deadline);
  EXPECT_EQ(interval, output.interval);
  EXPECT_EQ(type, output.type);
  EXPECT_EQ(on_critical_path, output.on_critical_path);
}

TEST_F(StructTraitsTest, CompositorFrameMetadata) {
  const float device_scale_factor = 2.6f;
  const gfx::Vector2dF root_scroll_offset(1234.5f, 6789.1f);
  const float page_scale_factor = 1337.5f;
  const gfx::SizeF scrollable_viewport_size(1337.7f, 1234.5f);
  const gfx::SizeF root_layer_size(1234.5f, 5432.1f);
  const float min_page_scale_factor = 3.5f;
  const float max_page_scale_factor = 4.6f;
  const bool root_overflow_x_hidden = true;
  const bool root_overflow_y_hidden = true;
  const gfx::Vector2dF location_bar_offset(1234.5f, 5432.1f);
  const gfx::Vector2dF location_bar_content_translation(1234.5f, 5432.1f);
  const uint32_t root_background_color = 1337;
  Selection<gfx::SelectionBound> selection;
  selection.start.SetEdge(gfx::PointF(1234.5f, 67891.f),
                          gfx::PointF(5432.1f, 1987.6f));
  selection.start.set_visible(true);
  selection.start.set_type(gfx::SelectionBound::CENTER);
  selection.end.SetEdge(gfx::PointF(1337.5f, 52124.f),
                        gfx::PointF(1234.3f, 8765.6f));
  selection.end.set_visible(false);
  selection.end.set_type(gfx::SelectionBound::RIGHT);
  selection.is_editable = true;
  selection.is_empty_text_form_control = true;
  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(
      ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT, 1337, 7331);
  std::vector<ui::LatencyInfo> latency_infos = {latency_info};
  std::vector<uint32_t> satisfies_sequences = {1234, 1337};
  std::vector<SurfaceId> referenced_surfaces;
  SurfaceId id(1234, 5678, 9101112);
  referenced_surfaces.push_back(id);

  CompositorFrameMetadata input;
  input.device_scale_factor = device_scale_factor;
  input.root_scroll_offset = root_scroll_offset;
  input.page_scale_factor = page_scale_factor;
  input.scrollable_viewport_size = scrollable_viewport_size;
  input.root_layer_size = root_layer_size;
  input.min_page_scale_factor = min_page_scale_factor;
  input.max_page_scale_factor = max_page_scale_factor;
  input.root_overflow_x_hidden = root_overflow_x_hidden;
  input.root_overflow_y_hidden = root_overflow_y_hidden;
  input.location_bar_offset = location_bar_offset;
  input.location_bar_content_translation = location_bar_content_translation;
  input.root_background_color = root_background_color;
  input.selection = selection;
  input.latency_info = latency_infos;
  input.satisfies_sequences = satisfies_sequences;
  input.referenced_surfaces = referenced_surfaces;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  CompositorFrameMetadata output;
  proxy->EchoCompositorFrameMetadata(input, &output);
  EXPECT_EQ(device_scale_factor, output.device_scale_factor);
  EXPECT_EQ(root_scroll_offset, output.root_scroll_offset);
  EXPECT_EQ(page_scale_factor, output.page_scale_factor);
  EXPECT_EQ(scrollable_viewport_size, output.scrollable_viewport_size);
  EXPECT_EQ(root_layer_size, output.root_layer_size);
  EXPECT_EQ(min_page_scale_factor, output.min_page_scale_factor);
  EXPECT_EQ(max_page_scale_factor, output.max_page_scale_factor);
  EXPECT_EQ(root_overflow_x_hidden, output.root_overflow_x_hidden);
  EXPECT_EQ(root_overflow_y_hidden, output.root_overflow_y_hidden);
  EXPECT_EQ(location_bar_offset, output.location_bar_offset);
  EXPECT_EQ(location_bar_content_translation,
            output.location_bar_content_translation);
  EXPECT_EQ(root_background_color, output.root_background_color);
  EXPECT_EQ(selection, output.selection);
  EXPECT_EQ(latency_infos.size(), output.latency_info.size());
  ui::LatencyInfo::LatencyComponent component;
  EXPECT_TRUE(output.latency_info[0].FindLatency(
      ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT, 1337,
      &component));
  EXPECT_EQ(7331, component.sequence_number);
  EXPECT_EQ(satisfies_sequences.size(), output.satisfies_sequences.size());
  for (uint32_t i = 0; i < satisfies_sequences.size(); ++i)
    EXPECT_EQ(satisfies_sequences[i], output.satisfies_sequences[i]);
  EXPECT_EQ(referenced_surfaces.size(), output.referenced_surfaces.size());
  for (uint32_t i = 0; i < referenced_surfaces.size(); ++i)
    EXPECT_EQ(referenced_surfaces[i], output.referenced_surfaces[i]);
}

TEST_F(StructTraitsTest, FilterOperation) {
  const FilterOperation inputs[] = {
      FilterOperation::CreateBlurFilter(20),
      FilterOperation::CreateDropShadowFilter(gfx::Point(4, 4), 4.0f,
                                              SkColorSetARGB(255, 40, 0, 0)),
      FilterOperation::CreateReferenceFilter(SkDropShadowImageFilter::Make(
          SkIntToScalar(3), SkIntToScalar(8), SkIntToScalar(4),
          SkIntToScalar(9), SK_ColorBLACK,
          SkDropShadowImageFilter::kDrawShadowAndForeground_ShadowMode,
          nullptr))};
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  for (const auto& input : inputs) {
    FilterOperation output;
    proxy->EchoFilterOperation(input, &output);
    EXPECT_EQ(input.type(), output.type());
    switch (input.type()) {
      case FilterOperation::GRAYSCALE:
      case FilterOperation::SEPIA:
      case FilterOperation::SATURATE:
      case FilterOperation::HUE_ROTATE:
      case FilterOperation::INVERT:
      case FilterOperation::BRIGHTNESS:
      case FilterOperation::SATURATING_BRIGHTNESS:
      case FilterOperation::CONTRAST:
      case FilterOperation::OPACITY:
      case FilterOperation::BLUR:
        EXPECT_EQ(input.amount(), output.amount());
        break;
      case FilterOperation::DROP_SHADOW:
        EXPECT_EQ(input.amount(), output.amount());
        EXPECT_EQ(input.drop_shadow_offset(), output.drop_shadow_offset());
        EXPECT_EQ(input.drop_shadow_color(), output.drop_shadow_color());
        break;
      case FilterOperation::COLOR_MATRIX:
        EXPECT_EQ(0, memcmp(input.matrix(), output.matrix(), 20));
        break;
      case FilterOperation::ZOOM:
        EXPECT_EQ(input.amount(), output.amount());
        EXPECT_EQ(input.zoom_inset(), output.zoom_inset());
        break;
      case FilterOperation::REFERENCE: {
        SkString input_str;
        input.image_filter()->toString(&input_str);
        SkString output_str;
        output.image_filter()->toString(&output_str);
        EXPECT_EQ(input_str, output_str);
        break;
      }
      case FilterOperation::ALPHA_THRESHOLD:
        NOTREACHED();
        break;
    }
  }
}

TEST_F(StructTraitsTest, FilterOperations) {
  FilterOperations input;
  input.Append(FilterOperation::CreateBlurFilter(0.f));
  input.Append(FilterOperation::CreateSaturateFilter(4.f));
  input.Append(FilterOperation::CreateZoomFilter(2.0f, 1));
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  FilterOperations output;
  proxy->EchoFilterOperations(input, &output);
  EXPECT_EQ(input.size(), output.size());
  for (size_t i = 0; i < input.size(); ++i) {
    EXPECT_EQ(input.at(i), output.at(i));
  }
}

TEST_F(StructTraitsTest, QuadListBasic) {
  const DrawQuad::Material material1 = DrawQuad::DEBUG_BORDER;
  const DrawQuad::Material material2 = DrawQuad::SOLID_COLOR;
  const DrawQuad::Material material3 = DrawQuad::SURFACE_CONTENT;
  const DrawQuad::Material material4 = DrawQuad::RENDER_PASS;
  const gfx::Rect rect1(1234, 4321, 1357, 7531);
  const gfx::Rect rect2(2468, 8642, 4321, 1234);
  const gfx::Rect rect3(1029, 3847, 5610, 2938);
  const gfx::Rect rect4(1234, 5678, 9101112, 13141516);
  const int32_t width1 = 1337;
  const uint32_t color2 = 0xffffffff;
  const SurfaceId surface_id(1234, 5678, 2468);
  const RenderPassId render_pass_id(1234, 5678);

  QuadList input;
  DebugBorderDrawQuad* debug_quad =
      input.AllocateAndConstruct<DebugBorderDrawQuad>();
  debug_quad->material = material1;
  debug_quad->rect = rect1;
  debug_quad->width = width1;

  SolidColorDrawQuad* solid_quad =
      input.AllocateAndConstruct<SolidColorDrawQuad>();
  solid_quad->material = material2;
  solid_quad->rect = rect2;
  solid_quad->color = color2;

  SurfaceDrawQuad* surface_quad = input.AllocateAndConstruct<SurfaceDrawQuad>();
  surface_quad->material = material3;
  surface_quad->rect = rect3;
  surface_quad->surface_id = surface_id;

  RenderPassDrawQuad* render_pass_quad =
      input.AllocateAndConstruct<RenderPassDrawQuad>();
  render_pass_quad->material = material4;
  render_pass_quad->rect = rect4;
  render_pass_quad->render_pass_id = render_pass_id;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  QuadList output;
  proxy->EchoQuadList(input, &output);

  ASSERT_EQ(input.size(), output.size());

  ASSERT_EQ(material1, output.ElementAt(0)->material);
  EXPECT_EQ(rect1, output.ElementAt(0)->rect);
  EXPECT_EQ(width1,
            static_cast<DebugBorderDrawQuad*>(output.ElementAt(0))->width);

  ASSERT_EQ(material2, output.ElementAt(1)->material);
  EXPECT_EQ(rect2, output.ElementAt(1)->rect);
  EXPECT_EQ(color2,
            static_cast<SolidColorDrawQuad*>(output.ElementAt(1))->color);

  ASSERT_EQ(material3, output.ElementAt(2)->material);
  EXPECT_EQ(rect3, output.ElementAt(2)->rect);
  EXPECT_EQ(surface_id,
            static_cast<SurfaceDrawQuad*>(output.ElementAt(2))->surface_id);

  ASSERT_EQ(material4, output.ElementAt(3)->material);
  EXPECT_EQ(rect4, output.ElementAt(3)->rect);
  EXPECT_EQ(
      render_pass_id,
      static_cast<RenderPassDrawQuad*>(output.ElementAt(3))->render_pass_id);
}

TEST_F(StructTraitsTest, RenderPassId) {
  const int layer_id = 1337;
  const uint32_t index = 0xdeadbeef;
  RenderPassId input(layer_id, index);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  RenderPassId output;
  proxy->EchoRenderPassId(input, &output);
  EXPECT_EQ(layer_id, output.layer_id);
  EXPECT_EQ(index, output.index);
}

TEST_F(StructTraitsTest, ReturnedResource) {
  const uint32_t id = 1337;
  const gpu::CommandBufferNamespace command_buffer_namespace = gpu::IN_PROCESS;
  const int32_t extra_data_field = 0xbeefbeef;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdead;
  const gpu::SyncToken sync_token(command_buffer_namespace, extra_data_field,
                                  command_buffer_id, release_count);
  const int count = 1234;
  const bool lost = true;

  ReturnedResource input;
  input.id = id;
  input.sync_token = sync_token;
  input.count = count;
  input.lost = lost;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  ReturnedResource output;
  proxy->EchoReturnedResource(input, &output);
  EXPECT_EQ(id, output.id);
  EXPECT_EQ(sync_token, output.sync_token);
  EXPECT_EQ(count, output.count);
  EXPECT_EQ(lost, output.lost);
}

TEST_F(StructTraitsTest, Selection) {
  gfx::SelectionBound start;
  start.SetEdge(gfx::PointF(1234.5f, 67891.f), gfx::PointF(5432.1f, 1987.6f));
  start.set_visible(true);
  start.set_type(gfx::SelectionBound::CENTER);
  gfx::SelectionBound end;
  end.SetEdge(gfx::PointF(1337.5f, 52124.f), gfx::PointF(1234.3f, 8765.6f));
  end.set_visible(false);
  end.set_type(gfx::SelectionBound::RIGHT);
  const bool is_editable = true;
  const bool is_empty_text_form_control = true;
  Selection<gfx::SelectionBound> input;
  input.start = start;
  input.end = end;
  input.is_editable = is_editable;
  input.is_empty_text_form_control = is_empty_text_form_control;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  Selection<gfx::SelectionBound> output;
  proxy->EchoSelection(input, &output);
  EXPECT_EQ(start, output.start);
  EXPECT_EQ(end, output.end);
  EXPECT_EQ(is_editable, output.is_editable);
  EXPECT_EQ(is_empty_text_form_control, output.is_empty_text_form_control);
}

TEST_F(StructTraitsTest, SurfaceId) {
  const uint32_t id_namespace = 1337;
  const uint32_t local_id = 0xfbadbeef;
  const uint64_t nonce = 0xdeadbeef;
  SurfaceId input(id_namespace, local_id, nonce);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SurfaceId output;
  proxy->EchoSurfaceId(input, &output);
  EXPECT_EQ(id_namespace, output.id_namespace());
  EXPECT_EQ(local_id, output.local_id());
  EXPECT_EQ(nonce, output.nonce());
}

TEST_F(StructTraitsTest, SurfaceSequence) {
  const uint32_t id_namespace = 2016;
  const uint32_t sequence = 0xfbadbeef;
  SurfaceSequence input(id_namespace, sequence);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SurfaceSequence output;
  proxy->EchoSurfaceSequence(input, &output);
  EXPECT_EQ(id_namespace, output.id_namespace);
  EXPECT_EQ(sequence, output.sequence);
}

TEST_F(StructTraitsTest, SharedQuadState) {
  const gfx::Transform quad_to_target_transform(1.f, 2.f, 3.f, 4.f, 5.f, 6.f,
                                                7.f, 8.f, 9.f, 10.f, 11.f, 12.f,
                                                13.f, 14.f, 15.f, 16.f);
  const gfx::Size layer_bounds(1234, 5678);
  const gfx::Rect visible_layer_rect(12, 34, 56, 78);
  const gfx::Rect clip_rect(123, 456, 789, 101112);
  const bool is_clipped = true;
  const float opacity = 0.9f;
  const SkXfermode::Mode blend_mode = SkXfermode::kSrcOver_Mode;
  const int sorting_context_id = 1337;
  SharedQuadState input_sqs;
  input_sqs.SetAll(quad_to_target_transform, layer_bounds, visible_layer_rect,
                   clip_rect, is_clipped, opacity, blend_mode,
                   sorting_context_id);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SharedQuadState output_sqs;
  proxy->EchoSharedQuadState(input_sqs, &output_sqs);
  EXPECT_EQ(quad_to_target_transform, output_sqs.quad_to_target_transform);
  EXPECT_EQ(layer_bounds, output_sqs.quad_layer_bounds);
  EXPECT_EQ(visible_layer_rect, output_sqs.visible_quad_layer_rect);
  EXPECT_EQ(clip_rect, output_sqs.clip_rect);
  EXPECT_EQ(is_clipped, output_sqs.is_clipped);
  EXPECT_EQ(opacity, output_sqs.opacity);
  EXPECT_EQ(blend_mode, output_sqs.blend_mode);
  EXPECT_EQ(sorting_context_id, output_sqs.sorting_context_id);
}

TEST_F(StructTraitsTest, TransferableResource) {
  const uint32_t id = 1337;
  const ResourceFormat format = ALPHA_8;
  const uint32_t filter = 1234;
  const gfx::Size size(1234, 5678);
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9,
      8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7,
      6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7};
  const gpu::CommandBufferNamespace command_buffer_namespace = gpu::IN_PROCESS;
  const int32_t extra_data_field = 0xbeefbeef;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdeadL;
  const uint32_t texture_target = 1337;
  const bool read_lock_fences_enabled = true;
  const bool is_software = false;
  const bool is_overlay_candidate = true;

  gpu::MailboxHolder mailbox_holder;
  mailbox_holder.mailbox.SetName(mailbox_name);
  mailbox_holder.sync_token =
      gpu::SyncToken(command_buffer_namespace, extra_data_field,
                     command_buffer_id, release_count);
  mailbox_holder.texture_target = texture_target;
  TransferableResource input;
  input.id = id;
  input.format = format;
  input.filter = filter;
  input.size = size;
  input.mailbox_holder = mailbox_holder;
  input.read_lock_fences_enabled = read_lock_fences_enabled;
  input.is_software = is_software;
  input.is_overlay_candidate = is_overlay_candidate;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  TransferableResource output;
  proxy->EchoTransferableResource(input, &output);
  EXPECT_EQ(id, output.id);
  EXPECT_EQ(format, output.format);
  EXPECT_EQ(filter, output.filter);
  EXPECT_EQ(size, output.size);
  EXPECT_EQ(mailbox_holder.mailbox, output.mailbox_holder.mailbox);
  EXPECT_EQ(mailbox_holder.sync_token, output.mailbox_holder.sync_token);
  EXPECT_EQ(mailbox_holder.texture_target,
            output.mailbox_holder.texture_target);
  EXPECT_EQ(read_lock_fences_enabled, output.read_lock_fences_enabled);
  EXPECT_EQ(is_software, output.is_software);
  EXPECT_EQ(is_overlay_candidate, output.is_overlay_candidate);
}

TEST_F(StructTraitsTest, YUVDrawQuad) {
  const DrawQuad::Material material = DrawQuad::YUV_VIDEO_CONTENT;
  const gfx::Rect rect(1234, 4321, 1357, 7531);
  const gfx::Rect opaque_rect(1357, 8642, 432, 123);
  const gfx::Rect visible_rect(1337, 7331, 561, 293);
  const bool needs_blending = true;
  const gfx::RectF ya_tex_coord_rect(1234.1f, 5678.2f, 9101112.3f, 13141516.4f);
  const gfx::RectF uv_tex_coord_rect(1234.1f, 4321.2f, 1357.3f, 7531.4f);
  const gfx::Size ya_tex_size(1234, 5678);
  const gfx::Size uv_tex_size(4321, 8765);
  const uint32_t y_plane_resource_id = 1337;
  const uint32_t u_plane_resource_id = 1234;
  const uint32_t v_plane_resource_id = 2468;
  const uint32_t a_plane_resource_id = 7890;
  const YUVVideoDrawQuad::ColorSpace color_space = YUVVideoDrawQuad::JPEG;
  const float resource_offset = 1337.5f;
  const float resource_multiplier = 1234.6f;

  SharedQuadState sqs;
  QuadList input;
  YUVVideoDrawQuad* quad = input.AllocateAndConstruct<YUVVideoDrawQuad>();
  quad->SetAll(&sqs, rect, opaque_rect, visible_rect, needs_blending,
               ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size, uv_tex_size,
               y_plane_resource_id, u_plane_resource_id, v_plane_resource_id,
               a_plane_resource_id, color_space, resource_offset,
               resource_multiplier);

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  QuadList output;
  proxy->EchoQuadList(input, &output);

  ASSERT_EQ(input.size(), output.size());

  ASSERT_EQ(material, output.ElementAt(0)->material);
  const YUVVideoDrawQuad* out_quad =
      YUVVideoDrawQuad::MaterialCast(output.ElementAt(0));
  EXPECT_EQ(rect, out_quad->rect);
  EXPECT_EQ(opaque_rect, out_quad->opaque_rect);
  EXPECT_EQ(visible_rect, out_quad->visible_rect);
  EXPECT_EQ(needs_blending, out_quad->needs_blending);
  EXPECT_EQ(ya_tex_coord_rect, out_quad->ya_tex_coord_rect);
  EXPECT_EQ(uv_tex_coord_rect, out_quad->uv_tex_coord_rect);
  EXPECT_EQ(ya_tex_size, out_quad->ya_tex_size);
  EXPECT_EQ(uv_tex_size, out_quad->uv_tex_size);
  EXPECT_EQ(y_plane_resource_id, out_quad->y_plane_resource_id());
  EXPECT_EQ(u_plane_resource_id, out_quad->u_plane_resource_id());
  EXPECT_EQ(v_plane_resource_id, out_quad->v_plane_resource_id());
  EXPECT_EQ(a_plane_resource_id, out_quad->a_plane_resource_id());
  EXPECT_EQ(color_space, out_quad->color_space);
  EXPECT_EQ(resource_offset, out_quad->resource_offset);
  EXPECT_EQ(resource_multiplier, out_quad->resource_multiplier);
}

}  // namespace cc
