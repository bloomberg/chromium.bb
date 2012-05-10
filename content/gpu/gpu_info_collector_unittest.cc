// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "content/gpu/gpu_info_collector.h"
#include "content/public/common/gpu_info.h"
#include "gpu/command_buffer/common/gl_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/gl/gl_implementation.h"

using ::gfx::MockGLInterface;
using ::testing::Return;

class GPUInfoCollectorTest : public testing::Test {
 public:
  GPUInfoCollectorTest() {}
  virtual ~GPUInfoCollectorTest() { }

  void SetUp() {
    // TODO(kbr): make this setup robust in the case where
    // GLSurface::InitializeOneOff() has already been called by
    // another unit test. http://crbug.com/100285
    gfx::InitializeGLBindings(gfx::kGLImplementationMockGL);
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::GLInterface::SetGLInterface(gl_.get());
#if defined(OS_WIN)
    const uint32 vendor_id = 0x10de;
    const uint32 device_id = 0x0658;
    const char* driver_vendor = "";  // not implemented
    const char* driver_version = "";
    const char* shader_version = "1.40";
    const char* gl_version = "3.1";
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
    const char* shader_version = "1.20";
    const char* gl_version = "2.1";
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
    const char* shader_version = "1.50";
    const char* gl_version = "3.2";
    const char* gl_renderer = "Quadro FX 380/PCI/SSE2";
    const char* gl_vendor = "NVIDIA Corporation";
    const char* gl_version_string = "3.2.0 NVIDIA 195.36.24";
    const char* gl_shading_language_version = "1.50 NVIDIA via Cg compiler";
    const char* gl_extensions =
        "GL_OES_packed_depth_stencil GL_EXT_texture_format_BGRA8888 "
        "GL_EXT_read_format_bgra";
#endif
    test_values_.gpu.vendor_id = vendor_id;
    test_values_.gpu.device_id = device_id;
    test_values_.driver_vendor = driver_vendor;
    test_values_.driver_version =driver_version;
    test_values_.pixel_shader_version = shader_version;
    test_values_.vertex_shader_version = shader_version;
    test_values_.gl_version = gl_version;
    test_values_.gl_renderer = gl_renderer;
    test_values_.gl_vendor = gl_vendor;
    test_values_.gl_version_string = gl_version_string;
    test_values_.gl_extensions = gl_extensions;
    test_values_.can_lose_context = false;

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
  content::GPUInfo test_values_;
};

// TODO(rlp): Test the vendor and device id collection if deemed necessary as
//            it involves several complicated mocks for each platform.

// TODO(kbr): re-enable these tests; see http://crbug.com/100285 .

TEST_F(GPUInfoCollectorTest, DISABLED_DriverVendorGL) {
  content::GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.driver_vendor,
            gpu_info.driver_vendor);
}

// Skip Windows because the driver version is obtained from bot registry.
#if !defined(OS_WIN)
TEST_F(GPUInfoCollectorTest, DISABLED_DriverVersionGL) {
  content::GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.driver_version,
            gpu_info.driver_version);
}
#endif

TEST_F(GPUInfoCollectorTest, DISABLED_PixelShaderVersionGL) {
  content::GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.pixel_shader_version,
            gpu_info.pixel_shader_version);
}

TEST_F(GPUInfoCollectorTest, DISABLED_VertexShaderVersionGL) {
  content::GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.vertex_shader_version,
            gpu_info.vertex_shader_version);
}

TEST_F(GPUInfoCollectorTest, DISABLED_GLVersionGL) {
  content::GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.gl_version,
            gpu_info.gl_version);
}

TEST_F(GPUInfoCollectorTest, DISABLED_GLVersionStringGL) {
  content::GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.gl_version_string,
            gpu_info.gl_version_string);
}

TEST_F(GPUInfoCollectorTest, DISABLED_GLRendererGL) {
  content::GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.gl_renderer,
            gpu_info.gl_renderer);
}

TEST_F(GPUInfoCollectorTest, DISABLED_GLVendorGL) {
  content::GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.gl_vendor,
            gpu_info.gl_vendor);
}

TEST_F(GPUInfoCollectorTest, DISABLED_GLExtensionsGL) {
  content::GPUInfo gpu_info;
  gpu_info_collector::CollectGraphicsInfoGL(&gpu_info);
  EXPECT_EQ(test_values_.gl_extensions,
            gpu_info.gl_extensions);
}
