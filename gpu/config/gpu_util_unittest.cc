// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_util.h"

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

TEST(GpuUtilTest, ParseSecondaryGpuDevicesFromCommandLine_Simple) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kGpuSecondaryVendorIDs, "0x10de");
  command_line.AppendSwitchASCII(switches::kGpuSecondaryDeviceIDs, "0x0de1");

  GPUInfo gpu_info;
  ParseSecondaryGpuDevicesFromCommandLine(command_line, &gpu_info);

  EXPECT_EQ(gpu_info.secondary_gpus.size(), 1ul);
  EXPECT_EQ(gpu_info.secondary_gpus[0].vendor_id, 0x10deul);
  EXPECT_EQ(gpu_info.secondary_gpus[0].device_id, 0x0de1ul);
}

TEST(GpuUtilTest, ParseSecondaryGpuDevicesFromCommandLine_Multiple) {
  std::vector<std::pair<uint32_t, uint32_t>> gpu_devices = {
      {0x10de, 0x0de1}, {0x1002, 0x6779}, {0x8086, 0x040a}};

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kGpuSecondaryVendorIDs,
                                 "0x10de;0x1002;0x8086");
  command_line.AppendSwitchASCII(switches::kGpuSecondaryDeviceIDs,
                                 "0x0de1;0x6779;0x040a");

  GPUInfo gpu_info;
  ParseSecondaryGpuDevicesFromCommandLine(command_line, &gpu_info);
  EXPECT_EQ(gpu_info.secondary_gpus.size(), 3ul);
  EXPECT_EQ(gpu_info.secondary_gpus.size(), gpu_devices.size());

  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    EXPECT_EQ(gpu_info.secondary_gpus[i].vendor_id, gpu_devices[i].first);
    EXPECT_EQ(gpu_info.secondary_gpus[i].device_id, gpu_devices[i].second);
  }
}

TEST(GpuUtilTest, ParseSecondaryGpuDevicesFromCommandLine_Generated) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  std::vector<std::pair<uint32_t, uint32_t>> gpu_devices;

  std::string vendor_ids_str;
  std::string device_ids_str;
  for (uint32_t i = 0x80000000; i > 1; i >>= 1) {
    gpu_devices.push_back(std::pair<uint32_t, uint32_t>(i, i + 1));

    if (!vendor_ids_str.empty())
      vendor_ids_str += ";";
    if (!device_ids_str.empty())
      device_ids_str += ";";
    vendor_ids_str += base::StringPrintf("0x%04x", i);
    device_ids_str += base::StringPrintf("0x%04x", i + 1);
  }

  command_line.AppendSwitchASCII(switches::kGpuSecondaryVendorIDs,
                                 vendor_ids_str);
  command_line.AppendSwitchASCII(switches::kGpuSecondaryDeviceIDs,
                                 device_ids_str);

  GPUInfo gpu_info;
  ParseSecondaryGpuDevicesFromCommandLine(command_line, &gpu_info);

  EXPECT_EQ(gpu_devices.size(), 31ul);
  EXPECT_EQ(gpu_info.secondary_gpus.size(), gpu_devices.size());

  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    EXPECT_EQ(gpu_info.secondary_gpus[i].vendor_id, gpu_devices[i].first);
    EXPECT_EQ(gpu_info.secondary_gpus[i].device_id, gpu_devices[i].second);
  }
}

TEST(GpuUtilTest, GetGpuFeatureInfo_WorkaroundFromCommandLine) {
  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    GPUInfo gpu_info;
    GpuFeatureInfo gpu_feature_info =
        ComputeGpuFeatureInfo(gpu_info, false, false, false, &command_line);
    EXPECT_FALSE(gpu_feature_info.IsWorkaroundEnabled(
        USE_GPU_DRIVER_WORKAROUND_FOR_TESTING));
  }

  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(GpuDriverBugWorkaroundTypeToString(
                                       USE_GPU_DRIVER_WORKAROUND_FOR_TESTING),
                                   "1");
    GPUInfo gpu_info;
    GpuFeatureInfo gpu_feature_info =
        ComputeGpuFeatureInfo(gpu_info, false, false, false, &command_line);
    EXPECT_TRUE(gpu_feature_info.IsWorkaroundEnabled(
        USE_GPU_DRIVER_WORKAROUND_FOR_TESTING));
  }

  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kGpuDriverBugListTestGroup, "1");
    // See gpu/config/gpu_driver_bug_list.json, test_group 1, entry 215.
    GPUInfo gpu_info;
    GpuFeatureInfo gpu_feature_info =
        ComputeGpuFeatureInfo(gpu_info, false, false, false, &command_line);
    EXPECT_TRUE(gpu_feature_info.IsWorkaroundEnabled(
        USE_GPU_DRIVER_WORKAROUND_FOR_TESTING));
  }

  {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kGpuDriverBugListTestGroup, "1");
    command_line.AppendSwitchASCII(GpuDriverBugWorkaroundTypeToString(
                                       USE_GPU_DRIVER_WORKAROUND_FOR_TESTING),
                                   "0");
    // See gpu/config/gpu_driver_bug_list.json, test_group 1, entry 215.
    GPUInfo gpu_info;
    GpuFeatureInfo gpu_feature_info =
        ComputeGpuFeatureInfo(gpu_info, false, false, false, &command_line);
    EXPECT_FALSE(gpu_feature_info.IsWorkaroundEnabled(
        USE_GPU_DRIVER_WORKAROUND_FOR_TESTING));
  }
}

}  // namespace gpu
