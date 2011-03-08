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
  input.level = GPUInfo::kPartial;
  input.initialization_time = base::TimeDelta::FromMilliseconds(100);
  input.vendor_id = 0x10de;
  input.device_id = 0x0658;
  input.driver_vendor = "NVIDIA";
  input.driver_version = "195.36.24";
  input.driver_date = "7-14-2009";
  input.pixel_shader_version = 0x0162;
  input.vertex_shader_version = 0x0162;
  input.gl_version = 0x0302;
  input.gl_version_string = "3.2.0 NVIDIA 195.36.24";
  input.gl_vendor = "NVIDIA Corporation";
  input.gl_renderer = "Quadro FX 380/PCI/SSE2";
  input.gl_extensions ="GL_ARB_texture_rg GL_ARB_window_pos";
  input.can_lose_context = false;

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  GPUInfo output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));
  EXPECT_EQ(input.level, output.level);
  EXPECT_EQ(input.initialization_time.InMilliseconds(),
            output.initialization_time.InMilliseconds());
  EXPECT_EQ(input.vendor_id, output.vendor_id);
  EXPECT_EQ(input.device_id, output.device_id);
  EXPECT_EQ(input.driver_vendor, output.driver_vendor);
  EXPECT_EQ(input.driver_version, output.driver_version);
  EXPECT_EQ(input.driver_date, output.driver_date);
  EXPECT_EQ(input.pixel_shader_version, output.pixel_shader_version);
  EXPECT_EQ(input.vertex_shader_version, output.vertex_shader_version);
  EXPECT_EQ(input.gl_version, output.gl_version);
  EXPECT_EQ(input.gl_version_string, output.gl_version_string);
  EXPECT_EQ(input.gl_vendor, output.gl_vendor);
  EXPECT_EQ(input.gl_renderer, output.gl_renderer);
  EXPECT_EQ(input.gl_extensions, output.gl_extensions);
  EXPECT_EQ(input.can_lose_context, output.can_lose_context);

  std::string log_message;
  IPC::LogParam(output, &log_message);
  EXPECT_STREQ("<GPUInfo> 2 100 10de 658 NVIDIA "
               "195.36.24 7-14-2009 162 162 302 0",
               log_message.c_str());
}
