// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_memory_buffer_impl.h"

#include "base/bind.h"
#include "content/common/gpu/gpu_memory_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"

namespace content {
namespace {

const int kClientId = 1;

class GpuMemoryBufferImplTest
    : public testing::TestWithParam<gfx::GpuMemoryBufferType> {
 public:
  GpuMemoryBufferImplTest() : buffer_count_(0), factory_(nullptr) {}

  // Overridden from testing::Test:
  void SetUp() override {
    factory_ = GpuMemoryBufferFactory::Create(GetParam());
    factory_->GetSupportedGpuMemoryBufferConfigurations(
        &supported_configurations_);
  }
  void TearDown() override { factory_.reset(); }

  gfx::GpuMemoryBufferHandle CreateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                                   const gfx::Size& size,
                                                   gfx::BufferFormat format,
                                                   gfx::BufferUsage usage) {
    ++buffer_count_;
    return factory_->CreateGpuMemoryBuffer(id, size, format, usage, kClientId,
                                           gfx::kNullPluginWindow);
  }

  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id, uint32 sync_point) {
    factory_->DestroyGpuMemoryBuffer(id, kClientId);
    DCHECK_GT(buffer_count_, 0);
    --buffer_count_;
  }

  std::vector<GpuMemoryBufferFactory::Configuration> supported_configurations_;
  int buffer_count_;

 private:
  scoped_ptr<GpuMemoryBufferFactory> factory_;
};

TEST_P(GpuMemoryBufferImplTest, CreateFromHandle) {
  const gfx::GpuMemoryBufferId kBufferId(1);

  gfx::Size buffer_size(8, 8);

  for (auto configuration : supported_configurations_) {
    scoped_ptr<GpuMemoryBufferImpl> buffer(
        GpuMemoryBufferImpl::CreateFromHandle(
            CreateGpuMemoryBuffer(kBufferId, buffer_size, configuration.format,
                                  configuration.usage),
            buffer_size, configuration.format, configuration.usage,
            base::Bind(&GpuMemoryBufferImplTest::DestroyGpuMemoryBuffer,
                       base::Unretained(this), kBufferId)));
    EXPECT_EQ(1, buffer_count_);
    ASSERT_TRUE(buffer);
    EXPECT_EQ(buffer->GetFormat(), configuration.format);

    // Check if destruction callback is executed when deleting the buffer.
    buffer.reset();
    EXPECT_EQ(0, buffer_count_);
  }
}

TEST_P(GpuMemoryBufferImplTest, Map) {
  const gfx::GpuMemoryBufferId kBufferId(1);

  // Use a multiple of 4 for both dimensions to support compressed formats.
  gfx::Size buffer_size(4, 4);

  for (auto configuration : supported_configurations_) {
    if (configuration.usage != gfx::BufferUsage::MAP)
      continue;

    scoped_ptr<GpuMemoryBufferImpl> buffer(
        GpuMemoryBufferImpl::CreateFromHandle(
            CreateGpuMemoryBuffer(kBufferId, buffer_size, configuration.format,
                                  configuration.usage),
            buffer_size, configuration.format, configuration.usage,
            base::Bind(&GpuMemoryBufferImplTest::DestroyGpuMemoryBuffer,
                       base::Unretained(this), kBufferId)));
    ASSERT_TRUE(buffer);
    EXPECT_FALSE(buffer->IsMapped());

    size_t num_planes =
        gfx::NumberOfPlanesForBufferFormat(configuration.format);

    // Map buffer into user space.
    scoped_ptr<void*[]> mapped_buffers(new void*[num_planes]);
    bool rv = buffer->Map(mapped_buffers.get());
    ASSERT_TRUE(rv);
    EXPECT_TRUE(buffer->IsMapped());

    // Get strides.
    scoped_ptr<int[]> strides(new int[num_planes]);
    buffer->GetStride(strides.get());

    // Copy and compare mapped buffers.
    for (size_t plane = 0; plane < num_planes; ++plane) {
      size_t row_size_in_bytes = 0;
      EXPECT_TRUE(GpuMemoryBufferImpl::RowSizeInBytes(
          buffer_size.width(), configuration.format, plane,
          &row_size_in_bytes));
      EXPECT_GT(row_size_in_bytes, 0u);

      scoped_ptr<char[]> data(new char[row_size_in_bytes]);
      memset(data.get(), 0x2a + plane, row_size_in_bytes);

      size_t height =
          buffer_size.height() /
          GpuMemoryBufferImpl::SubsamplingFactor(configuration.format, plane);
      for (size_t y = 0; y < height; ++y) {
        memcpy(static_cast<char*>(mapped_buffers[plane]) + y * strides[plane],
               data.get(), row_size_in_bytes);
        EXPECT_EQ(memcmp(static_cast<char*>(mapped_buffers[plane]) +
                             y * strides[plane],
                         data.get(), row_size_in_bytes),
                  0);
      }
    }

    buffer->Unmap();
    EXPECT_FALSE(buffer->IsMapped());
  }
}

TEST_P(GpuMemoryBufferImplTest, PersistentMap) {
  const gfx::GpuMemoryBufferId kBufferId(1);

  // Use a multiple of 4 for both dimensions to support compressed formats.
  gfx::Size buffer_size(4, 4);

  for (auto configuration : supported_configurations_) {
    if (configuration.usage != gfx::BufferUsage::PERSISTENT_MAP)
      continue;

    scoped_ptr<GpuMemoryBufferImpl> buffer(
        GpuMemoryBufferImpl::CreateFromHandle(
            CreateGpuMemoryBuffer(kBufferId, buffer_size, configuration.format,
                                  configuration.usage),
            buffer_size, configuration.format, configuration.usage,
            base::Bind(&GpuMemoryBufferImplTest::DestroyGpuMemoryBuffer,
                       base::Unretained(this), kBufferId)));
    ASSERT_TRUE(buffer);
    EXPECT_FALSE(buffer->IsMapped());

    size_t num_planes =
        gfx::NumberOfPlanesForBufferFormat(configuration.format);

    // Map buffer into user space.
    scoped_ptr<void* []> mapped_buffers(new void* [num_planes]);
    bool rv = buffer->Map(mapped_buffers.get());
    ASSERT_TRUE(rv);
    EXPECT_TRUE(buffer->IsMapped());

    // Get strides.
    scoped_ptr<int[]> strides(new int[num_planes]);
    buffer->GetStride(strides.get());

    // Copy and compare mapped buffers.
    for (size_t plane = 0; plane < num_planes; ++plane) {
      size_t row_size_in_bytes;
      EXPECT_TRUE(GpuMemoryBufferImpl::RowSizeInBytes(
          buffer_size.width(), configuration.format, plane,
          &row_size_in_bytes));

      scoped_ptr<char[]> data(new char[row_size_in_bytes]);
      memset(data.get(), 0x2a + plane, row_size_in_bytes);

      size_t height =
          buffer_size.height() /
          GpuMemoryBufferImpl::SubsamplingFactor(configuration.format, plane);
      for (size_t y = 0; y < height; ++y) {
        memcpy(static_cast<char*>(mapped_buffers[plane]) + y * strides[plane],
               data.get(), row_size_in_bytes);
        EXPECT_EQ(memcmp(static_cast<char*>(mapped_buffers[plane]) +
                             y * strides[plane],
                         data.get(), row_size_in_bytes),
                  0);
      }
    }

    buffer->Unmap();
    EXPECT_FALSE(buffer->IsMapped());

    // Remap the buffer, and compare again. It should contain the same data.
    rv = buffer->Map(mapped_buffers.get());
    ASSERT_TRUE(rv);
    EXPECT_TRUE(buffer->IsMapped());

    buffer->GetStride(strides.get());

    for (size_t plane = 0; plane < num_planes; ++plane) {
      size_t row_size_in_bytes;
      EXPECT_TRUE(GpuMemoryBufferImpl::RowSizeInBytes(
          buffer_size.width(), configuration.format, plane,
          &row_size_in_bytes));

      scoped_ptr<char[]> data(new char[row_size_in_bytes]);
      memset(data.get(), 0x2a + plane, row_size_in_bytes);

      size_t height =
          buffer_size.height() /
          GpuMemoryBufferImpl::SubsamplingFactor(configuration.format, plane);
      for (size_t y = 0; y < height; ++y) {
        EXPECT_EQ(memcmp(static_cast<char*>(mapped_buffers[plane]) +
                             y * strides[plane],
                         data.get(), row_size_in_bytes),
                  0);
      }
    }

    buffer->Unmap();
    EXPECT_FALSE(buffer->IsMapped());
  }
}

std::vector<gfx::GpuMemoryBufferType> GetSupportedGpuMemoryBufferTypes() {
  std::vector<gfx::GpuMemoryBufferType> supported_types;
  GpuMemoryBufferFactory::GetSupportedTypes(&supported_types);
  return supported_types;
}

INSTANTIATE_TEST_CASE_P(
    GpuMemoryBufferImplTests,
    GpuMemoryBufferImplTest,
    ::testing::ValuesIn(GetSupportedGpuMemoryBufferTypes()));

}  // namespace
}  // namespace content
