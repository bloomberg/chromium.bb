// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "gpu/config/gpu_control_list_jsons.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_switching_list.h"
#include "gpu/config/gpu_switching_option.h"
#include "testing/gtest/include/gtest/gtest.h"

#define LONG_STRING_CONST(...) #__VA_ARGS__

const char kOsVersion[] = "10.6.4";

namespace gpu {

class GpuSwitchingListTest : public testing::Test {
 public:
  GpuSwitchingListTest() { }

  virtual ~GpuSwitchingListTest() { }

  const GPUInfo& gpu_info() const {
    return gpu_info_;
  }

 protected:
  virtual void SetUp() {
    gpu_info_.gpu.vendor_id = 0x10de;
    gpu_info_.gpu.device_id = 0x0640;
    gpu_info_.driver_vendor = "NVIDIA";
    gpu_info_.driver_version = "1.6.18";
    gpu_info_.driver_date = "7-14-2009";
    gpu_info_.machine_model = "MacBookPro 7.1";
    gpu_info_.gl_vendor = "NVIDIA Corporation";
    gpu_info_.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
    gpu_info_.performance_stats.graphics = 5.0;
    gpu_info_.performance_stats.gaming = 5.0;
    gpu_info_.performance_stats.overall = 5.0;
  }

  virtual void TearDown() {
  }

 private:
  GPUInfo gpu_info_;
};

TEST_F(GpuSwitchingListTest, CurrentSwitchingListValidation) {
  scoped_ptr<GpuSwitchingList> switching_list(GpuSwitchingList::Create());
  EXPECT_TRUE(switching_list->LoadList(
      kGpuSwitchingListJson, GpuControlList::kAllOs));
  EXPECT_FALSE(switching_list->contains_unknown_fields());
}

TEST_F(GpuSwitchingListTest, GpuSwitching) {
  const std::string json = LONG_STRING_CONST(
      {
        "name": "gpu switching list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "os": {
              "type": "macosx"
            },
            "features": [
              "force_discrete"
            ]
          },
          {
            "id": 2,
            "os": {
              "type": "win"
            },
            "features": [
              "force_integrated"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuSwitchingList> switching_list(GpuSwitchingList::Create());
  EXPECT_TRUE(switching_list->LoadList(json, GpuControlList::kAllOs));
  std::set<int> switching = switching_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(1u, switching.size());
  EXPECT_EQ(1u, switching.count(GPU_SWITCHING_OPTION_FORCE_DISCRETE));
  std::vector<uint32> entries;
  switching_list->GetDecisionEntries(&entries, false);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(1u, entries[0]);

  switching_list.reset(GpuSwitchingList::Create());
  EXPECT_TRUE(switching_list->LoadList(json, GpuControlList::kAllOs));
  switching = switching_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(1u, switching.size());
  EXPECT_EQ(1u, switching.count(GPU_SWITCHING_OPTION_FORCE_INTEGRATED));
  switching_list->GetDecisionEntries(&entries, false);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(2u, entries[0]);
}

}  // namespace gpu

