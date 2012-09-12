// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/version.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/public/common/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::GpuFeatureType;
using content::GpuSwitchingOption;

class GpuBlacklistTest : public testing::Test {
 public:
  GpuBlacklistTest() { }

  virtual ~GpuBlacklistTest() { }

  const content::GPUInfo& gpu_info() const {
    return gpu_info_;
  }

  GpuBlacklist* Create() {
    return new GpuBlacklist();
  }

 protected:
  void SetUp() {
    gpu_info_.gpu.vendor_id = 0x10de;
    gpu_info_.gpu.device_id = 0x0640;
    gpu_info_.driver_vendor = "NVIDIA";
    gpu_info_.driver_version = "1.6.18";
    gpu_info_.driver_date = "7-14-2009";
    gpu_info_.gl_vendor = "NVIDIA Corporation";
    gpu_info_.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
    gpu_info_.performance_stats.graphics = 5.0;
    gpu_info_.performance_stats.gaming = 5.0;
    gpu_info_.performance_stats.overall = 5.0;
  }

  void TearDown() {
  }

 private:
  content::GPUInfo gpu_info_;
};

TEST_F(GpuBlacklistTest, CurrentBlacklistValidation) {
  FilePath data_file;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_file));
  data_file =
      data_file.Append(FILE_PATH_LITERAL("chrome"))
               .Append(FILE_PATH_LITERAL("browser"))
               .Append(FILE_PATH_LITERAL("resources"))
               .Append(FILE_PATH_LITERAL("software_rendering_list.json"));
  ASSERT_TRUE(file_util::PathExists(data_file));
  int64 data_file_size64 = 0;
  ASSERT_TRUE(file_util::GetFileSize(data_file, &data_file_size64));
  int data_file_size = static_cast<int>(data_file_size64);
  scoped_array<char> data(new char[data_file_size]);
  ASSERT_EQ(file_util::ReadFile(data_file, data.get(), data_file_size),
            data_file_size);
  std::string json_string(data.get(), data_file_size);
  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      json_string, GpuBlacklist::kAllOs));
  EXPECT_FALSE(blacklist->contains_unknown_fields());
}

TEST_F(GpuBlacklistTest, DefaultBlacklistSettings) {
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());
  // Default blacklist settings: all feature are allowed.
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
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
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      empty_list_json, GpuBlacklist::kAllOs));
  EXPECT_EQ(blacklist->GetVersion(), std::string("2.5"));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
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
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      exact_list_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING);

  // Invalid json input should not change the current blacklist settings.
  const std::string invalid_json = "invalid";

  EXPECT_FALSE(blacklist->LoadGpuBlacklist(
      invalid_json, GpuBlacklist::kAllOs));
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING);
  std::vector<uint32> entries;
  bool disabled = false;
  blacklist->GetDecisionEntries(entries, disabled);
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0], 5u);
  EXPECT_EQ(blacklist->max_entry_id(), 5u);
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
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  // Blacklist entries won't be filtered to the current OS only upon loading.
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      vendor_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_OPENBSD)
  // Blacklist entries will be filtered to the current OS only upon loading.
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      vendor_json, GpuBlacklist::kCurrentOsOnly));
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      vendor_linux_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);
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
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      linux_except_nvidia_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
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
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      linux_except_intel_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);
}

TEST_F(GpuBlacklistTest, DateOnWindowsEntry) {
  // Blacklist all drivers earlier than 2010-01 in Windows.
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
      "        \"number\": \"2010.1\"\n"
      "      },\n"
      "      \"blacklist\": [\n"
      "        \"accelerated_2d_canvas\"\n"
      "      ]\n"
      "    }\n"
      "  ]\n"
      "}";
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      date_windows_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);
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
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      devices_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_MULTISAMPLING);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_MULTISAMPLING);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_MULTISAMPLING);
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
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      devices_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsChromeOS, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
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
  Version os_version("10.6.4");

  scoped_ptr<GpuBlacklist> blacklist9(Create());
  EXPECT_TRUE(blacklist9->LoadGpuBlacklist(
      "9.0", browser_version_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist9->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);

  scoped_ptr<GpuBlacklist> blacklist10(Create());
  EXPECT_TRUE(blacklist10->LoadGpuBlacklist(
      "10.0", browser_version_json, GpuBlacklist::kAllOs));
  type = blacklist10->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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

  EXPECT_FALSE(blacklist->LoadGpuBlacklist(
      malformed_vendor_json, GpuBlacklist::kAllOs));
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
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      unknown_field_json, GpuBlacklist::kAllOs));
  EXPECT_EQ(1u, blacklist->num_entries());
  EXPECT_TRUE(blacklist->contains_unknown_fields());
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      unknown_exception_field_json, GpuBlacklist::kAllOs));
  EXPECT_EQ(1u, blacklist->num_entries());
  EXPECT_TRUE(blacklist->contains_unknown_fields());
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");
  scoped_ptr<GpuBlacklist> blacklist(Create());

  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      unknown_feature_json, GpuBlacklist::kAllOs));
  EXPECT_EQ(1u, blacklist->num_entries());
  EXPECT_TRUE(blacklist->contains_unknown_fields());
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      gl_vendor_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      gl_renderer_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
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
  Version os_version("10.6.4");

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      disabled_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, 0);
  std::vector<uint32> flag_entries;
  bool disabled = false;
  blacklist->GetDecisionEntries(flag_entries, disabled);
  EXPECT_EQ(flag_entries.size(), 0u);
  disabled = true;
  blacklist->GetDecisionEntries(flag_entries, disabled);
  EXPECT_EQ(flag_entries.size(), 1u);
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
  Version os_version("10.6.4");

  content::GPUInfo gpu_info;
  gpu_info.optimus = true;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      optimus_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");

  content::GPUInfo gpu_info;
  gpu_info.amd_switchable = true;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      amd_switchable_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");

  content::GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      lexical_json, GpuBlacklist::kAllOs));

  gpu_info.driver_version = "8.109";
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);

  gpu_info.driver_version = "8.2";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);

  gpu_info.driver_version = "8.21";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info).blacklisted_features;
  EXPECT_EQ(type, 0);

  gpu_info.driver_version = "8.2010";
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info).blacklisted_features;
  EXPECT_EQ(type, 0);
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
  Version os_version("10.6.4");

  content::GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0fd5;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      multi_gpu_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info).blacklisted_features;
  EXPECT_EQ(type, 0);

  content::GPUInfo::GPUDevice gpu_device;
  gpu_device.vendor_id = 0x8086;
  gpu_device.device_id = 0x0166;
  gpu_info.secondary_gpus.push_back(gpu_device);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");

  content::GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0fd5;

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      multi_gpu_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info).blacklisted_features;
  EXPECT_EQ(type, 0);

  content::GPUInfo::GPUDevice gpu_device;
  gpu_device.vendor_id = 0x8086;
  gpu_device.device_id = 0x0166;
  gpu_info.secondary_gpus.push_back(gpu_device);
  type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_WEBGL);
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
  Version os_version("10.6.4");

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      gpu_switching_json, GpuBlacklist::kAllOs));
  GpuSwitchingOption switching = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).gpu_switching;
  EXPECT_EQ(switching, content::GPU_SWITCHING_FORCE_DISCRETE);
  std::vector<uint32> entries;
  bool disabled = false;
  blacklist->GetDecisionEntries(entries, disabled);
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0], 1u);

  blacklist.reset(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      gpu_switching_json, GpuBlacklist::kAllOs));
  switching = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsWin, &os_version,
      gpu_info()).gpu_switching;
  EXPECT_EQ(switching, content::GPU_SWITCHING_FORCE_INTEGRATED);
  blacklist->GetDecisionEntries(entries, disabled);
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0], 2u);

  blacklist.reset(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      gpu_switching_json, GpuBlacklist::kAllOs));
  switching = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsLinux, &os_version,
      gpu_info()).gpu_switching;
  EXPECT_EQ(switching, content::GPU_SWITCHING_AUTOMATIC);
  blacklist->GetDecisionEntries(entries, disabled);
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0], 3u);
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
  Version os_version("10.6.4");

  scoped_ptr<GpuBlacklist> blacklist(Create());
  EXPECT_TRUE(blacklist->LoadGpuBlacklist(
      video_decode_json, GpuBlacklist::kAllOs));
  GpuFeatureType type = blacklist->MakeBlacklistDecision(
      GpuBlacklist::kOsMacosx, &os_version,
      gpu_info()).blacklisted_features;
  EXPECT_EQ(type, content::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE);
}
