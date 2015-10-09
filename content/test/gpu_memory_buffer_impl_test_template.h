// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines tests that implementations of GpuMemoryBufferFactory should
// pass in order to be conformant.

#ifndef CONTENT_TEST_GPU_MEMORY_BUFFER_IMPL_TEST_TEMPLATE_H_
#define CONTENT_TEST_GPU_MEMORY_BUFFER_IMPL_TEST_TEMPLATE_H_

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"

namespace content {

template <typename GpuMemoryBufferImplType>
class GpuMemoryBufferImplTest : public testing::Test {
 public:
  GpuMemoryBufferImpl::DestructionCallback AllocateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gfx::GpuMemoryBufferHandle* handle,
      bool* destroyed) {
    return base::Bind(&GpuMemoryBufferImplTest::FreeGpuMemoryBuffer,
                      base::Unretained(this),
                      GpuMemoryBufferImplType::AllocateForTesting(
                          size, format, usage, handle),
                      base::Unretained(destroyed));
  }

 private:
  void FreeGpuMemoryBuffer(const base::Closure& free_callback,
                           bool* destroyed,
                           uint32 sync_point) {
    free_callback.Run();
    if (destroyed)
      *destroyed = true;
  }
};

TYPED_TEST_CASE_P(GpuMemoryBufferImplTest);

TYPED_TEST_P(GpuMemoryBufferImplTest, CreateFromHandle) {
  gfx::Size buffer_size(8, 8);

  for (auto format : gfx::GetBufferFormats()) {
    gfx::BufferUsage usages[] = {gfx::BufferUsage::MAP,
                                 gfx::BufferUsage::PERSISTENT_MAP,
                                 gfx::BufferUsage::SCANOUT};
    for (auto usage : usages) {
      if (!TypeParam::IsConfigurationSupported(format, usage))
        continue;

      bool destroyed = false;
      gfx::GpuMemoryBufferHandle handle;
      GpuMemoryBufferImpl::DestructionCallback destroy_callback =
          TestFixture::AllocateGpuMemoryBuffer(
              buffer_size, format, gfx::BufferUsage::MAP, &handle, &destroyed);
      scoped_ptr<TypeParam> buffer(TypeParam::CreateFromHandle(
          handle, buffer_size, format, usage, destroy_callback));
      ASSERT_TRUE(buffer);
      EXPECT_EQ(buffer->GetFormat(), format);

      // Check if destruction callback is executed when deleting the buffer.
      buffer.reset();
      ASSERT_TRUE(destroyed);
    }
  }
}

TYPED_TEST_P(GpuMemoryBufferImplTest, Map) {
  // Use a multiple of 4 for both dimensions to support compressed formats.
  gfx::Size buffer_size(4, 4);

  for (auto format : gfx::GetBufferFormats()) {
    if (!TypeParam::IsConfigurationSupported(format, gfx::BufferUsage::MAP))
      continue;

    gfx::GpuMemoryBufferHandle handle;
    GpuMemoryBufferImpl::DestructionCallback destroy_callback =
        TestFixture::AllocateGpuMemoryBuffer(
            buffer_size, format, gfx::BufferUsage::MAP, &handle, nullptr);
    scoped_ptr<TypeParam> buffer(TypeParam::CreateFromHandle(
        handle, buffer_size, format, gfx::BufferUsage::MAP, destroy_callback));
    ASSERT_TRUE(buffer);

    size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format);

    // Map buffer into user space.
    scoped_ptr<void* []> mapped_buffers(new void*[num_planes]);
    bool rv = buffer->Map(mapped_buffers.get());
    ASSERT_TRUE(rv);

    // Get strides.
    scoped_ptr<int[]> strides(new int[num_planes]);
    buffer->GetStride(strides.get());

    // Copy and compare mapped buffers.
    for (size_t plane = 0; plane < num_planes; ++plane) {
      size_t row_size_in_bytes = 0;
      EXPECT_TRUE(gfx::RowSizeForBufferFormatChecked(
          buffer_size.width(), format, plane, &row_size_in_bytes));
      EXPECT_GT(row_size_in_bytes, 0u);

      scoped_ptr<char[]> data(new char[row_size_in_bytes]);
      memset(data.get(), 0x2a + plane, row_size_in_bytes);

      size_t height = buffer_size.height() /
                      gfx::SubsamplingFactorForBufferFormat(format, plane);
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
  }
}

TYPED_TEST_P(GpuMemoryBufferImplTest, PersistentMap) {
  // Use a multiple of 4 for both dimensions to support compressed formats.
  gfx::Size buffer_size(4, 4);

  for (auto format : gfx::GetBufferFormats()) {
    if (!TypeParam::IsConfigurationSupported(
            format, gfx::BufferUsage::PERSISTENT_MAP)) {
      continue;
    }

    gfx::GpuMemoryBufferHandle handle;
    GpuMemoryBufferImpl::DestructionCallback destroy_callback =
        TestFixture::AllocateGpuMemoryBuffer(buffer_size, format,
                                             gfx::BufferUsage::PERSISTENT_MAP,
                                             &handle, nullptr);
    scoped_ptr<TypeParam> buffer(TypeParam::CreateFromHandle(
        handle, buffer_size, format, gfx::BufferUsage::PERSISTENT_MAP,
        destroy_callback));
    ASSERT_TRUE(buffer);

    size_t num_planes = gfx::NumberOfPlanesForBufferFormat(format);

    // Map buffer into user space.
    scoped_ptr<void* []> mapped_buffers(new void*[num_planes]);
    bool rv = buffer->Map(mapped_buffers.get());
    ASSERT_TRUE(rv);

    // Get strides.
    scoped_ptr<int[]> strides(new int[num_planes]);
    buffer->GetStride(strides.get());

    // Copy and compare mapped buffers.
    for (size_t plane = 0; plane < num_planes; ++plane) {
      size_t row_size_in_bytes;
      EXPECT_TRUE(gfx::RowSizeForBufferFormatChecked(
          buffer_size.width(), format, plane, &row_size_in_bytes));
      EXPECT_GT(row_size_in_bytes, 0u);

      scoped_ptr<char[]> data(new char[row_size_in_bytes]);
      memset(data.get(), 0x2a + plane, row_size_in_bytes);

      size_t height = buffer_size.height() /
                      gfx::SubsamplingFactorForBufferFormat(format, plane);
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

    // Remap the buffer, and compare again. It should contain the same data.
    rv = buffer->Map(mapped_buffers.get());
    ASSERT_TRUE(rv);

    buffer->GetStride(strides.get());

    for (size_t plane = 0; plane < num_planes; ++plane) {
      size_t row_size_in_bytes;
      EXPECT_TRUE(gfx::RowSizeForBufferFormatChecked(
          buffer_size.width(), format, plane, &row_size_in_bytes));

      scoped_ptr<char[]> data(new char[row_size_in_bytes]);
      memset(data.get(), 0x2a + plane, row_size_in_bytes);

      size_t height = buffer_size.height() /
                      gfx::SubsamplingFactorForBufferFormat(format, plane);
      for (size_t y = 0; y < height; ++y) {
        EXPECT_EQ(memcmp(static_cast<char*>(mapped_buffers[plane]) +
                             y * strides[plane],
                         data.get(), row_size_in_bytes),
                  0);
      }
    }

    buffer->Unmap();
  }
}

// The GpuMemoryBufferImplTest test case verifies behavior that is expected
// from a GpuMemoryBuffer implementation in order to be conformant.
REGISTER_TYPED_TEST_CASE_P(GpuMemoryBufferImplTest,
                           CreateFromHandle,
                           Map,
                           PersistentMap);

}  // namespace content

#endif  // CONTENT_TEST_GPU_MEMORY_BUFFER_IMPL_TEST_TEMPLATE_H_
