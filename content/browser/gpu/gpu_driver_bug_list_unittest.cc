// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "content/browser/gpu/gpu_driver_bug_list.h"
#include "content/public/common/gpu_info.h"
#include "gpu/command_buffer/service/gpu_driver_bug_workaround_type.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kOsVersion[] = "10.6.4";

namespace content {

class GpuDriverBugListTest : public testing::Test {
 public:
  GpuDriverBugListTest() { }

  virtual ~GpuDriverBugListTest() { }

  const GPUInfo& gpu_info() const {
    return gpu_info_;
  }

  bool GetCurrentDriverBugList(std::string* json) {
    DCHECK(json);
    base::FilePath data_file;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &data_file))
      return false;
    data_file =
        data_file.Append(FILE_PATH_LITERAL("content"))
                 .Append(FILE_PATH_LITERAL("browser"))
                 .Append(FILE_PATH_LITERAL("gpu"))
                 .Append(FILE_PATH_LITERAL("gpu_driver_bug_list.json"));
    if (!file_util::PathExists(data_file))
      return false;
    int64 data_file_size64 = 0;
    if (!file_util::GetFileSize(data_file, &data_file_size64))
      return false;
    int data_file_size = static_cast<int>(data_file_size64);
    scoped_ptr<char[]> data(new char[data_file_size]);
    if (file_util::ReadFile(data_file, data.get(), data_file_size) !=
        data_file_size)
      return false;
    *json = std::string(data.get(), data_file_size);
    return true;
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
TEST_F(GpuDriverBugListTest, CurrentDriverBugListValidation) {
  scoped_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  std::string json;
  EXPECT_TRUE(GetCurrentDriverBugList(&json));
  EXPECT_TRUE(list->LoadList(json, GpuControlList::kAllOs));
  EXPECT_FALSE(list->contains_unknown_fields());
}

TEST_F(GpuDriverBugListTest, CurrentListForARM) {
  scoped_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  std::string json;
  EXPECT_TRUE(GetCurrentDriverBugList(&json));
  EXPECT_TRUE(list->LoadList(json, GpuControlList::kAllOs));

  GPUInfo gpu_info;
  gpu_info.gl_vendor = "ARM";
  gpu_info.gl_renderer = "MALi_T604";
  std::set<int> bugs = list->MakeDecision(
      GpuControlList::kOsAndroid, "4.1", gpu_info);
  EXPECT_EQ(1u, bugs.count(gpu::USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS));
}

TEST_F(GpuDriverBugListTest, CurrentListForImagination) {
  scoped_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  std::string json;
  EXPECT_TRUE(GetCurrentDriverBugList(&json));
  EXPECT_TRUE(list->LoadList(json, GpuControlList::kAllOs));

  GPUInfo gpu_info;
  gpu_info.gl_vendor = "Imagination Technologies";
  gpu_info.gl_renderer = "PowerVR SGX 540";
  std::set<int> bugs = list->MakeDecision(
      GpuControlList::kOsAndroid, "4.1", gpu_info);
  EXPECT_EQ(1u, bugs.count(gpu::USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS));
}
#endif

}  // namespace content

