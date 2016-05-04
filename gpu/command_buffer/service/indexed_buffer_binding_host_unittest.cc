// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/buffer_manager.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/indexed_buffer_binding_host.h"
#include "ui/gl/gl_mock.h"

namespace gpu {
namespace gles2 {

namespace {
const uint32_t kMaxBindings = 16;
const GLuint kBufferClientId = 87;
const GLuint kBufferServiceId = 987;
}  // namespace anonymous

class IndexedBufferBindingHostTest : public GpuServiceTest {
 public:
  IndexedBufferBindingHostTest()
      : host_(new IndexedBufferBindingHost(kMaxBindings, true)),
        buffer_manager_(new BufferManager(nullptr, nullptr)) {
    buffer_manager_->CreateBuffer(kBufferClientId, kBufferServiceId);
    buffer_ = buffer_manager_->GetBuffer(kBufferClientId);
    DCHECK(buffer_.get());
  }

  ~IndexedBufferBindingHostTest() override {}

 protected:
  void SetUp() override {
    GpuServiceTest::SetUpWithGLVersion("4.1", "");
  }

  void TearDown() override {
    host_->RemoveBoundBuffer(buffer_.get());
    buffer_ = nullptr;
    buffer_manager_->MarkContextLost();
    buffer_manager_->Destroy();
    buffer_manager_.reset();
    GpuServiceTest::TearDown();
  }

  void SetBufferSize(GLenum target, GLsizeiptr size) {
    buffer_manager_->SetInfo(
        buffer_.get(), target, size, GL_STATIC_DRAW, false);
  }

  scoped_refptr<IndexedBufferBindingHost> host_;
  std::unique_ptr<BufferManager> buffer_manager_;
  scoped_refptr<Buffer> buffer_;
};

TEST_F(IndexedBufferBindingHostTest, DoBindBufferRangeUninitializedBuffer) {
  const GLenum kTarget = GL_TRANSFORM_FEEDBACK_BUFFER;
  const GLuint kIndex = 2;
  const GLintptr kOffset = 4;
  const GLsizeiptr kSize = 8;

  EXPECT_CALL(*gl_, BindBufferBase(kTarget, kIndex, kBufferServiceId))
      .Times(1)
      .RetiresOnSaturation();

  host_->DoBindBufferRange(kTarget, kIndex, buffer_.get(), kOffset, kSize);

  for (uint32_t index = 0; index < kMaxBindings; ++index) {
    if (index != kIndex) {
      EXPECT_EQ(nullptr, host_->GetBufferBinding(index));
    } else {
      EXPECT_EQ(buffer_.get(), host_->GetBufferBinding(index));
      EXPECT_EQ(kSize, host_->GetBufferSize(index));
      EXPECT_EQ(kOffset, host_->GetBufferStart(index));
    }
  }
}

TEST_F(IndexedBufferBindingHostTest, DoBindBufferRangeBufferWithoutEnoughSize) {
  const GLenum kTarget = GL_TRANSFORM_FEEDBACK_BUFFER;
  const GLuint kIndex = 2;
  const GLintptr kOffset = 4;
  const GLsizeiptr kSize = 8;
  const GLsizeiptr kBufferSize = kOffset + kSize - 2;

  SetBufferSize(kTarget, kBufferSize);

  GLsizeiptr clamped_size = ((kBufferSize - kOffset) >> 2) << 2;

  EXPECT_CALL(*gl_, BindBufferRange(kTarget, kIndex, kBufferServiceId, kOffset,
                                    clamped_size))
      .Times(1)
      .RetiresOnSaturation();

  host_->DoBindBufferRange(kTarget, kIndex, buffer_.get(), kOffset, kSize);

  for (uint32_t index = 0; index < kMaxBindings; ++index) {
    if (index != kIndex) {
      EXPECT_EQ(nullptr, host_->GetBufferBinding(index));
    } else {
      EXPECT_EQ(buffer_.get(), host_->GetBufferBinding(index));
      EXPECT_EQ(kSize, host_->GetBufferSize(index));
      EXPECT_EQ(kOffset, host_->GetBufferStart(index));
    }
  }

  // Now adjust buffer size to be big enough.
  EXPECT_CALL(*gl_, BindBufferRange(kTarget, kIndex, kBufferServiceId, kOffset,
                                    kSize))
      .Times(1)
      .RetiresOnSaturation();

  SetBufferSize(kTarget, kOffset + kSize);
  host_->OnBufferData(kTarget, buffer_.get());
}

}  // namespace gles2
}  // namespace gpu


