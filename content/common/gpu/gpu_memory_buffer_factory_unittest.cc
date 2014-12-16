// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_memory_buffer_factory.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class GpuMemoryBufferFactoryTest
    : public testing::TestWithParam<gfx::GpuMemoryBufferType> {
 public:
  GpuMemoryBufferFactoryTest() : factory_(nullptr) {}

  // Overridden from testing::Test:
  void SetUp() override {
    factory_ = GpuMemoryBufferFactory::Create(GetParam());
    factory_->GetSupportedGpuMemoryBufferConfigurations(
        &supported_configurations_);
  }
  void TearDown() override { factory_.reset(); }

 protected:
  scoped_ptr<GpuMemoryBufferFactory> factory_;
  std::vector<GpuMemoryBufferFactory::Configuration> supported_configurations_;
};

TEST_P(GpuMemoryBufferFactoryTest, CreateAndDestroy) {
  const int kBufferId = 1;
  const int kClientId = 1;

  gfx::Size buffer_size(1, 1);

  for (auto configuration : supported_configurations_) {
    gfx::GpuMemoryBufferHandle handle = factory_->CreateGpuMemoryBuffer(
        kBufferId, buffer_size, configuration.format, configuration.usage,
        kClientId, gfx::kNullPluginWindow);
    EXPECT_EQ(handle.type, GetParam());
    factory_->DestroyGpuMemoryBuffer(kBufferId, kClientId);
  }
}

std::vector<gfx::GpuMemoryBufferType>
GetSupportedGpuMemoryBufferFactoryTypes() {
  std::vector<gfx::GpuMemoryBufferType> supported_types;
  GpuMemoryBufferFactory::GetSupportedTypes(&supported_types);
  return supported_types;
}

INSTANTIATE_TEST_CASE_P(
    GpuMemoryBufferFactoryTests,
    GpuMemoryBufferFactoryTest,
    ::testing::ValuesIn(GetSupportedGpuMemoryBufferFactoryTypes()));

}  // namespace
}  // namespace content
