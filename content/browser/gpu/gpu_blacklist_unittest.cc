// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/public/common/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kOsVersion[] = "10.6.4";
const uint32 kIntelVendorId = 0x8086;
const uint32 kIntelDeviceId = 0x0166;  // 3rd Gen Core Graphics
const uint32 kNvidiaVendorId = 0x10de;
const uint32 kNvidiaDeviceId = 0x0fd5;  // GeForce GT 650M

namespace content {

class GpuBlacklistTest : public testing::Test {
 public:
  GpuBlacklistTest() { }

  virtual ~GpuBlacklistTest() { }

  const GPUInfo& gpu_info() const {
    return gpu_info_;
  }

  GpuBlacklist* Create() {
    GpuBlacklist* rt = new GpuBlacklist();
    return rt;
  }

 protected:
  virtual void SetUp() {
    gpu_info_.gpu.vendor_id = kNvidiaVendorId;
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

TEST_F(GpuBlacklistTest, CurrentBlacklistValidation) {
  base::FilePath data_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_file));
  data_file =
      data_file.Append(FILE_PATH_LITERAL("content"))
               .Append(FILE_PATH_LITERAL("browser"))
               .Append(FILE_PATH_LITERAL("gpu"))
               .Append(FILE_PATH_LITERAL("software_rendering_list.json"));
  ASSERT_TRUE(file_util::PathExists(data_file));
  int64 data_file_size64 = 0;
  ASSERT_TRUE(file_util::GetFileSize(data_file, &data_file_size64));
  int data_file_size = static_cast<int>(data_file_size64);
  scoped_array<char> data(new char[data_file_size]);
  ASSERT_EQ(data_file_size,
            file_util::ReadFile(data_file, data.get(), data_file_size));
  std::string json_string(data.get(), data_file_size);
  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(json_string, GpuBlacklist::kAllOs));
  EXPECT_FALSE(blacklist->contains_unknown_fields());
}

TEST_F(GpuBlacklistTest, DefaultBlacklistSettings) {
  scoped_ptr<GpuBlacklist> blacklist(Create());
  // Default blacklist settings: all feature are allowed.
  GpuBlacklist::Decision decision = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(0, decision.blacklisted_features);
  EXPECT_EQ(GPU_SWITCHING_OPTION_UNKNOWN, decision.gpu_switching);
}

TEST_F(GpuBlacklistTest, EmptyBlacklist) {
  // Empty list: all features are allowed.
  const std::string empty_list_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"2.5\",\n"
      "  \"entries\": [\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(empty_list_json,
                                          GpuBlacklist::kAllOs));
  EXPECT_EQ("2.5", blacklist->GetVersion());
  GpuBlacklist::Decision decision = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(0, decision.blacklisted_features);
  EXPECT_EQ(GPU_SWITCHING_OPTION_UNKNOWN, decision.gpu_switching);
}

TEST_F(GpuBlacklistTest, DetailedEntryAndInvalidJson) {
  // Blacklist accelerated_compositing with exact setting.
  const std::string exact_list_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 5,\n"
      "      \"os\": {\n"
      "        \"type\": \"macosx\",\n"
      "        \"version\": {\n"
      "          \"op\": \"=\",\n"
      "          \"number\": \"10.6.4\"\n"
      "        }\n"
      "      },\n"
      "      \"vendor_id\": \"0x10de\",\n"
      "      \"device_id\": [\"0x0640\"],\n"
      "      \"driver_version\": {\n"
      "        \"op\": \"=\",\n"
      "        \"number\": \"1.6.18\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_compositing\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(exact_list_json,
                                          GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING, type);

  // Invalid json input should not change the current blacklist settings.
  const std::string invalid_json = "invalid";

  EXPECT_FALSE(blacklist->LoadGpuBlacklist(invalid_json, GpuBlacklist::kAllOs));
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING, type);
  std::vector<uint32> entries;
  blacklist->GetDecisionEntries(&entries, false);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(5u, entries[0]);
  EXPECT_EQ(5u, blacklist->max_entry_id());
}

TEST_F(GpuBlacklistTest, VendorOnAllOsEntry) {
  // Blacklist a vendor on all OS.
  const std::string vendor_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"vendor_id\": \"0x10de\",\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  // Blacklist entries won't be filtered to the current OS only upon loading.
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(vendor_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_OPENBSD)
  // Blacklist entries will be filtered to the current OS only upon loading.
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(vendor_json,
                                          GpuBlacklist::kCurrentOsOnly));
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
#endif
}

TEST_F(GpuBlacklistTest, VendorOnLinuxEntry) {
  // Blacklist a vendor on Linux only.
  const std::string vendor_linux_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x10de\",\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_2d_canvas\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(vendor_linux_json,
                                          GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(0, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(0, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, type);
}

TEST_F(GpuBlacklistTest, AllExceptNVidiaOnLinuxEntry) {
  // Blacklist all cards in Linux except NVIDIA.
  const std::string linux_except_nvidia_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"exceptions\": [\n"
      "        {\n"
      "          \"vendor_id\": \"0x10de\"\n"
      "        }\n"
      "      ],\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_2d_canvas\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(linux_except_nvidia_json,
                                          GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(0, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(0, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(0, type);
}

TEST_F(GpuBlacklistTest, AllExceptIntelOnLinuxEntry) {
  // Blacklist all cards in Linux except Intel.
  const std::string linux_except_intel_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"exceptions\": [\n"
      "        {\n"
      "          \"vendor_id\": \"0x8086\"\n"
      "        }\n"
      "      ],\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_2d_canvas\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(linux_except_intel_json,
                                          GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(0, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(0, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, type);
}

TEST_F(GpuBlacklistTest, DateOnWindowsEntry) {
  // Blacklist all drivers earlier than 2010-5-8 in Windows.
  const std::string date_windows_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"win\"\n"
      "      },\n"
      "      \"driver_date\": {\n"
      "        \"op\": \"<\",\n"
      "        \"number\": \"2010.5.8\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_2d_canvas\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  GPUInfo gpu_info;
  gpu_info.driver_date = "7-14-2009";

  EXPECT_TRUE(
      blacklist->LoadGpuBlacklist(date_windows_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, type);

  gpu_info.driver_date = "07-14-2009";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, type);

  gpu_info.driver_date = "1-1-2010";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, type);

  gpu_info.driver_date = "05-07-2010";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS, type);

  gpu_info.driver_date = "5-8-2010";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_date = "5-9-2010";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_date = "6-2-2010";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);
}

TEST_F(GpuBlacklistTest, MultipleDevicesEntry) {
  const std::string devices_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"vendor_id\": \"0x10de\",\n"
      "      \"device_id\": [\"0x1023\", \"0x0640\"],\n"
      "      \"blacklist\": [\n"
      "        \"multisampling\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(devices_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_MULTISAMPLING, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_MULTISAMPLING, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_MULTISAMPLING, type);
}

TEST_F(GpuBlacklistTest, ChromeOSEntry) {
  const std::string devices_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"chromeos\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(devices_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsChromeOS, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(0, type);
}

TEST_F(GpuBlacklistTest, ChromeVersionEntry) {
  const std::string browser_version_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"browser_version\": {\n"
      "        \"op\": \">=\",\n"
      "        \"number\": \"10\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  scoped_ptr<GpuBlacklist> blacklist9(Create());
  EXPECT_TRUE(blacklist9->LoadGpuBlacklist("9.0", browser_version_json,
                                           GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist9->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(0, type);

  scoped_ptr<GpuBlacklist> blacklist10(Create());
  EXPECT_TRUE(blacklist10->LoadGpuBlacklist("10.0", browser_version_json,
                                            GpuBlacklist::kAllOs));
  type = blacklist10->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, MalformedVendor) {
  // vendor_id is defined as list instead of string.
  const std::string malformed_vendor_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"vendor_id\": \"[0x10de]\",\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_2d_canvas\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_FALSE(blacklist->LoadGpuBlacklist(malformed_vendor_json,
                                           GpuBlacklist::kAllOs));
}

TEST_F(GpuBlacklistTest, UnknownField) {
  const std::string unknown_field_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"unknown_field\": 0,\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_2d_canvas\"\n"
      "      ]\n"
      "    },\n"
      "    {\n"
      "      \"id\": 2,\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(unknown_field_json,
                                          GpuBlacklist::kAllOs));
  EXPECT_EQ(1u, blacklist->num_entries());
  EXPECT_TRUE(blacklist->contains_unknown_fields());
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, UnknownExceptionField) {
  const std::string unknown_exception_field_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"unknown_field\": 0,\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_compositing\"\n"
      "      ]\n"
      "    },\n"
      "    {\n"
      "      \"id\": 2,\n"
      "      \"exceptions\": [\n"
      "        {\n"
      "          \"unknown_field\": 0\n"
      "        }\n"
      "      ],\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_2d_canvas\"\n"
      "      ]\n"
      "    },\n"
      "    {\n"
      "      \"id\": 3,\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(unknown_exception_field_json,
                                          GpuBlacklist::kAllOs));
  EXPECT_EQ(1u, blacklist->num_entries());
  EXPECT_TRUE(blacklist->contains_unknown_fields());
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, UnknownFeature) {
  const std::string unknown_feature_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_something\",\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(unknown_feature_json,
                                          GpuBlacklist::kAllOs));
  EXPECT_EQ(1u, blacklist->num_entries());
  EXPECT_TRUE(blacklist->contains_unknown_fields());
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, GlVendor) {
  const std::string gl_vendor_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"gl_vendor\": {\n"
      "        \"op\": \"beginwith\",\n"
      "        \"value\": \"NVIDIA\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(gl_vendor_json,
                                          GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, GlRenderer) {
  const std::string gl_renderer_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"gl_renderer\": {\n"
      "        \"op\": \"contains\",\n"
      "        \"value\": \"GeForce\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(gl_renderer_json,
                                          GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, PerfGraphics) {
  const std::string json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"perf_graphics\": {\n"
      "        \"op\": \"<\",\n"
      "        \"value\": \"6.0\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, PerfGaming) {
  const std::string json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"perf_gaming\": {\n"
      "        \"op\": \"<=\",\n"
      "        \"value\": \"4.0\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(0, type);
}

TEST_F(GpuBlacklistTest, PerfOverall) {
  const std::string json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"perf_overall\": {\n"
      "        \"op\": \"between\",\n"
      "        \"value\": \"1.0\",\n"
      "        \"value2\": \"9.0\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, DisabledEntry) {
  const std::string disabled_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"disabled\": true,\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(disabled_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
  std::vector<uint32> flag_entries;
  blacklist->GetDecisionEntries(&flag_entries, false);
  EXPECT_EQ(0u, flag_entries.size());
  blacklist->GetDecisionEntries(&flag_entries, true);
  EXPECT_EQ(1u, flag_entries.size());
}

TEST_F(GpuBlacklistTest, Optimus) {
  const std::string optimus_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"multi_gpu_style\": \"optimus\",\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  GPUInfo gpu_info;
  gpu_info.optimus = true;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(optimus_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, AMDSwitchable) {
  const std::string amd_switchable_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"macosx\"\n"
      "      },\n"
      "      \"multi_gpu_style\": \"amd_switchable\",\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  GPUInfo gpu_info;
  gpu_info.amd_switchable = true;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(amd_switchable_json,
                                          GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, LexicalDriverVersion) {
  const std::string lexical_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x1002\",\n"
      "      \"driver_version\": {\n"
      "        \"op\": \"<\",\n"
      "        \"style\": \"lexical\",\n"
      "        \"number\": \"8.201\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(lexical_json, GpuBlacklist::kAllOs));

  gpu_info.driver_version = "8.001.100";
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.109";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.10900";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.109.100";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.2";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.20";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.200";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.20.100";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.201";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_version = "8.2010";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_version = "8.21";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_version = "8.21.100";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_version = "9.002";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_version = "9.201";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_version = "12";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_version = "12.201";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);
}

TEST_F(GpuBlacklistTest, LexicalDriverVersion2) {
  const std::string lexical_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x1002\",\n"
      "      \"driver_version\": {\n"
      "        \"op\": \"<\",\n"
      "        \"style\": \"lexical\",\n"
      "        \"number\": \"9.002\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(lexical_json, GpuBlacklist::kAllOs));

  gpu_info.driver_version = "8.001.100";
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.109";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.10900";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.109.100";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.2";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.20";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.200";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.20.100";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.201";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.2010";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.21";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.21.100";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "9.002";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_version = "9.201";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_version = "12";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  gpu_info.driver_version = "12.201";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);
}

TEST_F(GpuBlacklistTest, LexicalDriverVersion3) {
  const std::string lexical_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x1002\",\n"
      "      \"driver_version\": {\n"
      "        \"op\": \"=\",\n"
      "        \"style\": \"lexical\",\n"
      "        \"number\": \"8.76\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(lexical_json, GpuBlacklist::kAllOs));

  gpu_info.driver_version = "8.76";
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.768";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);

  gpu_info.driver_version = "8.76.8";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, MultipleGPUsAny) {
  const std::string multi_gpu_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"macosx\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x8086\",\n"
      "      \"device_id\": [\"0x0166\"],\n"
      "      \"multi_gpu_category\": \"any\",\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kNvidiaVendorId;
  gpu_info.gpu.device_id = kNvidiaDeviceId;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(multi_gpu_json,
                                          GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  GPUInfo::GPUDevice gpu_device;
  gpu_device.vendor_id = kIntelVendorId;
  gpu_device.device_id = kIntelDeviceId;
  gpu_info.secondary_gpus.push_back(gpu_device);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, MultipleGPUsSecondary) {
  const std::string multi_gpu_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"macosx\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x8086\",\n"
      "      \"device_id\": [\"0x0166\"],\n"
      "      \"multi_gpu_category\": \"secondary\",\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kNvidiaVendorId;
  gpu_info.gpu.device_id = kNvidiaDeviceId;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(multi_gpu_json,
                                          GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);

  GPUInfo::GPUDevice gpu_device;
  gpu_device.vendor_id = kIntelVendorId;
  gpu_device.device_id = kIntelDeviceId;
  gpu_info.secondary_gpus.push_back(gpu_device);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
}

TEST_F(GpuBlacklistTest, GpuSwitching) {
  const std::string gpu_switching_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"macosx\"\n"
      "      },\n"
      "      \"gpu_switching\": \"force_discrete\"\n"
      "    },\n"
      "    {\n"
      "      \"id\": 2,\n"
      "      \"os\": {\n"
      "        \"type\": \"win\"\n"
      "      },\n"
      "      \"gpu_switching\": \"force_integrated\"\n"
      "    },\n"
      "    {\n"
      "      \"id\": 3,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"gpu_switching\": \"automatic\"\n"
      "    }\n"
      "  ]\n"
      "}";

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(gpu_switching_json,
                                          GpuBlacklist::kAllOs));
  GpuSwitchingOption switching = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).gpu_switching;
  EXPECT_EQ(GPU_SWITCHING_OPTION_FORCE_DISCRETE, switching);
  std::vector<uint32> entries;
  blacklist->GetDecisionEntries(&entries, false);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(1u, entries[0]);

  blacklist.reset(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(gpu_switching_json,
                                          GpuBlacklist::kAllOs));
  switching = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, kOsVersion, gpu_info()).gpu_switching;
  EXPECT_EQ(GPU_SWITCHING_OPTION_FORCE_INTEGRATED, switching);
  blacklist->GetDecisionEntries(&entries, false);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(2u, entries[0]);

  blacklist.reset(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(gpu_switching_json,
                                          GpuBlacklist::kAllOs));
  switching = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info()).gpu_switching;
  EXPECT_EQ(GPU_SWITCHING_OPTION_AUTOMATIC, switching);
  blacklist->GetDecisionEntries(&entries, false);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(3u, entries[0]);
}

TEST_F(GpuBlacklistTest, VideoDecode) {
  const std::string video_decode_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"macosx\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x10de\",\n"
      "      \"device_id\": [\"0x0640\"],\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_video_decode\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(video_decode_json,
                                          GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE, type);
}

TEST_F(GpuBlacklistTest, DualGpuModel) {
  const std::string model_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 5,\n"
      "      \"os\": {\n"
      "        \"type\": \"macosx\",\n"
      "        \"version\": {\n"
      "          \"op\": \">=\",\n"
      "          \"number\": \"10.7\"\n"
      "        }\n"
      "      },\n"
      "      \"machine_model\": {\n"
      "        \"name\": {\n"
      "          \"op\": \"=\",\n"
      "          \"value\": \"MacBookPro\"\n"
      "        },\n"
      "        \"version\": {\n"
      "          \"op\": \"<\",\n"
      "          \"number\": \"8\"\n"
      "        }\n"
      "      },\n"
      "      \"gpu_count\": {\n"
      "        \"op\": \"=\",\n"
      "        \"value\": \"2\"\n"
      "      },\n"
      "      \"gpu_switching\": \"force_discrete\"\n"
      "    }\n"
      "  ]\n"
      "}";
  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(model_json, GpuBlacklist::kAllOs));
  // Insert a second GPU.
  GPUInfo gpu_info;
  gpu_info.secondary_gpus.push_back(GPUInfo::GPUDevice());
  GpuSwitchingOption switching = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, "10.7.2", gpu_info).gpu_switching;
  EXPECT_EQ(GPU_SWITCHING_OPTION_FORCE_DISCRETE, switching);
}

TEST_F(GpuBlacklistTest, Css3D) {
  const std::string css_3d_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"macosx\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x10de\",\n"
      "      \"device_id\": [\"0x0640\"],\n"
      "      \"blacklist\": [\n"
      "        \"3d_css\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(css_3d_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_3D_CSS, type);
}

TEST_F(GpuBlacklistTest, Video) {
  const std::string video_json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"macosx\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x10de\",\n"
      "      \"device_id\": [\"0x0640\"],\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_video\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(video_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info()).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_ACCELERATED_VIDEO, type);
}

TEST_F(GpuBlacklistTest, NeedsMoreInfo) {
  const std::string json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x8086\",\n"
      "      \"driver_version\": {\n"
      "        \"op\": \"<\",\n"
      "        \"number\": \"10.7\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kIntelVendorId;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(json, GpuBlacklist::kAllOs));

  // The case this entry does not apply.
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);
  EXPECT_FALSE(blacklist->needs_more_info());

  // The case this entry might apply, but need more info.
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);
  EXPECT_TRUE(blacklist->needs_more_info());

  // The case we have full info, and this entry applies.
  gpu_info.driver_version = "10.6";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
  EXPECT_FALSE(blacklist->needs_more_info());

  // The case we have full info, and this entry does not apply.
  gpu_info.driver_version = "10.8";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);
  EXPECT_FALSE(blacklist->needs_more_info());
}

TEST_F(GpuBlacklistTest, NeedsMoreInfoForExceptions) {
  const std::string json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x8086\",\n"
      "      \"exceptions\": [\n"
      "        {\n"
      "          \"gl_renderer\": {\n"
      "            \"op\": \"contains\",\n"
      "            \"value\": \"mesa\"\n"
      "          }\n"
      "        }\n"
      "      ],\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kIntelVendorId;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(json, GpuBlacklist::kAllOs));

  // The case this entry does not apply.
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);
  EXPECT_FALSE(blacklist->needs_more_info());

  // The case this entry might apply, but need more info.
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);
  EXPECT_TRUE(blacklist->needs_more_info());

  // The case we have full info, and the exception applies (so the entry
  // does not apply).
  gpu_info.gl_renderer = "mesa";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(0, type);
  EXPECT_FALSE(blacklist->needs_more_info());

  // The case we have full info, and this entry applies.
  gpu_info.gl_renderer = "my renderer";
  type = blacklist->MakeBlacklistDecision(GpuBlacklist::kOsLinux, kOsVersion,
      gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
  EXPECT_FALSE(blacklist->needs_more_info());
}

TEST_F(GpuBlacklistTest, IgnorableEntries) {
  // If an entry will not change the blacklist decisions, then it should not
  // trigger the needs_more_info flag.
  const std::string json =
      "{\n"
      "  \"name\": \"gpu blacklist\",\n"
      "  \"version\": \"0.1\",\n"
      "  \"entries\": [\n"
      "    {\n"
      "      \"id\": 1,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x8086\",\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    },\n"
      "    {\n"
      "      \"id\": 2,\n"
      "      \"os\": {\n"
      "        \"type\": \"linux\"\n"
      "      },\n"
      "      \"vendor_id\": \"0x8086\",\n"
      "      \"driver_version\": {\n"
      "        \"op\": \"<\",\n"
      "        \"number\": \"10.7\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"webgl\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kIntelVendorId;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, kOsVersion, gpu_info).blacklisted_features;
  EXPECT_EQ(GPU_FEATURE_TYPE_WEBGL, type);
  EXPECT_FALSE(blacklist->needs_more_info());
}

}  // namespace content
