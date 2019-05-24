// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/webgpu_decoder.h"

#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace gpu {
namespace webgpu {

class WebGPUDecoderTest : public ::testing::Test {
 public:
  WebGPUDecoderTest() {}

  void SetUp() override {
    command_buffer_service_.reset(new FakeCommandBufferServiceBase());
    decoder_.reset(WebGPUDecoder::Create(nullptr, command_buffer_service_.get(),
                                         &shared_image_manager_, nullptr,
                                         &outputter_));
    if (decoder_->Initialize() != ContextResult::kSuccess) {
      decoder_ = nullptr;
    }

    factory_ = std::make_unique<SharedImageFactory>(
        GpuPreferences(), GpuDriverBugWorkarounds(), GpuFeatureInfo(),
        /*context_state=*/nullptr, /*mailbox_manager=*/nullptr,
        &shared_image_manager_, /*image_factory=*/nullptr, /*tracker=*/nullptr,
        /*enable_wrapped_sk_image=*/false);
  }

  void TearDown() override {
    factory_->DestroyAllSharedImages(true);
    factory_.reset();
  }

  bool WebGPUSupported() const { return decoder_ != nullptr; }

  template <typename T>
  error::Error ExecuteCmd(const T& cmd) {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    int entries_processed = 0;
    return decoder_->DoCommands(1, (const void*)&cmd,
                                ComputeNumEntries(sizeof(cmd)),
                                &entries_processed);
  }

  template <typename T>
  error::Error ExecuteImmediateCmd(const T& cmd, size_t data_size) {
    static_assert(T::kArgFlags == cmd::kAtLeastN,
                  "T::kArgFlags should equal cmd::kAtLeastN");
    int entries_processed = 0;
    return decoder_->DoCommands(1, (const void*)&cmd,
                                ComputeNumEntries(sizeof(cmd) + data_size),
                                &entries_processed);
  }

 protected:
  std::unique_ptr<FakeCommandBufferServiceBase> command_buffer_service_;
  std::unique_ptr<WebGPUDecoder> decoder_;
  gles2::TraceOutputter outputter_;
  SharedImageManager shared_image_manager_;
  std::unique_ptr<SharedImageFactory> factory_;
  scoped_refptr<gles2::ContextGroup> group_;
};

TEST_F(WebGPUDecoderTest, DawnCommands) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  cmds::DawnCommands cmd;
  cmd.Init(0, 0, 0);
  EXPECT_EQ(error::kOutOfBounds, ExecuteCmd(cmd));
}

struct AssociateMailboxCmdStorage {
  cmds::AssociateMailboxImmediate cmd;
  GLbyte data[GL_MAILBOX_SIZE_CHROMIUM];
};

TEST_F(WebGPUDecoderTest, AssociateMailbox) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  gpu::Mailbox mailbox = Mailbox::GenerateForSharedImage();
  EXPECT_TRUE(factory_->CreateSharedImage(
      mailbox, viz::ResourceFormat::RGBA_8888, {1, 1},
      gfx::ColorSpace::CreateSRGB(), SHARED_IMAGE_USAGE_WEBGPU));

  // Error case: invalid mailbox
  {
    gpu::Mailbox bad_mailbox;
    AssociateMailboxCmdStorage cmd;
    cmd.cmd.Init(0, 0, 1, 0, DAWN_TEXTURE_USAGE_BIT_SAMPLED, bad_mailbox.name);
    EXPECT_EQ(error::kInvalidArguments,
              ExecuteImmediateCmd(cmd.cmd, sizeof(bad_mailbox.name)));
  }

  // Error case: device doesn't exist.
  {
    AssociateMailboxCmdStorage cmd;
    cmd.cmd.Init(42, 42, 1, 0, DAWN_TEXTURE_USAGE_BIT_SAMPLED, mailbox.name);
    EXPECT_EQ(error::kInvalidArguments,
              ExecuteImmediateCmd(cmd.cmd, sizeof(mailbox.name)));
  }

  // Error case: texture ID invalid for the wire server.
  {
    AssociateMailboxCmdStorage cmd;
    cmd.cmd.Init(0, 0, 42, 42, DAWN_TEXTURE_USAGE_BIT_SAMPLED, mailbox.name);
    EXPECT_EQ(error::kInvalidArguments,
              ExecuteImmediateCmd(cmd.cmd, sizeof(mailbox.name)));
  }

  // Error case: invalid usage.
  {
    AssociateMailboxCmdStorage cmd;
    cmd.cmd.Init(0, 0, 42, 42, DAWN_TEXTURE_USAGE_BIT_SAMPLED, mailbox.name);
    EXPECT_EQ(error::kInvalidArguments,
              ExecuteImmediateCmd(cmd.cmd, sizeof(mailbox.name)));
  }

  // Error case: invalid texture usage.
  {
    AssociateMailboxCmdStorage cmd;
    cmd.cmd.Init(0, 0, 1, 0, DAWN_TEXTURE_USAGE_BIT_FORCE32, mailbox.name);
    EXPECT_EQ(error::kInvalidArguments,
              ExecuteImmediateCmd(cmd.cmd, sizeof(mailbox.name)));
  }

  // Control case: test a successful call to AssociateMailbox
  // (1, 0) is a valid texture ID on dawn_wire server start.
  // The control case is not put first because it modifies the internal state
  // of the Dawn wire server and would make calls with the same texture ID
  // and generation invalid.
  {
    AssociateMailboxCmdStorage cmd;
    cmd.cmd.Init(0, 0, 1, 0, DAWN_TEXTURE_USAGE_BIT_SAMPLED, mailbox.name);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd.cmd, sizeof(mailbox.name)));
  }

  // Error case: associated to an already associated texture.
  {
    AssociateMailboxCmdStorage cmd;
    cmd.cmd.Init(0, 0, 1, 0, DAWN_TEXTURE_USAGE_BIT_SAMPLED, mailbox.name);
    EXPECT_EQ(error::kInvalidArguments,
              ExecuteImmediateCmd(cmd.cmd, sizeof(mailbox.name)));
  }

  // Dissociate the image from the control case to remove its reference.
  {
    cmds::DissociateMailbox cmd;
    cmd.Init(1, 0);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
}

TEST_F(WebGPUDecoderTest, DissociateMailbox) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  gpu::Mailbox mailbox = Mailbox::GenerateForSharedImage();
  EXPECT_TRUE(factory_->CreateSharedImage(
      mailbox, viz::ResourceFormat::RGBA_8888, {1, 1},
      gfx::ColorSpace::CreateSRGB(), SHARED_IMAGE_USAGE_WEBGPU));

  // Associate a mailbox so we can later dissociate it.
  {
    AssociateMailboxCmdStorage cmd;
    cmd.cmd.Init(0, 0, 1, 0, DAWN_TEXTURE_USAGE_BIT_SAMPLED, mailbox.name);
    EXPECT_EQ(error::kNoError,
              ExecuteImmediateCmd(cmd.cmd, sizeof(mailbox.name)));
  }

  // Error case: wrong texture ID
  {
    cmds::DissociateMailbox cmd;
    cmd.Init(42, 42);
    EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(cmd));
  }

  // Success case
  {
    cmds::DissociateMailbox cmd;
    cmd.Init(1, 0);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  }
}

}  // namespace webgpu
}  // namespace gpu
