// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "gpu/config/gpu_blacklist.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GpuBlacklistTest : public testing::Test {
 public:
  GpuBlacklistTest() = default;
  ~GpuBlacklistTest() override = default;

  const GPUInfo& gpu_info() const {
    return gpu_info_;
  }

  void RunFeatureTest(GpuFeatureType feature_type) {
    const int kFeatureListForEntry1[1] = {feature_type};
    const uint32_t kDeviceIDsForEntry1[1] = {0x0640};
    const GpuControlList::Entry kTestEntries[1] = {{
        1,                      // id
        "Test entry",           // description
        1,                      // features size
        kFeatureListForEntry1,  // features
        0,                      // DisabledExtensions size
        nullptr,                // DisabledExtensions
        0,                      // DisabledWebGLExtensions size
        nullptr,                // DisabledWebGLExtensions
        0,                      // CrBugs size
        nullptr,                // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             nullptr, nullptr},                    // os_version
            0x10de,                                // vendor_id
            1,                                     // DeviceIDs size
            kDeviceIDsForEntry1,                   // DeviceIDs
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            0,                                     // gpu_series size
            nullptr,                               // gpu_series
            nullptr,                               // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    }};
    GpuControlListData data(1, kTestEntries);
    std::unique_ptr<GpuBlacklist> blacklist = GpuBlacklist::Create(data);
    std::set<int> type =
        blacklist->MakeDecision(GpuBlacklist::kOsMacosx, "10.12.3", gpu_info());
    EXPECT_EQ(1u, type.size());
    EXPECT_EQ(1u, type.count(feature_type));
  }

 protected:
  void SetUp() override {
    gpu_info_.gpu.vendor_id = 0x10de;
    gpu_info_.gpu.device_id = 0x0640;
    gpu_info_.driver_vendor = "NVIDIA";
    gpu_info_.driver_version = "1.6.18";
    gpu_info_.driver_date = "7-14-2009";
    gpu_info_.machine_model_name = "MacBookPro";
    gpu_info_.machine_model_version = "7.1";
    gpu_info_.gl_vendor = "NVIDIA Corporation";
    gpu_info_.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
  }

 private:
  GPUInfo gpu_info_;
};

#define GPU_BLACKLIST_FEATURE_TEST(test_name, feature_type) \
  TEST_F(GpuBlacklistTest, test_name) { RunFeatureTest(feature_type); }

GPU_BLACKLIST_FEATURE_TEST(Accelerated2DCanvas,
                           GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS)

GPU_BLACKLIST_FEATURE_TEST(GpuCompositing,
                           GPU_FEATURE_TYPE_GPU_COMPOSITING)

GPU_BLACKLIST_FEATURE_TEST(AcceleratedWebGL, GPU_FEATURE_TYPE_ACCELERATED_WEBGL)

GPU_BLACKLIST_FEATURE_TEST(Flash3D,
                           GPU_FEATURE_TYPE_FLASH3D)

GPU_BLACKLIST_FEATURE_TEST(FlashStage3D,
                           GPU_FEATURE_TYPE_FLASH_STAGE3D)

GPU_BLACKLIST_FEATURE_TEST(FlashStage3DBaseline,
                           GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE)

GPU_BLACKLIST_FEATURE_TEST(AcceleratedVideoDecode,
                           GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE)

GPU_BLACKLIST_FEATURE_TEST(GpuRasterization,
                           GPU_FEATURE_TYPE_GPU_RASTERIZATION)

GPU_BLACKLIST_FEATURE_TEST(WebGL2,
                           GPU_FEATURE_TYPE_ACCELERATED_WEBGL2)

GPU_BLACKLIST_FEATURE_TEST(ProtectedVideoDecode,
                           GPU_FEATURE_TYPE_PROTECTED_VIDEO_DECODE)

// Test for invariant "Assume the newly last added entry has the largest ID".
// See GpuControlList::GpuControlList.
// It checks software_rendering_list.json
TEST_F(GpuBlacklistTest, TestBlacklistIsValid) {
  std::unique_ptr<GpuBlacklist> list(GpuBlacklist::Create());
  uint32_t max_entry_id = list->max_entry_id();

  std::vector<uint32_t> indices(list->num_entries());
  int current = 0;
  std::generate(indices.begin(), indices.end(),
                [&current]() { return current++; });

  auto entries = list->GetEntryIDsFromIndices(indices);
  auto real_max_entry_id = *std::max_element(entries.begin(), entries.end());
  EXPECT_EQ(real_max_entry_id, max_entry_id);
}

}  // namespace gpu
