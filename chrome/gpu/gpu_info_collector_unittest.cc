// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gl/gl_implementation.h"
#include "base/scoped_ptr.h"
#include "chrome/common/gpu_info.h"
#include "chrome/gpu/gpu_info_collector.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::gfx::MockGLInterface;
using ::testing::Return;

class GPUInfoCollectorTest : public testing::Test {
 public:
  GPUInfoCollectorTest() {}
  virtual ~GPUInfoCollectorTest() { }

  void SetUp() {
    gfx::InitializeGLBindings(gfx::kGLImplementationMockGL);
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
#if defined(OS_WIN)
    const uint32 vendor_id = 0x10de;
    const uint32 device_id = 0x0658;
    const char* driver_vendor = "";  // not implemented
    const char* driver_version = "";
    const uint32 shader_version = 0x00000128;
    const uint32 gl_version = 0x00000301;
    const char* gl_renderer = "Quadro FX 380/PCI/SSE2";
    const char* gl_vendor = "NVIDIA Corporation";
    const char* gl_version_string = "3.1.0";
    const char* gl_shading_language_version = "1.40 NVIDIA via Cg compiler";
    const char* gl_extensions =
        "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
        "GL_EXT_read_format_bgra";
#elif defined(OS_MACOSX)
    const uint32 vendor_id = 0x10de;
    const uint32 device_id = 0x0640;
    const char* driver_vendor = "";  // not implemented
    const char* driver_version = "1.6.18";
    const uint32 shader_version = 0x00000114;
    const uint32 gl_version = 0x00000201;
    const char* gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
    const char* gl_vendor = "NVIDIA Corporation";
    const char* gl_version_string = "2.1 NVIDIA-1.6.18";
    const char* gl_shading_language_version = "1.20 ";
    const char* gl_extensions =
        "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
        "GL_EXT_read_format_bgra";
#else  // defined (OS_LINUX)
    const uint32 vendor_id = 0x10de;
    const uint32 device_id = 0x0658;
    const char* driver_vendor = "NVIDIA";
    const char* driver_version = "195.36.24";
    const uint32 shader_version = 0x00000132;
    const uint32 gl_version = 0x00000302;
    const char* gl_renderer = "Quadro FX 380/PCI/SSE2";
    const char* gl_vendor = "NVIDIA Corporation";
    const char* gl_version_string = "3.2.0 NVIDIA 195.36.24";
    const char* gl_shading_language_version = "1.50 NVIDIA via Cg compiler";
    const char* gl_extensions =
        "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
        "GL_EXT_read_format_bgra";
#endif
    test_values_.SetVideoCardInfo(vendor_id, device_id);
    test_values_.SetDriverInfo(driver_vendor, driver_version);
    test_values_.SetShaderVersion(shader_version, shader_version);
    test_values_.SetGLVersion(gl_version);
    test_values_.SetGLRenderer(gl_renderer);
    test_values_.SetGLVendor(gl_vendor);
    test_values_.SetGLVersionString(gl_version_string);
    test_values_.SetGLExtensions(gl_extensions);
    test_values_.SetCanLoseContext(false);

    EXPECT_CALL(*gl_, GetString(GL_EXTENSIONS))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_extensions)));
    EXPECT_CALL(*gl_, GetString(GL_SHADING_LANGUAGE_VERSION))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_shading_language_version)));
    EXPECT_CALL(*gl_, GetString(GL_VERSION))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_version_string)));
    EXPECT_CALL(*gl_, GetString(GL_VENDOR))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_vendor)));
    EXPECT_CALL(*gl_, GetString(GL_RENDERER))
        .WillRepeatedly(Return(reinterpret_cast<const GLubyte*>(
            gl_renderer)));
  }

  void TearDown() {
    ::gfx::GLInterface::SetGLInterface(NULL);
    gl_.reset();
  }

 public:
  // Use StrictMock to make 100% sure we know how GL will be called.
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  GPUInfo test_values_;
};

// TODO(rlp): Test the vendor and device id collection if deemed necessary as
//            it involves several complicated mocks for each platform.

TEST_F(GPUInfoCollectorTest, DriverVendorGL) {
  GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  std::string driver_vendor = gpu_info.driver_vendor();
  EXPECT_EQ(test_values_.driver_vendor(), driver_vendor);
}

TEST_F(GPUInfoCollectorTest, DriverVersionGL) {
  GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  std::string driver_version = gpu_info.driver_version();
  EXPECT_EQ(test_values_.driver_version(), driver_version);
}

TEST_F(GPUInfoCollectorTest, PixelShaderVersionGL) {
  GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  uint32 ps_version = gpu_info.pixel_shader_version();
  EXPECT_EQ(test_values_.pixel_shader_version(), ps_version);
}

TEST_F(GPUInfoCollectorTest, VertexShaderVersionGL) {
  GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  uint32 vs_version = gpu_info.vertex_shader_version();
  EXPECT_EQ(test_values_.vertex_shader_version(), vs_version);
}

TEST_F(GPUInfoCollectorTest, GLVersionGL) {
  GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  uint32 gl_version = gpu_info.gl_version();
  EXPECT_EQ(test_values_.gl_version(), gl_version);
}

TEST_F(GPUInfoCollectorTest, GLVersionStringGL) {
  GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  std::string gl_version_string = gpu_info.gl_version_string();
  EXPECT_EQ(test_values_.gl_version_string(), gl_version_string);
}

TEST_F(GPUInfoCollectorTest, GLRendererGL) {
  GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  std::string gl_renderer = gpu_info.gl_renderer();
  EXPECT_EQ(test_values_.gl_renderer(), gl_renderer);
}

TEST_F(GPUInfoCollectorTest, GLVendorGL) {
  GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  std::string gl_vendor = gpu_info.gl_vendor();
  EXPECT_EQ(test_values_.gl_vendor(), gl_vendor);
}

TEST_F(GPUInfoCollectorTest, GLExtensionsGL) {
  GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  std::string gl_extensions = gpu_info.gl_extensions();
  EXPECT_EQ(test_values_.gl_extensions(), gl_extensions);
}


