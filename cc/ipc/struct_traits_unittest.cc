// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "cc/input/selection.h"
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
  void EchoCopyOutputResult(std::unique_ptr<viz::CopyOutputResult> c,
                            EchoCopyOutputResultCallback callback) override {
    std::move(callback).Run(std::move(c));
  }

  void EchoTextureMailbox(const viz::TextureMailbox& t,
                          EchoTextureMailboxCallback callback) override {
    std::move(callback).Run(t);
  }

  mojo::BindingSet<TraitsTestService> traits_test_bindings_;
  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

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

TEST_F(StructTraitsTest, CopyOutputResult_Bitmap) {
  auto bitmap = std::make_unique<SkBitmap>();
  bitmap->allocN32Pixels(7, 8);
  bitmap->eraseARGB(123, 213, 77, 33);
  auto in_bitmap = std::make_unique<SkBitmap>();
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
