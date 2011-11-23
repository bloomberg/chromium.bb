// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

class GpuBlacklistTest : public testing::Test {
 public:
  GpuBlacklistTest() { }

  virtual ~GpuBlacklistTest() { }

  const content::GPUInfo& gpu_info() const {
    return gpu_info_;
  }

 protected:
  void SetUp() {
    gpu_info_.vendor_id = 0x10de;
    gpu_info_.device_id = 0x0640;
    gpu_info_.driver_vendor = "NVIDIA";
    gpu_info_.driver_version = "1.6.18";
    gpu_info_.driver_date = "7-14-2009";
    gpu_info_.gl_vendor = "NVIDIA Corporation";
    gpu_info_.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
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
               .Append(FILE_PATH_LITERAL("software_rendering_list"))
               .Append(FILE_PATH_LITERAL("software_rendering_list.json"));
  ASSERT_TRUE(file_util::PathExists(data_file));
  int64 data_file_size64 = 0;
  ASSERT_TRUE(file_util::GetFileSize(data_file, &data_file_size64));
  int data_file_size = static_cast<int>(data_file_size64);
  scoped_array<char> data(new char[data_file_size]);
  ASSERT_EQ(file_util::ReadFile(data_file, data.get(), data_file_size),
            data_file_size);
  std::string json_string(data.get(), data_file_size);
  GpuBlacklist blacklist("1.0");
  EXPECT_TRUE(blacklist.LoadGpuBlacklist(json_string, GpuBlacklist::kAllOs));
  EXPECT_FALSE(blacklist.contains_unknown_fields());
}

TEST_F(GpuBlacklistTest, DefaultBlacklistSettings) {
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");
  // Default blacklist settings: all feature are allowed.
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsMacosx, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  EXPECT_TRUE(
      blacklist.LoadGpuBlacklist(empty_list_json, GpuBlacklist::kAllOs));
  uint16 major, minor;
  EXPECT_TRUE(blacklist.GetVersion(&major, &minor));
  EXPECT_EQ(major, 2u);
  EXPECT_EQ(minor, 5u);
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsMacosx, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  EXPECT_TRUE(
      blacklist.LoadGpuBlacklist(exact_list_json, GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsMacosx, os_version.get(), gpu_info());
  EXPECT_EQ(
      flags.flags(),
      static_cast<uint32>(GpuFeatureFlags::kGpuFeatureAcceleratedCompositing));

  // Invalid json input should not change the current blacklist settings.
  const std::string invalid_json = "invalid";

  EXPECT_FALSE(blacklist.LoadGpuBlacklist(invalid_json, GpuBlacklist::kAllOs));
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsMacosx, os_version.get(), gpu_info());
  EXPECT_EQ(
      flags.flags(),
      static_cast<uint32>(GpuFeatureFlags::kGpuFeatureAcceleratedCompositing));
  std::vector<uint32> entries;
  bool disabled = false;
  blacklist.GetGpuFeatureFlagEntries(
      GpuFeatureFlags::kGpuFeatureAcceleratedCompositing, entries, disabled);
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0], 5u);
  blacklist.GetGpuFeatureFlagEntries(
      GpuFeatureFlags::kGpuFeatureAll, entries, disabled);
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0], 5u);
  EXPECT_EQ(blacklist.max_entry_id(), 5u);
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  // Blacklist entries won't be filtered to the current OS only upon loading.
  EXPECT_TRUE(blacklist.LoadGpuBlacklist(vendor_json, GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsMacosx, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsLinux, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_OPENBSD)
  // Blacklist entries will be filtered to the current OS only upon loading.
  EXPECT_TRUE(
      blacklist.LoadGpuBlacklist(vendor_json, GpuBlacklist::kCurrentOsOnly));
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsMacosx, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsLinux, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  EXPECT_TRUE(
      blacklist.LoadGpuBlacklist(vendor_linux_json, GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsMacosx, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsLinux, os_version.get(), gpu_info());
  EXPECT_EQ(
      flags.flags(),
      static_cast<uint32>(GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  EXPECT_TRUE(blacklist.LoadGpuBlacklist(linux_except_nvidia_json,
      GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsMacosx, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsLinux, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  EXPECT_TRUE(blacklist.LoadGpuBlacklist(linux_except_intel_json,
      GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsMacosx, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsLinux, os_version.get(), gpu_info());
  EXPECT_EQ(
      flags.flags(),
      static_cast<uint32>(GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  EXPECT_TRUE(
      blacklist.LoadGpuBlacklist(date_windows_json, GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsMacosx, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsLinux, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(
      flags.flags(),
      static_cast<uint32>(GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  EXPECT_TRUE(blacklist.LoadGpuBlacklist(devices_json, GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsMacosx, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureMultisampling));
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureMultisampling));
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsLinux, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureMultisampling));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  EXPECT_TRUE(blacklist.LoadGpuBlacklist(devices_json, GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsChromeOS, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
  flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsLinux, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));

  GpuBlacklist blacklist9("9.0");
  EXPECT_TRUE(
      blacklist9.LoadGpuBlacklist(browser_version_json, GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist9.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);

  GpuBlacklist blacklist10("10.0");
  EXPECT_TRUE(
      blacklist10.LoadGpuBlacklist(browser_version_json, GpuBlacklist::kAllOs));
  flags = blacklist10.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
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
  GpuBlacklist blacklist("1.0");

  EXPECT_FALSE(
      blacklist.LoadGpuBlacklist(malformed_vendor_json, GpuBlacklist::kAllOs));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  EXPECT_TRUE(
      blacklist.LoadGpuBlacklist(unknown_field_json, GpuBlacklist::kAllOs));
  EXPECT_EQ(1u, blacklist.num_entries());
  EXPECT_TRUE(blacklist.contains_unknown_fields());
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  EXPECT_TRUE(blacklist.LoadGpuBlacklist(unknown_exception_field_json,
      GpuBlacklist::kAllOs));
  EXPECT_EQ(1u, blacklist.num_entries());
  EXPECT_TRUE(blacklist.contains_unknown_fields());
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));
  GpuBlacklist blacklist("1.0");

  EXPECT_TRUE(
      blacklist.LoadGpuBlacklist(unknown_feature_json, GpuBlacklist::kAllOs));
  EXPECT_EQ(1u, blacklist.num_entries());
  EXPECT_TRUE(blacklist.contains_unknown_fields());
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));

  GpuBlacklist blacklist("1.0");
  EXPECT_TRUE(
      blacklist.LoadGpuBlacklist(gl_vendor_json, GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));

  GpuBlacklist blacklist("1.0");
  EXPECT_TRUE(
      blacklist.LoadGpuBlacklist(gl_renderer_json, GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(GpuFeatureFlags::kGpuFeatureWebgl));
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
  scoped_ptr<Version> os_version(Version::GetVersionFromString("10.6.4"));

  GpuBlacklist blacklist("1.0");
  EXPECT_TRUE(
      blacklist.LoadGpuBlacklist(disabled_json, GpuBlacklist::kAllOs));
  GpuFeatureFlags flags = blacklist.DetermineGpuFeatureFlags(
      GpuBlacklist::kOsWin, os_version.get(), gpu_info());
  EXPECT_EQ(flags.flags(), 0u);
  std::vector<uint32> flag_entries;
  bool disabled = false;
  blacklist.GetGpuFeatureFlagEntries(
      GpuFeatureFlags::kGpuFeatureAll, flag_entries, disabled);
  EXPECT_EQ(flag_entries.size(), 0u);
  disabled = true;
  blacklist.GetGpuFeatureFlagEntries(
      GpuFeatureFlags::kGpuFeatureAll, flag_entries, disabled);
  EXPECT_EQ(flag_entries.size(), 1u);
}

