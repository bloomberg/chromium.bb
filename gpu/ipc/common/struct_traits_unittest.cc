// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "gpu/ipc/common/traits_test_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

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
  void EchoMailbox(const Mailbox& m,
                   const EchoMailboxCallback& callback) override {
    callback.Run(m);
  }

  void EchoMailboxHolder(const MailboxHolder& r,
                         const EchoMailboxHolderCallback& callback) override {
    callback.Run(r);
  }

  void EchoSyncToken(const SyncToken& s,
                     const EchoSyncTokenCallback& callback) override {
    callback.Run(s);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;
};

}  // namespace

TEST_F(StructTraitsTest, Mailbox) {
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9,
      8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7,
      6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7};
  gpu::Mailbox input;
  input.SetName(mailbox_name);
  base::RunLoop loop;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  proxy->EchoMailbox(input, [&mailbox_name, &loop](const Mailbox& pass) {
    gpu::Mailbox test_mailbox;
    test_mailbox.SetName(mailbox_name);
    EXPECT_EQ(test_mailbox, pass);
    loop.Quit();
  });
  loop.Run();
}

TEST_F(StructTraitsTest, MailboxHolder) {
  gpu::MailboxHolder input;
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9,
      8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7,
      6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7};
  gpu::Mailbox mailbox;
  mailbox.SetName(mailbox_name);
  const gpu::CommandBufferNamespace namespace_id = gpu::IN_PROCESS;
  const int32_t extra_data_field = 0xbeefbeef;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdeadL;
  const gpu::SyncToken sync_token(namespace_id, extra_data_field,
                                  command_buffer_id, release_count);
  const uint32_t texture_target = 1337;
  input.mailbox = mailbox;
  input.sync_token = sync_token;
  input.texture_target = texture_target;

  base::RunLoop loop;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  proxy->EchoMailboxHolder(input, [mailbox, sync_token, texture_target,
                                   &loop](const MailboxHolder& pass) {
    EXPECT_EQ(mailbox, pass.mailbox);
    EXPECT_EQ(sync_token, pass.sync_token);
    EXPECT_EQ(texture_target, pass.texture_target);
    loop.Quit();
  });
}

TEST_F(StructTraitsTest, SyncToken) {
  const gpu::CommandBufferNamespace namespace_id = gpu::IN_PROCESS;
  const int32_t extra_data_field = 0xbeefbeef;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdead;
  const bool verified_flush = false;
  gpu::SyncToken input(namespace_id, extra_data_field, command_buffer_id,
                       release_count);
  base::RunLoop loop;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  proxy->EchoSyncToken(
      input, [namespace_id, extra_data_field, command_buffer_id, release_count,
              verified_flush, &loop](const SyncToken& pass) {
        EXPECT_EQ(namespace_id, pass.namespace_id());
        EXPECT_EQ(extra_data_field, pass.extra_data_field());
        EXPECT_EQ(command_buffer_id, pass.command_buffer_id());
        EXPECT_EQ(release_count, pass.release_count());
        EXPECT_EQ(verified_flush, pass.verified_flush());
        loop.Quit();
      });
  loop.Run();
}

}  // namespace gpu
