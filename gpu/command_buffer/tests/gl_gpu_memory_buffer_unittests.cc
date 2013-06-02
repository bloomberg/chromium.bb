// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <GLES2/gl2.h>
#include <GLES2/gl2chromium.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_mock.h"
#include "gpu/command_buffer/client/image_factory_mock.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/tests/gl_manager.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_image_mock.h"

using testing::_;
using testing::IgnoreResult;
using testing::InvokeWithoutArgs;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace gpu {
namespace gles2 {

static const int kImageWidth = 256;
static const int kImageHeight = 256;
static const int kImageBytesPerPixel = 4;

class MockGpuMemoryBufferTest : public testing::Test {
 protected:
  virtual void SetUp() {
    GLManager::Options options;
    image_manager_ = new ImageManager;
    image_factory_.reset(
        new StrictMock<ImageFactoryMock>(image_manager_));
    options.image_manager = image_manager_.get();
    options.image_factory = image_factory_.get();

    gl_.Initialize(options);
    gl_.MakeCurrent();
  }

  virtual void TearDown() {
    gl_.Destroy();
  }

  scoped_ptr<StrictMock<ImageFactoryMock> > image_factory_;
  scoped_refptr<ImageManager> image_manager_;
  GLManager gl_;
};

// An end to end test that tests the whole GpuMemoryBuffer lifecycle.
TEST_F(MockGpuMemoryBufferTest, Lifecycle) {
  // Create a client texture id.
  GLuint texture_id;
  glGenTextures(1, &texture_id);

  // Buffer is owned and freed by GpuMemoryBufferTracker.
  StrictMock<GpuMemoryBufferMock>* gpu_memory_buffer =
      new StrictMock<GpuMemoryBufferMock>(kImageWidth, kImageHeight);

  const GLuint kImageId = 345u;

  EXPECT_CALL(*image_factory_.get(), CreateGpuMemoryBufferMock(
      kImageWidth, kImageHeight, GL_RGBA8_OES, _))
      .Times(1)
      .WillOnce(DoAll(SetArgPointee<3>(kImageId),
                      Return(gpu_memory_buffer)))
      .RetiresOnSaturation();

  // Create the GLImage and insert it into the ImageManager, which
  // would be done within CreateGpuMemoryBufferMock if it weren't a mock.
  GLuint image_id = glCreateImageCHROMIUM(
      kImageWidth, kImageHeight, GL_RGBA8_OES);
  EXPECT_EQ(kImageId, image_id);

  gfx::Size size(kImageWidth, kImageHeight);
  scoped_refptr<gfx::GLImageMock> gl_image(
      new gfx::GLImageMock(gpu_memory_buffer, size));
  image_manager_->AddImage(gl_image.get(), image_id);

  EXPECT_CALL(*gpu_memory_buffer, IsMapped())
      .WillOnce(Return(false))
      .RetiresOnSaturation();

  scoped_ptr<uint8[]> buffer_pixels(new uint8[
      kImageWidth * kImageHeight * kImageBytesPerPixel]);

  EXPECT_CALL(*gpu_memory_buffer, Map(_, _))
      .Times(1)
      .WillOnce(SetArgPointee<1>(buffer_pixels.get()))
      .RetiresOnSaturation();
  void* mapped_buffer =
      glMapImageCHROMIUM(image_id, GL_WRITE_ONLY);
  EXPECT_EQ(buffer_pixels.get(), mapped_buffer);

  EXPECT_CALL(*gpu_memory_buffer, IsMapped())
      .WillOnce(Return(true))
      .RetiresOnSaturation();

  // Unmap the image.
  EXPECT_CALL(*gpu_memory_buffer, Unmap())
      .Times(1)
      .RetiresOnSaturation();
  glUnmapImageCHROMIUM(image_id);

  // Bind the texture and the image.
  glBindTexture(GL_TEXTURE_2D, texture_id);
  EXPECT_CALL(*gl_image.get(), BindTexImage()).Times(1).WillOnce(Return(true))
      .RetiresOnSaturation();
  EXPECT_CALL(*gl_image.get(), GetSize()).Times(1).WillOnce(Return(size))
      .RetiresOnSaturation();
  glBindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_id);

  // Destroy the image.
  EXPECT_CALL(*gpu_memory_buffer, Die())
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*image_factory_.get(), DeleteGpuMemoryBuffer(image_id))
      .Times(1)
      .RetiresOnSaturation();

  glDestroyImageCHROMIUM(image_id);

  // Delete the texture.
  glDeleteTextures(1, &texture_id);
}

}  // namespace gles2
}  // namespace gpu
