// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/common/gpu_info.h"
#include "chrome/common/gpu_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test GPUInfo serialization
TEST(GPUIPCMessageTest, GPUInfo) {
  GPUInfo input;
  // Test variables taken from HP Z600 Workstation
  input.SetProgress(GPUInfo::kPartial);
  input.SetInitializationTime(base::TimeDelta::FromMilliseconds(100));
  input.SetVideoCardInfo(0x10de, 0x0658);
  input.SetDriverInfo("NVIDIA", "195.36.24");
  input.SetShaderVersion(0x0162, 0x0162);
  input.SetGLVersion(0x0302);
  input.SetGLVersionString("3.2.0 NVIDIA 195.36.24");
  input.SetGLVendor("NVIDIA Corporation");
  input.SetGLRenderer("Quadro FX 380/PCI/SSE2");
  input.SetCanLoseContext(false);

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  GPUInfo output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));
  EXPECT_EQ(input.progress(), output.progress());
  EXPECT_EQ(input.initialization_time().InMilliseconds(),
            output.initialization_time().InMilliseconds());
  EXPECT_EQ(input.vendor_id(), output.vendor_id());
  EXPECT_EQ(input.device_id(), output.device_id());
  EXPECT_EQ(input.driver_vendor(), output.driver_vendor());
  EXPECT_EQ(input.driver_version(), output.driver_version());
  EXPECT_EQ(input.pixel_shader_version(), output.pixel_shader_version());
  EXPECT_EQ(input.vertex_shader_version(), output.vertex_shader_version());
  EXPECT_EQ(input.gl_version(), output.gl_version());
  EXPECT_EQ(input.gl_version_string(), output.gl_version_string());
  EXPECT_EQ(input.gl_vendor(), output.gl_vendor());
  EXPECT_EQ(input.gl_renderer(), output.gl_renderer());
  EXPECT_EQ(input.can_lose_context(), output.can_lose_context());

  std::string log_message;
  IPC::LogParam(output, &log_message);
  EXPECT_STREQ("<GPUInfo> 1 100 10de 658 NVIDIA 195.36.24 162 162 302 0",
               log_message.c_str());
}
