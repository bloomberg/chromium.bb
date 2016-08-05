// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop/message_loop.h"
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
  void EchoDxDiagNode(const DxDiagNode& d,
                      const EchoDxDiagNodeCallback& callback) override {
    callback.Run(d);
  }

  void EchoGpuDevice(const GPUInfo::GPUDevice& g,
                     const EchoGpuDeviceCallback& callback) override {
    callback.Run(g);
  }

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

  void EchoVideoDecodeAcceleratorSupportedProfile(
      const VideoDecodeAcceleratorSupportedProfile& v,
      const EchoVideoDecodeAcceleratorSupportedProfileCallback& callback)
      override {
    callback.Run(v);
  }

  void EchoVideoDecodeAcceleratorCapabilities(
      const VideoDecodeAcceleratorCapabilities& v,
      const EchoVideoDecodeAcceleratorCapabilitiesCallback& callback) override {
    callback.Run(v);
  }

  void EchoVideoEncodeAcceleratorSupportedProfile(
      const VideoEncodeAcceleratorSupportedProfile& v,
      const EchoVideoEncodeAcceleratorSupportedProfileCallback& callback)
      override {
    callback.Run(v);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

}  // namespace

TEST_F(StructTraitsTest, DxDiagNode) {
  gpu::DxDiagNode input;
  input.values["abc"] = "123";
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::DxDiagNode output;
  proxy->EchoDxDiagNode(input, &output);

  gpu::DxDiagNode test_dx_diag_node;
  test_dx_diag_node.values["abc"] = "123";
  EXPECT_EQ(test_dx_diag_node.values, output.values);
}

TEST_F(StructTraitsTest, GPUDevice) {
  gpu::GPUInfo::GPUDevice input;
  // Using the values from gpu/config/gpu_info_collector_unittest.cc::nvidia_gpu
  const uint32_t vendor_id = 0x10de;
  const uint32_t device_id = 0x0df8;
  const std::string vendor_string = "vendor_string";
  const std::string device_string = "device_string";

  input.vendor_id = vendor_id;
  input.device_id = device_id;
  input.vendor_string = vendor_string;
  input.device_string = device_string;
  input.active = false;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::GPUInfo::GPUDevice output;
  proxy->EchoGpuDevice(input, &output);

  EXPECT_EQ(vendor_id, output.vendor_id);
  EXPECT_EQ(device_id, output.device_id);
  EXPECT_FALSE(output.active);
  EXPECT_TRUE(vendor_string.compare(output.vendor_string) == 0);
  EXPECT_TRUE(device_string.compare(output.device_string) == 0);
}

TEST_F(StructTraitsTest, Mailbox) {
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9,
      8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7,
      6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2, 4, 6, 8, 0, 0, 9, 8, 7};
  gpu::Mailbox input;
  input.SetName(mailbox_name);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::Mailbox output;
  proxy->EchoMailbox(input, &output);
  gpu::Mailbox test_mailbox;
  test_mailbox.SetName(mailbox_name);
  EXPECT_EQ(test_mailbox, output);
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

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::MailboxHolder output;
  proxy->EchoMailboxHolder(input, &output);
  EXPECT_EQ(mailbox, output.mailbox);
  EXPECT_EQ(sync_token, output.sync_token);
  EXPECT_EQ(texture_target, output.texture_target);
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
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::SyncToken output;
  proxy->EchoSyncToken(input, &output);
  EXPECT_EQ(namespace_id, output.namespace_id());
  EXPECT_EQ(extra_data_field, output.extra_data_field());
  EXPECT_EQ(command_buffer_id, output.command_buffer_id());
  EXPECT_EQ(release_count, output.release_count());
  EXPECT_EQ(verified_flush, output.verified_flush());
}

TEST_F(StructTraitsTest, VideoDecodeAcceleratorSupportedProfile) {
  const gpu::VideoCodecProfile profile =
      gpu::VideoCodecProfile::H264PROFILE_MAIN;
  const int32_t max_width = 1920;
  const int32_t max_height = 1080;
  const int32_t min_width = 640;
  const int32_t min_height = 480;
  const gfx::Size max_resolution(max_width, max_height);
  const gfx::Size min_resolution(min_width, min_height);

  gpu::VideoDecodeAcceleratorSupportedProfile input;
  input.profile = profile;
  input.max_resolution = max_resolution;
  input.min_resolution = min_resolution;
  input.encrypted_only = false;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::VideoDecodeAcceleratorSupportedProfile output;
  proxy->EchoVideoDecodeAcceleratorSupportedProfile(input, &output);
  EXPECT_EQ(profile, output.profile);
  EXPECT_EQ(max_resolution, output.max_resolution);
  EXPECT_EQ(min_resolution, output.min_resolution);
  EXPECT_FALSE(output.encrypted_only);
}

TEST_F(StructTraitsTest, VideoDecodeAcceleratorCapabilities) {
  const uint32_t flags = 1234;

  gpu::VideoDecodeAcceleratorCapabilities input;
  input.flags = flags;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::VideoDecodeAcceleratorCapabilities output;
  proxy->EchoVideoDecodeAcceleratorCapabilities(input, &output);
  EXPECT_EQ(flags, output.flags);
}

TEST_F(StructTraitsTest, VideoEncodeAcceleratorSupportedProfile) {
  const gpu::VideoCodecProfile profile =
      gpu::VideoCodecProfile::H264PROFILE_MAIN;
  const gfx::Size max_resolution(1920, 1080);
  const uint32_t max_framerate_numerator = 144;
  const uint32_t max_framerate_denominator = 12;

  gpu::VideoEncodeAcceleratorSupportedProfile input;
  input.profile = profile;
  input.max_resolution = max_resolution;
  input.max_framerate_numerator = max_framerate_numerator;
  input.max_framerate_denominator = max_framerate_denominator;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::VideoEncodeAcceleratorSupportedProfile output;
  proxy->EchoVideoEncodeAcceleratorSupportedProfile(input, &output);
  EXPECT_EQ(profile, output.profile);
  EXPECT_EQ(max_resolution, output.max_resolution);
  EXPECT_EQ(max_framerate_numerator, output.max_framerate_numerator);
  EXPECT_EQ(max_framerate_denominator, output.max_framerate_denominator);
}

}  // namespace gpu
