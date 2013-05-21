// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "content/browser/gpu/gpu_control_list.h"
#include "content/public/common/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#define LONG_STRING_CONST(...) #__VA_ARGS__

namespace content {

enum TestFeatureType {
  TEST_FEATURE_0 = 0,
  TEST_FEATURE_1,
  TEST_FEATURE_2
};

class GpuControlListEntryTest : public testing::Test {
 public:
  GpuControlListEntryTest() { }
  virtual ~GpuControlListEntryTest() { }

  const GPUInfo& gpu_info() const {
    return gpu_info_;
  }

  typedef GpuControlList::ScopedGpuControlListEntry ScopedEntry;

  static ScopedEntry GetEntryFromString(
      const std::string& json, bool supports_feature_type_all) {
    scoped_ptr<base::Value> root;
    root.reset(base::JSONReader::Read(json));
    DictionaryValue* value = NULL;
    if (root.get() == NULL || !root->GetAsDictionary(&value))
      return NULL;

    GpuControlList::FeatureMap feature_map;
    feature_map["test_feature_0"] = TEST_FEATURE_0;
    feature_map["test_feature_1"] = TEST_FEATURE_1;
    feature_map["test_feature_2"] = TEST_FEATURE_2;

    return GpuControlList::GpuControlListEntry::GetEntryFromValue(
        value, true, feature_map, supports_feature_type_all);
  }

  static ScopedEntry GetEntryFromString(const std::string& json) {
    return GetEntryFromString(json, false);
  }

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

 private:
  GPUInfo gpu_info_;
};

TEST_F(GpuControlListEntryTest, DetailedEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 5,
        "description": "test entry",
        "cr_bugs": [1024, 678],
        "webkit_bugs": [1950],
        "os": {
          "type": "macosx",
          "version": {
            "op": "=",
            "number": "10.6.4"
          }
        },
        "vendor_id": "0x10de",
        "device_id": ["0x0640"],
        "driver_version": {
          "op": "=",
          "number": "1.6.18"
        },
        "features": [
          "test_feature_0"
        ]
      }
  );

  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsMacosx, entry->GetOsType());
  EXPECT_FALSE(entry->disabled());
  EXPECT_EQ(5u, entry->id());
  EXPECT_STREQ("test entry", entry->description().c_str());
  EXPECT_EQ(2u, entry->cr_bugs().size());
  EXPECT_EQ(1024, entry->cr_bugs()[0]);
  EXPECT_EQ(678, entry->cr_bugs()[1]);
  EXPECT_EQ(1u, entry->webkit_bugs().size());
  EXPECT_EQ(1950, entry->webkit_bugs()[0]);
  EXPECT_EQ(1u, entry->features().size());
  EXPECT_EQ(1u, entry->features().count(TEST_FEATURE_0));
  EXPECT_FALSE(entry->contains_unknown_fields());
  EXPECT_FALSE(entry->contains_unknown_features());
  EXPECT_FALSE(entry->NeedsMoreInfo(gpu_info()));
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsMacosx, "10.6.4", gpu_info()));
}

TEST_F(GpuControlListEntryTest, VendorOnAllOsEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "vendor_id": "0x10de",
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsAny, entry->GetOsType());

  const GpuControlList::OsType os_type[] = {
    GpuControlList::kOsMacosx,
    GpuControlList::kOsWin,
    GpuControlList::kOsLinux,
    GpuControlList::kOsChromeOS,
    GpuControlList::kOsAndroid
  };
  for (size_t i = 0; i < arraysize(os_type); ++i)
    EXPECT_TRUE(entry->Contains(os_type[i], "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, VendorOnLinuxEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "os": {
          "type": "linux"
        },
        "vendor_id": "0x10de",
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsLinux, entry->GetOsType());

  const GpuControlList::OsType os_type[] = {
    GpuControlList::kOsMacosx,
    GpuControlList::kOsWin,
    GpuControlList::kOsChromeOS,
    GpuControlList::kOsAndroid
  };
  for (size_t i = 0; i < arraysize(os_type); ++i)
    EXPECT_FALSE(entry->Contains(os_type[i], "10.6", gpu_info()));
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsLinux, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, AllExceptNVidiaOnLinuxEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "os": {
          "type": "linux"
        },
        "exceptions": [
          {
            "vendor_id": "0x10de"
          }
        ],
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsLinux, entry->GetOsType());

  const GpuControlList::OsType os_type[] = {
    GpuControlList::kOsMacosx,
    GpuControlList::kOsWin,
    GpuControlList::kOsLinux,
    GpuControlList::kOsChromeOS,
    GpuControlList::kOsAndroid
  };
  for (size_t i = 0; i < arraysize(os_type); ++i)
    EXPECT_FALSE(entry->Contains(os_type[i], "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, AllExceptIntelOnLinuxEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "os": {
          "type": "linux"
        },
        "exceptions": [
          {
            "vendor_id": "0x8086"
          }
        ],
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsLinux, entry->GetOsType());

  const GpuControlList::OsType os_type[] = {
    GpuControlList::kOsMacosx,
    GpuControlList::kOsWin,
    GpuControlList::kOsChromeOS,
    GpuControlList::kOsAndroid
  };
  for (size_t i = 0; i < arraysize(os_type); ++i)
    EXPECT_FALSE(entry->Contains(os_type[i], "10.6", gpu_info()));
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsLinux, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, DateOnWindowsEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "os": {
          "type": "win"
        },
        "driver_date": {
          "op": "<",
          "number": "2010.5.8"
        },
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsWin, entry->GetOsType());

  GPUInfo gpu_info;
  gpu_info.driver_date = "4-12-2010";
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsWin, "10.6", gpu_info));
  gpu_info.driver_date = "5-8-2010";
  EXPECT_FALSE(entry->Contains(
      GpuControlList::kOsWin, "10.6", gpu_info));
  gpu_info.driver_date = "5-9-2010";
  EXPECT_FALSE(entry->Contains(
      GpuControlList::kOsWin, "10.6", gpu_info));
}

TEST_F(GpuControlListEntryTest, MultipleDevicesEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "vendor_id": "0x10de",
        "device_id": ["0x1023", "0x0640"],
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsAny, entry->GetOsType());

  const GpuControlList::OsType os_type[] = {
    GpuControlList::kOsMacosx,
    GpuControlList::kOsWin,
    GpuControlList::kOsLinux,
    GpuControlList::kOsChromeOS,
    GpuControlList::kOsAndroid
  };
  for (size_t i = 0; i < arraysize(os_type); ++i)
    EXPECT_TRUE(entry->Contains(os_type[i], "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, ChromeOSEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "os": {
          "type": "chromeos"
        },
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsChromeOS, entry->GetOsType());

  const GpuControlList::OsType os_type[] = {
    GpuControlList::kOsMacosx,
    GpuControlList::kOsWin,
    GpuControlList::kOsLinux,
    GpuControlList::kOsAndroid
  };
  for (size_t i = 0; i < arraysize(os_type); ++i)
    EXPECT_FALSE(entry->Contains(os_type[i], "10.6", gpu_info()));
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsChromeOS, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, MalformedVendor) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "vendor_id": "[0x10de]",
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry == NULL);
}

TEST_F(GpuControlListEntryTest, UnknownFieldEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "unknown_field": 0,
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->contains_unknown_fields());
  EXPECT_FALSE(entry->contains_unknown_features());
}

TEST_F(GpuControlListEntryTest, UnknownExceptionFieldEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 2,
        "exceptions": [
          {
            "unknown_field": 0
          }
        ],
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->contains_unknown_fields());
  EXPECT_FALSE(entry->contains_unknown_features());
}

TEST_F(GpuControlListEntryTest, UnknownFeatureEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "features": [
          "some_unknown_feature",
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_FALSE(entry->contains_unknown_fields());
  EXPECT_TRUE(entry->contains_unknown_features());
  EXPECT_EQ(1u, entry->features().size());
  EXPECT_EQ(1u, entry->features().count(TEST_FEATURE_0));

  const GpuControlList::OsType os_type[] = {
    GpuControlList::kOsMacosx,
    GpuControlList::kOsWin,
    GpuControlList::kOsLinux,
    GpuControlList::kOsChromeOS,
    GpuControlList::kOsAndroid
  };
  for (size_t i = 0; i < arraysize(os_type); ++i)
    EXPECT_TRUE(entry->Contains(os_type[i], "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, GlVendorEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "gl_vendor": {
          "op": "beginwith",
          "value": "NVIDIA"
        },
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);

  const GpuControlList::OsType os_type[] = {
    GpuControlList::kOsMacosx,
    GpuControlList::kOsWin,
    GpuControlList::kOsLinux,
    GpuControlList::kOsChromeOS,
    GpuControlList::kOsAndroid
  };
  for (size_t i = 0; i < arraysize(os_type); ++i)
    EXPECT_TRUE(entry->Contains(os_type[i], "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, GlRendererEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "gl_renderer": {
          "op": "contains",
          "value": "GeForce"
        },
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);

  const GpuControlList::OsType os_type[] = {
    GpuControlList::kOsMacosx,
    GpuControlList::kOsWin,
    GpuControlList::kOsLinux,
    GpuControlList::kOsChromeOS,
    GpuControlList::kOsAndroid
  };
  for (size_t i = 0; i < arraysize(os_type); ++i)
    EXPECT_TRUE(entry->Contains(os_type[i], "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, PerfGraphicsEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "perf_graphics": {
          "op": "<",
          "value": "6.0"
        },
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsWin, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, PerfGamingEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "perf_graphics": {
          "op": "<=",
          "value": "4.0"
        },
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_FALSE(entry->Contains(
      GpuControlList::kOsWin, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, PerfOverallEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "perf_overall": {
          "op": "between",
          "value": "1.0",
          "value2": "9.0"
        },
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsWin, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, DisabledEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "disabled": true,
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_TRUE(entry->disabled());
}

TEST_F(GpuControlListEntryTest, OptimusEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "os": {
          "type": "linux"
        },
        "multi_gpu_style": "optimus",
        "features": [
          "test_feature_0"
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.optimus = true;

  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsLinux, entry->GetOsType());
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsLinux, "10.6", gpu_info));
}

TEST_F(GpuControlListEntryTest, AMDSwitchableEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "os": {
          "type": "macosx"
        },
        "multi_gpu_style": "amd_switchable",
        "features": [
          "test_feature_0"
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.amd_switchable = true;

  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsMacosx, entry->GetOsType());
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsMacosx, "10.6", gpu_info));
}

TEST_F(GpuControlListEntryTest, LexicalDriverVersionEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "os": {
          "type": "linux"
        },
        "vendor_id": "0x1002",
        "driver_version": {
          "op": "=",
          "style": "lexical",
          "number": "8.76"
        },
        "features": [
          "test_feature_0"
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;

  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsLinux, entry->GetOsType());

  gpu_info.driver_version = "8.76";
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsLinux, "10.6", gpu_info));

  gpu_info.driver_version = "8.768";
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsLinux, "10.6", gpu_info));

  gpu_info.driver_version = "8.76.8";
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsLinux, "10.6", gpu_info));
}

TEST_F(GpuControlListEntryTest, MultipleGPUsAnyEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "os": {
          "type": "macosx"
        },
        "vendor_id": "0x8086",
        "device_id": ["0x0166"],
        "multi_gpu_category": "any",
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsMacosx, entry->GetOsType());

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x1976;
  EXPECT_FALSE(entry->Contains(
      GpuControlList::kOsMacosx, "10.6", gpu_info));

  GPUInfo::GPUDevice gpu_device;
  gpu_device.vendor_id = 0x8086;
  gpu_device.device_id = 0x0166;
  gpu_info.secondary_gpus.push_back(gpu_device);
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsMacosx, "10.6", gpu_info));
}

TEST_F(GpuControlListEntryTest, MultipleGPUsSecondaryEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "os": {
          "type": "macosx"
        },
        "vendor_id": "0x8086",
        "device_id": ["0x0166"],
        "multi_gpu_category": "secondary",
        "features": [
          "test_feature_0"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(GpuControlList::kOsMacosx, entry->GetOsType());

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x1976;
  EXPECT_FALSE(entry->Contains(
      GpuControlList::kOsMacosx, "10.6", gpu_info));

  GPUInfo::GPUDevice gpu_device;
  gpu_device.vendor_id = 0x8086;
  gpu_device.device_id = 0x0166;
  gpu_info.secondary_gpus.push_back(gpu_device);
  EXPECT_TRUE(entry->Contains(
      GpuControlList::kOsMacosx, "10.6", gpu_info));
}

TEST_F(GpuControlListEntryTest, NeedsMoreInfoEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "vendor_id": "0x8086",
        "driver_version": {
          "op": "<",
          "number": "10.7"
        },
        "features": [
          "test_feature_1"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  EXPECT_TRUE(entry->NeedsMoreInfo(gpu_info));

  gpu_info.driver_version = "10.6";
  EXPECT_FALSE(entry->NeedsMoreInfo(gpu_info));
}

TEST_F(GpuControlListEntryTest, NeedsMoreInfoForExceptionsEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "vendor_id": "0x8086",
        "exceptions": [
          {
            "gl_renderer": {
              "op": "contains",
              "value": "mesa"
            }
          }
        ],
        "features": [
          "test_feature_1"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json));
  EXPECT_TRUE(entry != NULL);

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  EXPECT_TRUE(entry->NeedsMoreInfo(gpu_info));

  gpu_info.gl_renderer = "mesa";
  EXPECT_FALSE(entry->NeedsMoreInfo(gpu_info));
}

TEST_F(GpuControlListEntryTest, FeatureTypeAllEntry) {
  const std::string json = LONG_STRING_CONST(
      {
        "id": 1,
        "features": [
          "all"
        ]
      }
  );
  ScopedEntry entry(GetEntryFromString(json, true));
  EXPECT_TRUE(entry != NULL);
  EXPECT_EQ(3u, entry->features().size());
  EXPECT_EQ(1u, entry->features().count(TEST_FEATURE_0));
  EXPECT_EQ(1u, entry->features().count(TEST_FEATURE_1));
  EXPECT_EQ(1u, entry->features().count(TEST_FEATURE_2));
}

}  // namespace content

