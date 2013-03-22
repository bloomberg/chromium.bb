// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "content/browser/gpu/gpu_driver_bug_list.h"
#include "content/public/common/gpu_info.h"
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
  base::FilePath data_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_file));
  data_file =
      data_file.Append(FILE_PATH_LITERAL("content"))
               .Append(FILE_PATH_LITERAL("browser"))
               .Append(FILE_PATH_LITERAL("gpu"))
               .Append(FILE_PATH_LITERAL("gpu_driver_bug_list.json"));
  ASSERT_TRUE(file_util::PathExists(data_file));
  int64 data_file_size64 = 0;
  ASSERT_TRUE(file_util::GetFileSize(data_file, &data_file_size64));
  int data_file_size = static_cast<int>(data_file_size64);
  scoped_array<char> data(new char[data_file_size]);
  ASSERT_EQ(data_file_size,
            file_util::ReadFile(data_file, data.get(), data_file_size));
  std::string json_string(data.get(), data_file_size);
  scoped_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  EXPECT_TRUE(list->LoadList(json_string, GpuControlList::kAllOs));
  EXPECT_FALSE(list->contains_unknown_fields());
}
#endif

}  // namespace content

