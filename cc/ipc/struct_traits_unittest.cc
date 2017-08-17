// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "cc/input/selection.h"
#include "cc/ipc/copy_output_request_struct_traits.h"
#include "cc/ipc/traits_test_service.mojom.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/render_pass_draw_quad.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/surface_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "components/viz/common/quads/copy_output_result.h"
#include "components/viz/test/begin_frame_args_test.h"
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
    mojom::TraitsTestServicePtr proxy;
    traits_test_bindings_.AddBinding(this, mojo::MakeRequest(&proxy));
    return proxy;
  }

 private:
  // TraitsTestService:
  void EchoCopyOutputRequest(std::unique_ptr<viz::CopyOutputRequest> c,
                             EchoCopyOutputRequestCallback callback) override {
    std::move(callback).Run(std::move(c));
  }

  void EchoCopyOutputResult(std::unique_ptr<viz::CopyOutputResult> c,
                            EchoCopyOutputResultCallback callback) override {
    std::move(callback).Run(std::move(c));
  }

  void EchoFilterOperation(const FilterOperation& f,
                           EchoFilterOperationCallback callback) override {
    std::move(callback).Run(f);
  }

  void EchoFilterOperations(const FilterOperations& f,
                            EchoFilterOperationsCallback callback) override {
    std::move(callback).Run(f);
  }

  void EchoSurfaceId(const viz::SurfaceId& s,
                     EchoSurfaceIdCallback callback) override {
    std::move(callback).Run(s);
  }

  void EchoTextureMailbox(const viz::TextureMailbox& t,
                          EchoTextureMailboxCallback callback) override {
    std::move(callback).Run(t);
  }

  mojo::BindingSet<TraitsTestService> traits_test_bindings_;
  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

void CopyOutputRequestCallback(base::Closure quit_closure,
                               gfx::Size expected_size,
                               std::unique_ptr<viz::CopyOutputResult> result) {
  EXPECT_EQ(expected_size, result->size());
  quit_closure.Run();
}

void CopyOutputRequestCallbackRunsOnceCallback(
    int* n_called,
    std::unique_ptr<viz::CopyOutputResult> result) {
  ++*n_called;
}

void CopyOutputRequestMessagePipeBrokenCallback(
    base::Closure closure,
    std::unique_ptr<viz::CopyOutputResult> result) {
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

TEST_F(StructTraitsTest, CopyOutputRequest_BitmapRequest) {
  const gfx::Rect area(5, 7, 44, 55);
  const auto source =
      base::UnguessableToken::Deserialize(0xdeadbeef, 0xdeadf00d);
  gfx::Size size(9, 8);
  auto bitmap = base::MakeUnique<SkBitmap>();
  bitmap->allocN32Pixels(size.width(), size.height());
  base::RunLoop run_loop;
  std::unique_ptr<viz::CopyOutputRequest> input =
      viz::CopyOutputRequest::CreateBitmapRequest(base::BindOnce(
          CopyOutputRequestCallback, run_loop.QuitClosure(), size));
  input->set_area(area);
  input->set_source(source);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::unique_ptr<viz::CopyOutputRequest> output;
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
  viz::TextureMailbox texture_mailbox(mailbox, gpu::SyncToken(), target);
  base::RunLoop run_loop;
  std::unique_ptr<viz::CopyOutputRequest> input =
      viz::CopyOutputRequest::CreateRequest(base::BindOnce(
          CopyOutputRequestCallback, run_loop.QuitClosure(), gfx::Size()));
  input->SetTextureMailbox(texture_mailbox);

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::unique_ptr<viz::CopyOutputRequest> output;
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
  auto request = viz::CopyOutputRequest::CreateRequest(
      base::BindOnce(CopyOutputRequestCallbackRunsOnceCallback, &n_called));
  auto result_sender = mojo::StructTraits<
      mojom::CopyOutputRequestDataView,
      std::unique_ptr<viz::CopyOutputRequest>>::result_sender(request);
  for (int i = 0; i < 10; i++)
    result_sender->SendResult(viz::CopyOutputResult::CreateEmptyResult());
  EXPECT_EQ(0, n_called);
  result_sender.FlushForTesting();
  EXPECT_EQ(1, n_called);
}

TEST_F(StructTraitsTest, CopyOutputRequest_MessagePipeBroken) {
  base::RunLoop run_loop;
  auto request = viz::CopyOutputRequest::CreateRequest(base::BindOnce(
      CopyOutputRequestMessagePipeBrokenCallback, run_loop.QuitClosure()));
  auto result_sender = mojo::StructTraits<
      mojom::CopyOutputRequestDataView,
      std::unique_ptr<viz::CopyOutputRequest>>::result_sender(request);
  result_sender.reset();
  // The callback must be called with an empty viz::CopyOutputResult. If it's
  // never called, this will never end and the test times out.
  run_loop.Run();
}

TEST_F(StructTraitsTest, CopyOutputResult_Bitmap) {
  auto bitmap = base::MakeUnique<SkBitmap>();
  bitmap->allocN32Pixels(7, 8);
  bitmap->eraseARGB(123, 213, 77, 33);
  auto in_bitmap = base::MakeUnique<SkBitmap>();
  in_bitmap->allocN32Pixels(7, 8);
  in_bitmap->eraseARGB(123, 213, 77, 33);
  auto input = viz::CopyOutputResult::CreateBitmapResult(std::move(bitmap));
  auto size = input->size();

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::unique_ptr<viz::CopyOutputResult> output;
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
  auto callback = viz::SingleReleaseCallback::Create(base::Bind(
      CopyOutputResultCallback, run_loop.QuitClosure(), sync_token, is_lost));
  gpu::Mailbox mailbox;
  mailbox.SetName(mailbox_name);
  viz::TextureMailbox texture_mailbox(mailbox, gpu::SyncToken(), target);

  auto input = viz::CopyOutputResult::CreateTextureResult(size, texture_mailbox,
                                                          std::move(callback));

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  std::unique_ptr<viz::CopyOutputResult> output;
  proxy->EchoCopyOutputResult(std::move(input), &output);

  EXPECT_FALSE(output->HasBitmap());
  EXPECT_TRUE(output->HasTexture());
  EXPECT_EQ(size, output->size());

  viz::TextureMailbox out_mailbox;
  std::unique_ptr<viz::SingleReleaseCallback> out_callback;
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

TEST_F(StructTraitsTest, SurfaceId) {
  static constexpr viz::FrameSinkId frame_sink_id(1337, 1234);
  static viz::LocalSurfaceId local_surface_id(0xfbadbeef,
                                              base::UnguessableToken::Create());
  viz::SurfaceId input(frame_sink_id, local_surface_id);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  viz::SurfaceId output;
  proxy->EchoSurfaceId(input, &output);
  EXPECT_EQ(frame_sink_id, output.frame_sink_id());
  EXPECT_EQ(local_surface_id, output.local_surface_id());
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
  const gfx::ColorSpace color_space = gfx::ColorSpace(
      gfx::ColorSpace::PrimaryID::BT470M, gfx::ColorSpace::TransferID::GAMMA28,
      gfx::ColorSpace::MatrixID::BT2020_NCL, gfx::ColorSpace::RangeID::LIMITED);
#if defined(OS_ANDROID)
  const bool is_backed_by_surface_texture = true;
  const bool wants_promotion_hint = true;
#endif

  gpu::Mailbox mailbox;
  mailbox.SetName(mailbox_name);
  viz::TextureMailbox input(mailbox, sync_token, texture_target, size_in_pixels,
                            is_overlay_candidate, secure_output_only);
  input.set_nearest_neighbor(nearest_neighbor);
  input.set_color_space(color_space);
#if defined(OS_ANDROID)
  input.set_is_backed_by_surface_texture(is_backed_by_surface_texture);
  input.set_wants_promotion_hint(wants_promotion_hint);
#endif

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  viz::TextureMailbox output;
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

}  // namespace cc
