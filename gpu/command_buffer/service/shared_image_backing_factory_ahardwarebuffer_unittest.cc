// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_ahardwarebuffer.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

class SharedImageBackingFactoryAHardwareBufferTest : public testing::Test {
 public:
  void SetUp() override {
    // AHardwareBuffer is only supported on ANDROID O+. Hence these tests
    // should not be run on android versions less that O.
    if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
      return;

    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    bool result = context_->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    GpuDriverBugWorkarounds workarounds;
    workarounds.max_texture_size = INT_MAX - 1;
    backing_factory_ =
        std::make_unique<SharedImageBackingFactoryAHardwareBuffer>(workarounds);
  }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  std::unique_ptr<SharedImageBackingFactoryAHardwareBuffer> backing_factory_;
  gles2::MailboxManagerImpl mailbox_manager_;
};

// Basic test to check creation and deletion of AHB backed shared image.
TEST_F(SharedImageBackingFactoryAHardwareBufferTest, Basic) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  auto backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                     color_space, usage);
  EXPECT_TRUE(backing);

  // There is no Legacy mailbox support in AHardwareBuffer Implementation.
  EXPECT_FALSE(backing->ProduceLegacyMailbox(&mailbox_manager_));
  TextureBase* texture_base = mailbox_manager_.ConsumeTexture(mailbox);
  ASSERT_FALSE(texture_base);

  // Destroy the backing resource.
  backing->Destroy();
}

// Test to check invalid format support.
TEST_F(SharedImageBackingFactoryAHardwareBufferTest, InvalidFormat) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::UYVY_422;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  auto backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                     color_space, usage);
  EXPECT_FALSE(backing);
}

// Test to check invalid size support.
TEST_F(SharedImageBackingFactoryAHardwareBufferTest, InvalidSize) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  auto mailbox = Mailbox::Generate();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(0, 0);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  auto backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                     color_space, usage);
  EXPECT_FALSE(backing);

  size = gfx::Size(INT_MAX, INT_MAX);
  backing = backing_factory_->CreateSharedImage(mailbox, format, size,
                                                color_space, usage);
  EXPECT_FALSE(backing);
}

}  // anonymous namespace
}  // namespace gpu
