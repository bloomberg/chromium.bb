// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "content/browser/gpu/gpu_switching_list.h"
#include "content/public/common/gpu_info.h"
#include "content/public/common/gpu_switching_option.h"
#include "testing/gtest/include/gtest/gtest.h"

#define LONG_STRING_CONST(...) #__VA_ARGS__

const char kOsVersion[] = "10.6.4";

namespace content {

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

#if !defined(OS_ANDROID)
TEST_F(GpuSwitchingListTest, CurrentSwitchingListValidation) {
  base::FilePath data_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_file));
  data_file =
      data_file.Append(FILE_PATH_LITERAL("content"))
               .Append(FILE_PATH_LITERAL("browser"))
               .Append(FILE_PATH_LITERAL("gpu"))
               .Append(FILE_PATH_LITERAL("gpu_switching_list.json"));
  ASSERT_TRUE(file_util::PathExists(data_file));
  int64 data_file_size64 = 0;
  ASSERT_TRUE(file_util::GetFileSize(data_file, &data_file_size64));
  int data_file_size = static_cast<int>(data_file_size64);
  scoped_array<char> data(new char[data_file_size]);
  ASSERT_EQ(data_file_size,
            file_util::ReadFile(data_file, data.get(), data_file_size));
  std::string json_string(data.get(), data_file_size);
  scoped_ptr<GpuSwitchingList> switching_list(GpuSwitchingList::Create());
  EXPECT_TRUE(switching_list->LoadList(json_string, GpuControlList::kAllOs));
  EXPECT_FALSE(switching_list->contains_unknown_fields());
}
#endif

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
  int switching = switching_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(GPU_SWITCHING_OPTION_FORCE_DISCRETE,
            static_cast<GpuSwitchingOption>(switching));
  std::vector<uint32> entries;
  switching_list->GetDecisionEntries(&entries, false);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(1u, entries[0]);

  switching_list.reset(GpuSwitchingList::Create());
  EXPECT_TRUE(switching_list->LoadList(json, GpuControlList::kAllOs));
  switching = switching_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(GPU_SWITCHING_OPTION_FORCE_INTEGRATED,
            static_cast<GpuSwitchingOption>(switching));
  switching_list->GetDecisionEntries(&entries, false);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(2u, entries[0]);
}

}  // namespace content

