// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "content/common/gpu_info.h"
#include "content/gpu/gpu_idirect3d9_mock_win.h"
#include "content/gpu/gpu_info_collector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;

class GPUInfoTest : public testing::Test {
 public:
  GPUInfoTest() { }
  virtual ~GPUInfoTest() { }

 protected:
  void SetUp() {
    // Test variables taken from Lenovo T61
    test_identifier_.VendorId = 0x10de;
    test_identifier_.DeviceId = 0x429;
    test_identifier_.DriverVersion.QuadPart = 0x6000e000b1e23;  // 6.14.11.7715
    test_caps_.PixelShaderVersion = 0xffff0300;  // 3.0
    test_caps_.VertexShaderVersion = 0xfffe0300;  // 3.0

    EXPECT_CALL(d3d_, GetDeviceCaps(_, _, _))
        .WillOnce(DoAll(SetArgumentPointee<2>(test_caps_),
                        Return(D3D_OK)));
    EXPECT_CALL(d3d_, QueryInterface(__uuidof(IDirect3D9Ex), _))
        .WillOnce(Return(E_NOINTERFACE));
    EXPECT_CALL(d3d_, Release());
  }
  void TearDown() {
  }

 public:
  IDirect3D9Mock d3d_;
 private:
  D3DADAPTER_IDENTIFIER9 test_identifier_;
  D3DCAPS9 test_caps_;
};

TEST_F(GPUInfoTest, PixelShaderVersionD3D) {
  GPUInfo gpu_info;
  ASSERT_TRUE(gpu_info_collector::CollectGraphicsInfoD3D(&d3d_, &gpu_info));
  uint32 ps_version = gpu_info.pixel_shader_version;
  EXPECT_EQ(ps_version, D3DPS_VERSION(3, 0));
}

TEST_F(GPUInfoTest, VertexShaderVersionD3D) {
  GPUInfo gpu_info;
  ASSERT_TRUE(gpu_info_collector::CollectGraphicsInfoD3D(&d3d_, &gpu_info));
  uint32 vs_version = gpu_info.vertex_shader_version;
  EXPECT_EQ(vs_version, D3DVS_VERSION(3, 0));
}
