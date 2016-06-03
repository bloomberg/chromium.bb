// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
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
  void EchoBeginFrameArgs(const BeginFrameArgs& b,
                          const EchoBeginFrameArgsCallback& callback) override {
    callback.Run(b);
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

  void EchoSurfaceId(const SurfaceId& s,
                     const EchoSurfaceIdCallback& callback) override {
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
  const int gpu_memory_buffer_id = 0xdeadbeef;
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
  input.gpu_memory_buffer_id.id = gpu_memory_buffer_id;
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
  EXPECT_EQ(gpu_memory_buffer_id, output.gpu_memory_buffer_id.id);
  EXPECT_EQ(is_overlay_candidate, output.is_overlay_candidate);
}

}  // namespace cc
