// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "cc/ipc/traits_test_service.mojom.h"
#include "cc/quads/render_pass_id.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  void EchoRenderPassId(const RenderPassId& r,
                        const EchoRenderPassIdCallback& callback) override {
    callback.Run(r);
  }

  void EchoReturnedResource(
      const ReturnedResource& r,
      const EchoReturnedResourceCallback& callback) override {
    callback.Run(r);
  }

  void EchoSurfaceId(const SurfaceId& s,
                     const EchoSurfaceIdCallback& callback) override {
    callback.Run(s);
  }

  mojo::BindingSet<TraitsTestService> traits_test_bindings_;
};

}  // namespace

TEST_F(StructTraitsTest, RenderPassId) {
  const int layer_id = 1337;
  const uint32_t index = 0xdeadbeef;
  RenderPassId input(layer_id, index);
  base::RunLoop loop;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  proxy->EchoRenderPassId(input,
                          [layer_id, index, &loop](const RenderPassId& pass) {
                            EXPECT_EQ(layer_id, pass.layer_id);
                            EXPECT_EQ(index, pass.index);
                            loop.Quit();
                          });
  loop.Run();
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
  base::RunLoop loop;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  proxy->EchoReturnedResource(input, [id, sync_token, count, lost,
                                      &loop](const ReturnedResource& pass) {
    EXPECT_EQ(id, pass.id);
    EXPECT_EQ(sync_token, pass.sync_token);
    EXPECT_EQ(count, pass.count);
    EXPECT_EQ(lost, pass.lost);
    loop.Quit();
  });
  loop.Run();
}

TEST_F(StructTraitsTest, SurfaceId) {
  const uint32_t id_namespace = 1337;
  const uint32_t local_id = 0xfbadbeef;
  const uint64_t nonce = 0xdeadbeef;
  SurfaceId input(id_namespace, local_id, nonce);
  base::RunLoop loop;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  proxy->EchoSurfaceId(
      input, [id_namespace, local_id, nonce, &loop](const SurfaceId& pass) {
        EXPECT_EQ(id_namespace, pass.id_namespace());
        EXPECT_EQ(local_id, pass.local_id());
        EXPECT_EQ(nonce, pass.nonce());
        loop.Quit();
      });
  loop.Run();
}

}  // namespace cc
