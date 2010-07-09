// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chrome/common/gpu_info.h"
#include "chrome/common/gpu_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test GPUInfo serialization
TEST(GPUIPCMessageTest, GPUInfo) {
  GPUInfo input;
  // Test variables taken from Lenovo T61
  input.SetGraphicsInfo(0x10de, 0x429, L"6.14.11.7715",
                        0xffff0300, 0xfffe0300);

  IPC::Message msg(1, 2, IPC::Message::PRIORITY_NORMAL);
  IPC::WriteParam(&msg, input);

  GPUInfo output;
  void* iter = NULL;
  EXPECT_TRUE(IPC::ReadParam(&msg, &iter, &output));
  EXPECT_EQ(input.vendor_id(), output.vendor_id());
  EXPECT_EQ(input.device_id(), output.device_id());
  EXPECT_EQ(input.driver_version(), output.driver_version());
  EXPECT_EQ(input.pixel_shader_version(), output.pixel_shader_version());
  EXPECT_EQ(input.vertex_shader_version(), output.vertex_shader_version());

  std::wstring log_message;
  IPC::LogParam(output, &log_message);
  EXPECT_STREQ(L"<GPUInfo> 10de 429 6.14.11.7715", log_message.c_str());
}
