// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory_ozone_native_pixmap.h"
#include "content/test/gpu_memory_buffer_factory_test_template.h"

namespace content {
namespace {

INSTANTIATE_TYPED_TEST_CASE_P(GpuMemoryBufferFactoryOzoneNativePixmap,
                              GpuMemoryBufferFactoryTest,
                              GpuMemoryBufferFactoryOzoneNativePixmap);

INSTANTIATE_TYPED_TEST_CASE_P(GpuMemoryBufferFactoryOzoneNativePixmap,
                              GpuMemoryBufferFactoryImportTest,
                              GpuMemoryBufferFactoryOzoneNativePixmap);

}  // namespace
}  // namespace content
