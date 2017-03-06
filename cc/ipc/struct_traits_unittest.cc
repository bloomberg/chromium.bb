// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "cc/input/selection.h"
#include "cc/ipc/copy_output_request_struct_traits.h"
#include "cc/ipc/traits_test_service.mojom.h"
#include "cc/output/copy_output_result.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
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

  void EchoCompositorFrame(
      CompositorFrame c,
      const EchoCompositorFrameCallback& callback) override {
    callback.Run(std::move(c));
  }

  void EchoCompositorFrameMetadata(
      CompositorFrameMetadata c,
      const EchoCompositorFrameMetadataCallback& callback) override {
    callback.Run(std::move(c));
  }

  void EchoCopyOutputRequest(
      std::unique_ptr<CopyOutputRequest> c,
      const EchoCopyOutputRequestCallback& callback) override {
    callback.Run(std::move(c));
  }

  void EchoCopyOutputResult(
      std::unique_ptr<CopyOutputResult> c,
      const EchoCopyOutputResultCallback& callback) override {
    callback.Run(std::move(c));
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

  void EchoRenderPass(std::unique_ptr<RenderPass> r,
                      const EchoRenderPassCallback& callback) override {
    callback.Run(std::move(r));
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

  void EchoSurfaceReference(
      const SurfaceReference& s,
      const EchoSurfaceReferenceCallback& callback) override {
    callback.Run(s);
  }

  void EchoSurfaceSequence(
      const SurfaceSequence& s,
      const EchoSurfaceSequenceCallback& callback) override {
    callback.Run(s);
  }

  void EchoTextureMailbox(const TextureMailbox& t,
                          const EchoTextureMailboxCallback& callback) override {
    callback.Run(t);
  }

  void EchoTransferableResource(
      const TransferableResource& t,
      const EchoTransferableResourceCallback& callback) override {
    callback.Run(t);
  }

  mojo::BindingSet<TraitsTestService> traits_test_bindings_;
  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

void CopyOutputRequestCallback(base::Closure quit_closure,
                               gfx::Size expected_size,
                               std::unique_ptr<CopyOutputResult> result) {
  EXPECT_EQ(expected_size, result->size());
  quit_closure.Run();
}

void CopyOutputRequestCallbackRunsOnceCallback(
    int* n_called,
    std::unique_ptr<CopyOutputResult> result) {
  ++*n_called;
}

void CopyOutputRequestMessagePipeBrokenCallback(
    base::Closure closure,
    std::unique_ptr<CopyOutputResult> result) {
  EXPECT_TRUE(result->IsEmpty());
  closure.Run();
}

void CopyOutputResultCallback(base::Closure quit_closure,
                              const gpu::SyncToken& expected_sync_token,
                              bool expected_is_lost,
                              const gpu::SyncToken& sync_token,
                              bool is_lost) {
  EXPECT_EQ(expected_sync_token, sync_token);
  EXPECT_EQ(expected_is_lost, is_lost);
  quit_closure.Run();
}

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

// Note that this is a fairly trivial test of CompositorFrame serialization as
// most of the heavy lifting has already been done by CompositorFrameMetadata,
// RenderPass, and QuadListBasic unit tests.
TEST_F(StructTraitsTest, CompositorFrame) {
  std::unique_ptr<RenderPass> render_pass = RenderPass::Create();
  render_pass->SetNew(1, gfx::Rect(5, 6), gfx::Rect(2, 3), gfx::Transform());

  // SharedQuadState.
  const gfx::Transform sqs_quad_to_target_transform(
      1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f, 12.f, 13.f, 14.f,
      15.f, 16.f);
  const gfx::Size sqs_layer_bounds(1234, 5678);
  const gfx::Rect sqs_visible_layer_rect(12, 34, 56, 78);
  const gfx::Rect sqs_clip_rect(123, 456, 789, 101112);
  const bool sqs_is_clipped = true;
  const float sqs_opacity = 0.9f;
  const SkBlendMode sqs_blend_mode = SkBlendMode::kSrcOver;
  const int sqs_sorting_context_id = 1337;
  SharedQuadState* sqs = render_pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(sqs_quad_to_target_transform, sqs_layer_bounds,
              sqs_visible_layer_rect, sqs_clip_rect, sqs_is_clipped,
              sqs_opacity, sqs_blend_mode, sqs_sorting_context_id);

  // DebugBorderDrawQuad.
  const gfx::Rect rect1(1234, 4321, 1357, 7531);
  const SkColor color1 = SK_ColorRED;
  const int32_t width1 = 1337;
  DebugBorderDrawQuad* debug_quad =
      render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  debug_quad->SetNew(sqs, rect1, rect1, color1, width1);

  // SolidColorDrawQuad.
  const gfx::Rect rect2(2468, 8642, 4321, 1234);
  const uint32_t color2 = 0xffffffff;
  const bool force_anti_aliasing_off = true;
  SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_quad->SetNew(sqs, rect2, rect2, color2, force_anti_aliasing_off);

  // TransferableResource constants.
  const uint32_t tr_id = 1337;
  const ResourceFormat tr_format = ALPHA_8;
  const uint32_t tr_filter = 1234;
  const gfx::Size tr_size(1234, 5678);
  TransferableResource resource;
  resource.id = tr_id;
  resource.format = tr_format;
  resource.filter = tr_filter;
  resource.size = tr_size;

  // CompositorFrameMetadata constants.
  const float device_scale_factor = 2.6f;
  const gfx::Vector2dF root_scroll_offset(1234.5f, 6789.1f);
  const float page_scale_factor = 1337.5f;
  const gfx::SizeF scrollable_viewport_size(1337.7f, 1234.5f);
  const uint32_t content_source_id = 3;

  CompositorFrame input;
  input.metadata.device_scale_factor = device_scale_factor;
  input.metadata.root_scroll_offset = root_scroll_offset;
  input.metadata.page_scale_factor = page_scale_factor;
  input.metadata.scrollable_viewport_size = scrollable_viewport_size;
  input.render_pass_list.push_back(std::move(render_pass));
  input.resource_list.push_back(resource);
  input.metadata.content_source_id = content_source_id;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  CompositorFrame output;
  proxy->EchoCompositorFrame(std::move(input), &output);

  EXPECT_EQ(device_scale_factor, output.metadata.device_scale_factor);
  EXPECT_EQ(root_scroll_offset, output.metadata.root_scroll_offset);
  EXPECT_EQ(page_scale_factor, output.metadata.page_scale_factor);
  EXPECT_EQ(scrollable_viewport_size, output.metadata.scrollable_viewport_size);
  EXPECT_EQ(content_source_id, output.metadata.content_source_id);

  ASSERT_EQ(1u, output.resource_list.size());
  TransferableResource out_resource = output.resource_list[0];
  EXPECT_EQ(tr_id, out_resource.id);
  EXPECT_EQ(tr_format, out_resource.format);
  EXPECT_EQ(tr_filter, out_resource.filter);
  EXPECT_EQ(tr_size, out_resource.size);

  EXPECT_EQ(1u, output.render_pass_list.size());
  const RenderPass* out_render_pass = output.render_pass_list[0].get();
  ASSERT_EQ(2u, out_render_pass->quad_list.size());
  ASSERT_EQ(1u, out_render_pass->shared_quad_state_list.size());

  const SharedQuadState* out_sqs =
      out_render_pass->shared_quad_state_list.ElementAt(0);
  EXPECT_EQ(sqs_quad_to_target_transform, out_sqs->quad_to_target_transform);
  EXPECT_EQ(sqs_layer_bounds, out_sqs->quad_layer_bounds);
  EXPECT_EQ(sqs_visible_layer_rect, out_sqs->visible_quad_layer_rect);
  EXPECT_EQ(sqs_clip_rect, out_sqs->clip_rect);
  EXPECT_EQ(sqs_is_clipped, out_sqs->is_clipped);
  EXPECT_EQ(sqs_opacity, out_sqs->opacity);
  EXPECT_EQ(sqs_blend_mode, out_sqs->blend_mode);
  EXPECT_EQ(sqs_sorting_context_id, out_sqs->sorting_context_id);

  const DebugBorderDrawQuad* out_debug_border_draw_quad =
      DebugBorderDrawQuad::MaterialCast(
          out_render_pass->quad_list.ElementAt(0));
  EXPECT_EQ(rect1, out_debug_border_draw_quad->rect);
  EXPECT_EQ(rect1, out_debug_border_draw_quad->visible_rect);
  EXPECT_EQ(color1, out_debug_border_draw_quad->color);
  EXPECT_EQ(width1, out_debug_border_draw_quad->width);

  const SolidColorDrawQuad* out_solid_color_draw_quad =
      SolidColorDrawQuad::MaterialCast(out_render_pass->quad_list.ElementAt(1));
  EXPECT_EQ(rect2, out_solid_color_draw_quad->rect);
  EXPECT_EQ(rect2, out_solid_color_draw_quad->visible_rect);
  EXPECT_EQ(color2, out_solid_color_draw_quad->color);
  EXPECT_EQ(force_anti_aliasing_off,
            out_solid_color_draw_quad->force_anti_aliasing_off);
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
  const bool may_contain_video = true;
  const bool is_resourceless_software_draw_with_scroll_or_animation = true;
  const float top_bar_height(1234.5f);
  const float top_bar_shown_ratio(1.0f);
  const float bottom_bar_height(1234.5f);
  const float bottom_bar_shown_ratio(1.0f);
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
  ui::LatencyInfo latency_info;
  latency_info.AddLatencyNumber(
      ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT, 1337, 7331);
  std::vector<ui::LatencyInfo> latency_infos = {latency_info};
  std::vector<SurfaceId> referenced_surfaces;
  SurfaceId id(FrameSinkId(1234, 4321),
               LocalSurfaceId(5678, base::UnguessableToken::Create()));
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
  input.may_contain_video = may_contain_video;
  input.is_resourceless_software_draw_with_scroll_or_animation =
      is_resourceless_software_draw_with_scroll_or_animation;
  input.top_controls_height = top_bar_height;
  input.top_controls_shown_ratio = top_bar_shown_ratio;
  input.bottom_controls_height = bottom_bar_height;
  input.bottom_controls_shown_ratio = bottom_bar_shown_ratio;
  input.root_background_color = root_background_color;
  input.selection = selection;
  input.latency_info = latency_infos;
  input.referenced_surfaces = referenced_surfaces;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  CompositorFrameMetadata output;
  proxy->EchoCompositorFrameMetadata(std::move(input), &output);
  EXPECT_EQ(device_scale_factor, output.device_scale_factor);
  EXPECT_EQ(root_scroll_offset, output.root_scroll_offset);
  EXPECT_EQ(page_scale_factor, output.page_scale_factor);
  EXPECT_EQ(scrollable_viewport_size, output.scrollable_viewport_size);
  EXPECT_EQ(root_layer_size, output.root_layer_size);
  EXPECT_EQ(min_page_scale_factor, output.min_page_scale_factor);
  EXPECT_EQ(max_page_scale_factor, output.max_page_scale_factor);
  EXPECT_EQ(root_overflow_x_hidden, output.root_overflow_x_hidden);
  EXPECT_EQ(root_overflow_y_hidden, output.root_overflow_y_hidden);
  EXPECT_EQ(may_contain_video, output.may_contain_video);
  EXPECT_EQ(is_resourceless_software_draw_with_scroll_or_animation,
            output.is_resourceless_software_draw_with_scroll_or_animation);
  EXPECT_EQ(top_bar_height, output.top_controls_height);
  EXPECT_EQ(top_bar_shown_ratio, output.top_controls_shown_ratio);
  EXPECT_EQ(bottom_bar_height, output.bottom_controls_height);
  EXPECT_EQ(bottom_bar_shown_ratio, output.bottom_controls_shown_ratio);
  EXPECT_EQ(root_background_color, output.root_background_color);
  EXPECT_EQ(selection, output.selection);
  EXPECT_EQ(latency_infos.size(), output.latency_info.size());
  ui::LatencyInfo::LatencyComponent component;
  EXPECT_TRUE(output.latency_info[0].FindLatency(
      ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT, 1337,
      &component));
  EXPECT_EQ(7331, component.sequence_number);
  EXPECT_EQ(referenced_surfaces.size(), output.referenced_surfaces.size());
  for (uint32_t i = 0; i < referenced_surfaces.size(); ++i)
    EXPECT_EQ(referenced_surfaces[i], output.referenced_surfaces[i]);
}

TEST_F(StructTraitsTest, CopyOutputRequest_BitmapRequest) {
  const gfx::Rect area(5, 7, 44, 55);
  const auto source =
      base::UnguessableToken::Deserialize(0xdeadbeef, 0xdeadf00d);
  gfx::Size size(9, 8);
  auto bitmap = base::MakeUnique<SkBitmap>();
  bitmap->allocN32Pixels(size.width(), size.height());
  base::RunLoop run_loop;
  auto callback =
      base::Bind(CopyOutputRequestCallback, run_loop.QuitClosure(), size);

  std::unique_ptr<CopyOutputRequest> input;
  input = CopyOutputRequest::CreateBitmapRequest(callback);
  input->set_area(area);
  input->set_source(source);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::unique_ptr<CopyOutputRequest> output;
  proxy->EchoCopyOutputRequest(std::move(input), &output);

  EXPECT_TRUE(output->force_bitmap_result());
  EXPECT_FALSE(output->has_texture_mailbox());
  EXPECT_TRUE(output->has_area());
  EXPECT_EQ(area, output->area());
  EXPECT_EQ(source, output->source());
  output->SendBitmapResult(std::move(bitmap));
  // If CopyOutputRequestCallback is called, this ends. Otherwise, the test
  // will time out and fail.
  run_loop.Run();
}

TEST_F(StructTraitsTest, CopyOutputRequest_TextureRequest) {
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 3};
  const uint32_t target = 3;
  gpu::Mailbox mailbox;
  mailbox.SetName(mailbox_name);
  TextureMailbox texture_mailbox(mailbox, gpu::SyncToken(), target);
  base::RunLoop run_loop;
  auto callback = base::Bind(CopyOutputRequestCallback, run_loop.QuitClosure(),
                             gfx::Size());

  auto input = CopyOutputRequest::CreateRequest(callback);
  input->SetTextureMailbox(texture_mailbox);

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::unique_ptr<CopyOutputRequest> output;
  proxy->EchoCopyOutputRequest(std::move(input), &output);

  EXPECT_TRUE(output->has_texture_mailbox());
  EXPECT_FALSE(output->has_area());
  EXPECT_EQ(mailbox, output->texture_mailbox().mailbox());
  EXPECT_EQ(target, output->texture_mailbox().target());
  EXPECT_FALSE(output->has_source());
  output->SendEmptyResult();
  // If CopyOutputRequestCallback is called, this ends. Otherwise, the test
  // will time out and fail.
  run_loop.Run();
}

TEST_F(StructTraitsTest, CopyOutputRequest_CallbackRunsOnce) {
  int n_called = 0;
  auto request = CopyOutputRequest::CreateRequest(
      base::Bind(CopyOutputRequestCallbackRunsOnceCallback, &n_called));
  auto result_sender = mojo::StructTraits<
      mojom::CopyOutputRequestDataView,
      std::unique_ptr<CopyOutputRequest>>::result_sender(request);
  for (int i = 0; i < 10; i++)
    result_sender->SendResult(CopyOutputResult::CreateEmptyResult());
  EXPECT_EQ(0, n_called);
  result_sender.FlushForTesting();
  EXPECT_EQ(1, n_called);
}

TEST_F(StructTraitsTest, CopyOutputRequest_MessagePipeBroken) {
  base::RunLoop run_loop;
  auto request = CopyOutputRequest::CreateRequest(base::Bind(
      CopyOutputRequestMessagePipeBrokenCallback, run_loop.QuitClosure()));
  auto result_sender = mojo::StructTraits<
      mojom::CopyOutputRequestDataView,
      std::unique_ptr<CopyOutputRequest>>::result_sender(request);
  result_sender.reset();
  // The callback must be called with an empty CopyOutputResult. If it's never
  // called, this will never end and the test times out.
  run_loop.Run();
}

TEST_F(StructTraitsTest, CopyOutputResult_Bitmap) {
  auto bitmap = base::MakeUnique<SkBitmap>();
  bitmap->allocN32Pixels(7, 8);
  bitmap->eraseARGB(123, 213, 77, 33);
  auto in_bitmap = base::MakeUnique<SkBitmap>();
  bitmap->deepCopyTo(in_bitmap.get());
  auto input = CopyOutputResult::CreateBitmapResult(std::move(bitmap));
  auto size = input->size();

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::unique_ptr<CopyOutputResult> output;
  proxy->EchoCopyOutputResult(std::move(input), &output);

  EXPECT_TRUE(output->HasBitmap());
  EXPECT_FALSE(output->HasTexture());
  EXPECT_EQ(size, output->size());

  std::unique_ptr<SkBitmap> out_bitmap = output->TakeBitmap();
  EXPECT_EQ(in_bitmap->getSize(), out_bitmap->getSize());
  EXPECT_EQ(0, std::memcmp(in_bitmap->getPixels(), out_bitmap->getPixels(),
                           in_bitmap->getSize()));
}

TEST_F(StructTraitsTest, CopyOutputResult_Texture) {
  const gfx::Size size(1234, 5678);
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 3};
  const uint32_t target = 3;
  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::GPU_IO, 0,
                            gpu::CommandBufferId::FromUnsafeValue(0x123),
                            71234838);
  bool is_lost = true;
  base::RunLoop run_loop;
  auto callback = SingleReleaseCallback::Create(base::Bind(
      CopyOutputResultCallback, run_loop.QuitClosure(), sync_token, is_lost));
  gpu::Mailbox mailbox;
  mailbox.SetName(mailbox_name);
  TextureMailbox texture_mailbox(mailbox, gpu::SyncToken(), target);

  auto input = CopyOutputResult::CreateTextureResult(size, texture_mailbox,
                                                     std::move(callback));

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::unique_ptr<CopyOutputResult> output;
  proxy->EchoCopyOutputResult(std::move(input), &output);

  EXPECT_FALSE(output->HasBitmap());
  EXPECT_TRUE(output->HasTexture());
  EXPECT_EQ(size, output->size());

  TextureMailbox out_mailbox;
  std::unique_ptr<SingleReleaseCallback> out_callback;
  output->TakeTexture(&out_mailbox, &out_callback);
  EXPECT_EQ(mailbox, out_mailbox.mailbox());
  out_callback->Run(sync_token, is_lost);
  // If CopyOutputResultCallback is called (which is the intended behaviour),
  // this will exit. Otherwise, this test will time out and fail.
  // In CopyOutputResultCallback we verify that the given sync_token and is_lost
  // have their intended values.
  run_loop.Run();
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
  std::unique_ptr<RenderPass> render_pass = RenderPass::Create();
  render_pass->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());

  SharedQuadState* sqs = render_pass->CreateAndAppendSharedQuadState();

  const gfx::Rect rect1(1234, 4321, 1357, 7531);
  const SkColor color1 = SK_ColorRED;
  const int32_t width1 = 1337;
  DebugBorderDrawQuad* debug_quad =
      render_pass->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  debug_quad->SetNew(sqs, rect1, rect1, color1, width1);

  const gfx::Rect rect2(2468, 8642, 4321, 1234);
  const uint32_t color2 = 0xffffffff;
  const bool force_anti_aliasing_off = true;
  SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_quad->SetNew(sqs, rect2, rect2, color2, force_anti_aliasing_off);

  const gfx::Rect rect3(1029, 3847, 5610, 2938);
  const SurfaceId primary_surface_id(
      FrameSinkId(1234, 4321),
      LocalSurfaceId(5678, base::UnguessableToken::Create()));
  const SurfaceId fallback_surface_id(
      FrameSinkId(2468, 1357),
      LocalSurfaceId(1234, base::UnguessableToken::Create()));
  SurfaceDrawQuad* primary_surface_quad =
      render_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  SurfaceDrawQuad* fallback_surface_quad =
      render_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  primary_surface_quad->SetNew(sqs, rect3, rect3, primary_surface_id,
                               SurfaceDrawQuadType::PRIMARY,
                               fallback_surface_quad);
  fallback_surface_quad->SetNew(sqs, rect3, rect3, fallback_surface_id,
                                SurfaceDrawQuadType::FALLBACK, nullptr);

  const gfx::Rect rect4(1234, 5678, 9101112, 13141516);
  const ResourceId resource_id4(1337);
  const int render_pass_id = 1234;
  const gfx::RectF mask_uv_rect(0, 0, 1337.1f, 1234.2f);
  const gfx::Size mask_texture_size(1234, 5678);
  gfx::Vector2dF filters_scale(1234.1f, 4321.2f);
  gfx::PointF filters_origin(8765.4f, 4567.8f);
  gfx::RectF tex_coord_rect(1.f, 1.f, 1234.f, 5678.f);

  RenderPassDrawQuad* render_pass_quad =
      render_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad->SetNew(sqs, rect4, rect4, render_pass_id, resource_id4,
                           mask_uv_rect, mask_texture_size, filters_scale,
                           filters_origin, tex_coord_rect);

  const gfx::Rect rect5(123, 567, 91011, 131415);
  const ResourceId resource_id5(1337);
  const float vertex_opacity[4] = {1.f, 2.f, 3.f, 4.f};
  const bool premultiplied_alpha = true;
  const gfx::PointF uv_top_left(12.1f, 34.2f);
  const gfx::PointF uv_bottom_right(56.3f, 78.4f);
  const SkColor background_color = SK_ColorGREEN;
  const bool y_flipped = true;
  const bool nearest_neighbor = true;
  const bool secure_output_only = true;
  const bool needs_blending = true;
  const gfx::Size resource_size_in_pixels5(1234, 5678);
  TextureDrawQuad* texture_draw_quad =
      render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  texture_draw_quad->SetAll(sqs, rect5, rect5, rect5, needs_blending,
                            resource_id5, resource_size_in_pixels5,
                            premultiplied_alpha, uv_top_left, uv_bottom_right,
                            background_color, vertex_opacity, y_flipped,
                            nearest_neighbor, secure_output_only);

  const gfx::Rect rect6(321, 765, 11109, 151413);
  const ResourceId resource_id6(1234);
  const gfx::Size resource_size_in_pixels(1234, 5678);
  const gfx::Transform matrix(16.1f, 15.3f, 14.3f, 13.7f, 12.2f, 11.4f, 10.4f,
                              9.8f, 8.1f, 7.3f, 6.3f, 5.7f, 4.8f, 3.4f, 2.4f,
                              1.2f);
  StreamVideoDrawQuad* stream_video_draw_quad =
      render_pass->CreateAndAppendDrawQuad<StreamVideoDrawQuad>();
  stream_video_draw_quad->SetNew(sqs, rect6, rect6, rect6, resource_id6,
                                 resource_size_in_pixels, matrix);

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::unique_ptr<RenderPass> output;
  proxy->EchoRenderPass(render_pass->DeepCopy(), &output);

  ASSERT_EQ(render_pass->quad_list.size(), output->quad_list.size());

  const DebugBorderDrawQuad* out_debug_border_draw_quad =
      DebugBorderDrawQuad::MaterialCast(output->quad_list.ElementAt(0));
  EXPECT_EQ(rect1, out_debug_border_draw_quad->rect);
  EXPECT_EQ(rect1, out_debug_border_draw_quad->visible_rect);
  EXPECT_EQ(color1, out_debug_border_draw_quad->color);
  EXPECT_EQ(width1, out_debug_border_draw_quad->width);

  const SolidColorDrawQuad* out_solid_color_draw_quad =
      SolidColorDrawQuad::MaterialCast(output->quad_list.ElementAt(1));
  EXPECT_EQ(rect2, out_solid_color_draw_quad->rect);
  EXPECT_EQ(rect2, out_solid_color_draw_quad->visible_rect);
  EXPECT_EQ(color2, out_solid_color_draw_quad->color);
  EXPECT_EQ(force_anti_aliasing_off,
            out_solid_color_draw_quad->force_anti_aliasing_off);

  const SurfaceDrawQuad* out_primary_surface_draw_quad =
      SurfaceDrawQuad::MaterialCast(output->quad_list.ElementAt(2));
  EXPECT_EQ(rect3, out_primary_surface_draw_quad->rect);
  EXPECT_EQ(rect3, out_primary_surface_draw_quad->visible_rect);
  EXPECT_EQ(primary_surface_id, out_primary_surface_draw_quad->surface_id);
  EXPECT_EQ(SurfaceDrawQuadType::PRIMARY,
            out_primary_surface_draw_quad->surface_draw_quad_type);

  const SurfaceDrawQuad* out_fallback_surface_draw_quad =
      SurfaceDrawQuad::MaterialCast(output->quad_list.ElementAt(3));
  EXPECT_EQ(out_fallback_surface_draw_quad,
            out_primary_surface_draw_quad->fallback_quad);
  EXPECT_EQ(rect3, out_fallback_surface_draw_quad->rect);
  EXPECT_EQ(rect3, out_fallback_surface_draw_quad->visible_rect);
  EXPECT_EQ(fallback_surface_id, out_fallback_surface_draw_quad->surface_id);
  EXPECT_EQ(SurfaceDrawQuadType::FALLBACK,
            out_fallback_surface_draw_quad->surface_draw_quad_type);
  EXPECT_FALSE(out_fallback_surface_draw_quad->fallback_quad);

  const RenderPassDrawQuad* out_render_pass_draw_quad =
      RenderPassDrawQuad::MaterialCast(output->quad_list.ElementAt(4));
  EXPECT_EQ(rect4, out_render_pass_draw_quad->rect);
  EXPECT_EQ(rect4, out_render_pass_draw_quad->visible_rect);
  EXPECT_EQ(render_pass_id, out_render_pass_draw_quad->render_pass_id);
  EXPECT_EQ(resource_id4, out_render_pass_draw_quad->mask_resource_id());
  EXPECT_EQ(mask_texture_size, out_render_pass_draw_quad->mask_texture_size);
  EXPECT_EQ(filters_scale, out_render_pass_draw_quad->filters_scale);

  const TextureDrawQuad* out_texture_draw_quad =
      TextureDrawQuad::MaterialCast(output->quad_list.ElementAt(5));
  EXPECT_EQ(rect5, out_texture_draw_quad->rect);
  EXPECT_EQ(rect5, out_texture_draw_quad->opaque_rect);
  EXPECT_EQ(rect5, out_texture_draw_quad->visible_rect);
  EXPECT_EQ(needs_blending, out_texture_draw_quad->needs_blending);
  EXPECT_EQ(resource_id5, out_texture_draw_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels5,
            out_texture_draw_quad->resource_size_in_pixels());
  EXPECT_EQ(premultiplied_alpha, out_texture_draw_quad->premultiplied_alpha);
  EXPECT_EQ(uv_top_left, out_texture_draw_quad->uv_top_left);
  EXPECT_EQ(uv_bottom_right, out_texture_draw_quad->uv_bottom_right);
  EXPECT_EQ(background_color, out_texture_draw_quad->background_color);
  EXPECT_EQ(vertex_opacity[0], out_texture_draw_quad->vertex_opacity[0]);
  EXPECT_EQ(vertex_opacity[1], out_texture_draw_quad->vertex_opacity[1]);
  EXPECT_EQ(vertex_opacity[2], out_texture_draw_quad->vertex_opacity[2]);
  EXPECT_EQ(vertex_opacity[3], out_texture_draw_quad->vertex_opacity[3]);
  EXPECT_EQ(y_flipped, out_texture_draw_quad->y_flipped);
  EXPECT_EQ(nearest_neighbor, out_texture_draw_quad->nearest_neighbor);
  EXPECT_EQ(secure_output_only, out_texture_draw_quad->secure_output_only);

  const StreamVideoDrawQuad* out_stream_video_draw_quad =
      StreamVideoDrawQuad::MaterialCast(output->quad_list.ElementAt(6));
  EXPECT_EQ(rect6, out_stream_video_draw_quad->rect);
  EXPECT_EQ(rect6, out_stream_video_draw_quad->opaque_rect);
  EXPECT_EQ(rect6, out_stream_video_draw_quad->visible_rect);
  EXPECT_EQ(resource_id6, out_stream_video_draw_quad->resource_id());
  EXPECT_EQ(resource_size_in_pixels,
            out_stream_video_draw_quad->resource_size_in_pixels());
  EXPECT_EQ(matrix, out_stream_video_draw_quad->matrix);
}

TEST_F(StructTraitsTest, RenderPass) {
  const int id = 3;
  const gfx::Rect output_rect(45, 22, 120, 13);
  const gfx::Transform transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  const gfx::Rect damage_rect(56, 123, 19, 43);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(0.f));
  filters.Append(FilterOperation::CreateZoomFilter(2.0f, 1));
  FilterOperations background_filters;
  background_filters.Append(FilterOperation::CreateSaturateFilter(4.f));
  background_filters.Append(FilterOperation::CreateZoomFilter(2.0f, 1));
  background_filters.Append(FilterOperation::CreateSaturateFilter(2.f));
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateXYZD50();
  const bool has_transparent_background = true;
  std::unique_ptr<RenderPass> input = RenderPass::Create();
  input->SetAll(id, output_rect, damage_rect, transform_to_root, filters,
                background_filters, color_space, has_transparent_background);

  SharedQuadState* shared_state_1 = input->CreateAndAppendSharedQuadState();
  shared_state_1->SetAll(
      gfx::Transform(16.1f, 15.3f, 14.3f, 13.7f, 12.2f, 11.4f, 10.4f, 9.8f,
                     8.1f, 7.3f, 6.3f, 5.7f, 4.8f, 3.4f, 2.4f, 1.2f),
      gfx::Size(1, 2), gfx::Rect(1337, 5679, 9101112, 131415),
      gfx::Rect(1357, 2468, 121314, 1337), true, 2, SkBlendMode::kSrcOver, 1);

  SharedQuadState* shared_state_2 = input->CreateAndAppendSharedQuadState();
  shared_state_2->SetAll(
      gfx::Transform(1.1f, 2.3f, 3.3f, 4.7f, 5.2f, 6.4f, 7.4f, 8.8f, 9.1f,
                     10.3f, 11.3f, 12.7f, 13.8f, 14.4f, 15.4f, 16.2f),
      gfx::Size(1337, 1234), gfx::Rect(1234, 5678, 9101112, 13141516),
      gfx::Rect(1357, 2468, 121314, 1337), true, 2, SkBlendMode::kSrcOver, 1);

  // This quad uses the first shared quad state. The next two quads use the
  // second shared quad state.
  DebugBorderDrawQuad* debug_quad =
      input->CreateAndAppendDrawQuad<DebugBorderDrawQuad>();
  const gfx::Rect debug_quad_rect(12, 56, 89, 10);
  debug_quad->SetNew(shared_state_1, debug_quad_rect, debug_quad_rect,
                     SK_ColorBLUE, 1337);

  SolidColorDrawQuad* color_quad =
      input->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  const gfx::Rect color_quad_rect(123, 456, 789, 101);
  color_quad->SetNew(shared_state_2, color_quad_rect, color_quad_rect,
                     SK_ColorRED, true);

  SurfaceDrawQuad* surface_quad =
      input->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  const gfx::Rect surface_quad_rect(1337, 2448, 1234, 5678);
  surface_quad->SetNew(
      shared_state_2, surface_quad_rect, surface_quad_rect,
      SurfaceId(FrameSinkId(1337, 1234),
                LocalSurfaceId(1234, base::UnguessableToken::Create())),
      SurfaceDrawQuadType::PRIMARY, nullptr);

  std::unique_ptr<RenderPass> output;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  proxy->EchoRenderPass(input->DeepCopy(), &output);

  EXPECT_EQ(input->quad_list.size(), output->quad_list.size());
  EXPECT_EQ(input->shared_quad_state_list.size(),
            output->shared_quad_state_list.size());
  EXPECT_EQ(id, output->id);
  EXPECT_EQ(output_rect, output->output_rect);
  EXPECT_EQ(damage_rect, output->damage_rect);
  EXPECT_EQ(transform_to_root, output->transform_to_root_target);
  EXPECT_EQ(color_space, output->color_space);
  EXPECT_EQ(has_transparent_background, output->has_transparent_background);
  EXPECT_EQ(filters, output->filters);
  EXPECT_EQ(background_filters, output->background_filters);

  SharedQuadState* out_sqs1 = output->shared_quad_state_list.ElementAt(0);
  EXPECT_EQ(shared_state_1->quad_to_target_transform,
            out_sqs1->quad_to_target_transform);
  EXPECT_EQ(shared_state_1->quad_layer_bounds, out_sqs1->quad_layer_bounds);
  EXPECT_EQ(shared_state_1->visible_quad_layer_rect,
            out_sqs1->visible_quad_layer_rect);
  EXPECT_EQ(shared_state_1->clip_rect, out_sqs1->clip_rect);
  EXPECT_EQ(shared_state_1->is_clipped, out_sqs1->is_clipped);
  EXPECT_EQ(shared_state_1->opacity, out_sqs1->opacity);
  EXPECT_EQ(shared_state_1->blend_mode, out_sqs1->blend_mode);
  EXPECT_EQ(shared_state_1->sorting_context_id, out_sqs1->sorting_context_id);

  SharedQuadState* out_sqs2 = output->shared_quad_state_list.ElementAt(1);
  EXPECT_EQ(shared_state_2->quad_to_target_transform,
            out_sqs2->quad_to_target_transform);
  EXPECT_EQ(shared_state_2->quad_layer_bounds, out_sqs2->quad_layer_bounds);
  EXPECT_EQ(shared_state_2->visible_quad_layer_rect,
            out_sqs2->visible_quad_layer_rect);
  EXPECT_EQ(shared_state_2->clip_rect, out_sqs2->clip_rect);
  EXPECT_EQ(shared_state_2->is_clipped, out_sqs2->is_clipped);
  EXPECT_EQ(shared_state_2->opacity, out_sqs2->opacity);
  EXPECT_EQ(shared_state_2->blend_mode, out_sqs2->blend_mode);
  EXPECT_EQ(shared_state_2->sorting_context_id, out_sqs2->sorting_context_id);

  const DebugBorderDrawQuad* out_debug_quad =
      DebugBorderDrawQuad::MaterialCast(output->quad_list.ElementAt(0));
  EXPECT_EQ(out_debug_quad->shared_quad_state, out_sqs1);
  EXPECT_EQ(debug_quad->rect, out_debug_quad->rect);
  EXPECT_EQ(debug_quad->visible_rect, out_debug_quad->visible_rect);
  EXPECT_EQ(debug_quad->color, out_debug_quad->color);
  EXPECT_EQ(debug_quad->width, out_debug_quad->width);

  const SolidColorDrawQuad* out_color_quad =
      SolidColorDrawQuad::MaterialCast(output->quad_list.ElementAt(1));
  EXPECT_EQ(out_color_quad->shared_quad_state, out_sqs2);
  EXPECT_EQ(color_quad->rect, out_color_quad->rect);
  EXPECT_EQ(color_quad->visible_rect, out_color_quad->visible_rect);
  EXPECT_EQ(color_quad->color, out_color_quad->color);
  EXPECT_EQ(color_quad->force_anti_aliasing_off,
            out_color_quad->force_anti_aliasing_off);

  const SurfaceDrawQuad* out_surface_quad =
      SurfaceDrawQuad::MaterialCast(output->quad_list.ElementAt(2));
  EXPECT_EQ(out_surface_quad->shared_quad_state, out_sqs2);
  EXPECT_EQ(surface_quad->rect, out_surface_quad->rect);
  EXPECT_EQ(surface_quad->visible_rect, out_surface_quad->visible_rect);
  EXPECT_EQ(surface_quad->surface_id, out_surface_quad->surface_id);
}

TEST_F(StructTraitsTest, RenderPassWithEmptySharedQuadStateList) {
  const int id = 3;
  const gfx::Rect output_rect(45, 22, 120, 13);
  const gfx::Transform transform_to_root =
      gfx::Transform(1.0, 0.5, 0.5, -0.5, -1.0, 0.0);
  const gfx::Rect damage_rect(56, 123, 19, 43);
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSCRGBLinear();
  const bool has_transparent_background = true;
  std::unique_ptr<RenderPass> input = RenderPass::Create();
  input->SetAll(id, output_rect, damage_rect, transform_to_root,
                FilterOperations(), FilterOperations(), color_space,
                has_transparent_background);

  // Unlike the previous test, don't add any quads to the list; we need to
  // verify that the serialization code can deal with that.
  std::unique_ptr<RenderPass> output;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  proxy->EchoRenderPass(input->DeepCopy(), &output);

  EXPECT_EQ(input->quad_list.size(), output->quad_list.size());
  EXPECT_EQ(input->shared_quad_state_list.size(),
            output->shared_quad_state_list.size());
  EXPECT_EQ(id, output->id);
  EXPECT_EQ(output_rect, output->output_rect);
  EXPECT_EQ(damage_rect, output->damage_rect);
  EXPECT_EQ(transform_to_root, output->transform_to_root_target);
  EXPECT_EQ(has_transparent_background, output->has_transparent_background);
  EXPECT_EQ(color_space, output->color_space);
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
  Selection<gfx::SelectionBound> input;
  input.start = start;
  input.end = end;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  Selection<gfx::SelectionBound> output;
  proxy->EchoSelection(input, &output);
  EXPECT_EQ(start, output.start);
  EXPECT_EQ(end, output.end);
}

TEST_F(StructTraitsTest, SurfaceId) {
  static constexpr FrameSinkId frame_sink_id(1337, 1234);
  static LocalSurfaceId local_surface_id(0xfbadbeef,
                                         base::UnguessableToken::Create());
  SurfaceId input(frame_sink_id, local_surface_id);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SurfaceId output;
  proxy->EchoSurfaceId(input, &output);
  EXPECT_EQ(frame_sink_id, output.frame_sink_id());
  EXPECT_EQ(local_surface_id, output.local_surface_id());
}

TEST_F(StructTraitsTest, SurfaceReference) {
  const SurfaceId parent_id(
      FrameSinkId(2016, 1234),
      LocalSurfaceId(0xfbadbeef,
                     base::UnguessableToken::Deserialize(123, 456)));
  const SurfaceId child_id(
      FrameSinkId(1111, 9999),
      LocalSurfaceId(0xabcdabcd,
                     base::UnguessableToken::Deserialize(333, 333)));
  const SurfaceReference input(parent_id, child_id);

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SurfaceReference output;
  proxy->EchoSurfaceReference(input, &output);
  EXPECT_EQ(parent_id, output.parent_id());
  EXPECT_EQ(child_id, output.child_id());
}

TEST_F(StructTraitsTest, SurfaceSequence) {
  const FrameSinkId frame_sink_id(2016, 1234);
  const uint32_t sequence = 0xfbadbeef;
  SurfaceSequence input(frame_sink_id, sequence);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  SurfaceSequence output;
  proxy->EchoSurfaceSequence(input, &output);
  EXPECT_EQ(frame_sink_id, output.frame_sink_id);
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
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
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

TEST_F(StructTraitsTest, TextureMailbox) {
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2};
  const gpu::CommandBufferNamespace command_buffer_namespace = gpu::IN_PROCESS;
  const int32_t extra_data_field = 0xbeefbeef;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdeadL;
  const gpu::SyncToken sync_token(command_buffer_namespace, extra_data_field,
                                  command_buffer_id, release_count);
  const uint32_t texture_target = 1337;
  const gfx::Size size_in_pixels(93, 24);
  const bool is_overlay_candidate = true;
  const bool secure_output_only = true;
  const bool nearest_neighbor = true;
  const gfx::ColorSpace color_space =
      gfx::ColorSpace::CreateVideo(4, 5, 9, gfx::ColorSpace::RangeID::LIMITED);
#if defined(OS_ANDROID)
  const bool is_backed_by_surface_texture = true;
  const bool wants_promotion_hint = true;
#endif

  gpu::Mailbox mailbox;
  mailbox.SetName(mailbox_name);
  TextureMailbox input(mailbox, sync_token, texture_target, size_in_pixels,
                       is_overlay_candidate, secure_output_only);
  input.set_nearest_neighbor(nearest_neighbor);
  input.set_color_space(color_space);
#if defined(OS_ANDROID)
  input.set_is_backed_by_surface_texture(is_backed_by_surface_texture);
  input.set_wants_promotion_hint(wants_promotion_hint);
#endif

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  TextureMailbox output;
  proxy->EchoTextureMailbox(input, &output);

  EXPECT_EQ(mailbox, output.mailbox());
  EXPECT_EQ(sync_token, output.sync_token());
  EXPECT_EQ(texture_target, output.target());
  EXPECT_EQ(size_in_pixels, output.size_in_pixels());
  EXPECT_EQ(is_overlay_candidate, output.is_overlay_candidate());
  EXPECT_EQ(secure_output_only, output.secure_output_only());
  EXPECT_EQ(nearest_neighbor, output.nearest_neighbor());
  EXPECT_EQ(color_space, output.color_space());
#if defined(OS_ANDROID)
  EXPECT_EQ(is_backed_by_surface_texture,
            output.is_backed_by_surface_texture());
  EXPECT_EQ(wants_promotion_hint, output.wants_promotion_hint());
#endif
}

TEST_F(StructTraitsTest, TransferableResource) {
  const uint32_t id = 1337;
  const ResourceFormat format = ALPHA_8;
  const uint32_t filter = 1234;
  const gfx::Size size(1234, 5678);
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2};
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
  std::unique_ptr<RenderPass> render_pass = RenderPass::Create();
  render_pass->SetNew(1, gfx::Rect(), gfx::Rect(), gfx::Transform());

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
  const gfx::ColorSpace video_color_space = gfx::ColorSpace::CreateJpeg();
  const float resource_offset = 1337.5f;
  const float resource_multiplier = 1234.6f;
  const uint32_t bits_per_channel = 13;

  SharedQuadState* sqs = render_pass->CreateAndAppendSharedQuadState();
  YUVVideoDrawQuad* quad =
      render_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
  quad->SetAll(sqs, rect, opaque_rect, visible_rect, needs_blending,
               ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size, uv_tex_size,
               y_plane_resource_id, u_plane_resource_id, v_plane_resource_id,
               a_plane_resource_id, color_space, video_color_space,
               resource_offset, resource_multiplier, bits_per_channel);

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::unique_ptr<RenderPass> output;
  proxy->EchoRenderPass(render_pass->DeepCopy(), &output);

  ASSERT_EQ(render_pass->quad_list.size(), output->quad_list.size());

  ASSERT_EQ(material, output->quad_list.ElementAt(0)->material);
  const YUVVideoDrawQuad* out_quad =
      YUVVideoDrawQuad::MaterialCast(output->quad_list.ElementAt(0));
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
  EXPECT_EQ(bits_per_channel, out_quad->bits_per_channel);
}

}  // namespace cc
