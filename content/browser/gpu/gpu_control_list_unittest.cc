// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/browser/gpu/gpu_control_list.h"
#include "content/public/common/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kOsVersion[] = "10.6.4";
const uint32 kIntelVendorId = 0x8086;
const uint32 kIntelDeviceId = 0x0166;  // 3rd Gen Core Graphics
const uint32 kNvidiaVendorId = 0x10de;
const uint32 kNvidiaDeviceId = 0x0fd5;  // GeForce GT 650M

#define LONG_STRING_CONST(...) #__VA_ARGS__

namespace content {

enum TestFeatureType {
  TEST_FEATURE_0 = 1,
  TEST_FEATURE_1 = 1 << 2,
  TEST_FEATURE_2 = 1 << 3,
};

class GpuControlListTest : public testing::Test {
 public:
  GpuControlListTest() { }

  virtual ~GpuControlListTest() { }

  const GPUInfo& gpu_info() const {
    return gpu_info_;
  }

  GpuControlList* Create() {
    GpuControlList* rt = new GpuControlList();
    rt->AddFeature("test_feature_0", TEST_FEATURE_0);
    rt->AddFeature("test_feature_1", TEST_FEATURE_1);
    rt->AddFeature("test_feature_2", TEST_FEATURE_2);
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

TEST_F(GpuControlListTest, DefaultControlListSettings) {
  scoped_ptr<GpuControlList> control_list(Create());
  // Default control list settings: all feature are allowed.
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);
}

TEST_F(GpuControlListTest, EmptyControlList) {
  // Empty list: all features are allowed.
  const std::string empty_list_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "2.5",
        "entries": [
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  EXPECT_TRUE(control_list->LoadList(empty_list_json,
                                     GpuControlList::kAllOs));
  EXPECT_EQ("2.5", control_list->GetVersion());
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);
}

TEST_F(GpuControlListTest, DetailedEntryAndInvalidJson) {
  // exact setting.
  const std::string exact_list_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 5,
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
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  EXPECT_TRUE(control_list->LoadList(exact_list_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);

  // Invalid json input should not change the current control_list settings.
  const std::string invalid_json = "invalid";

  EXPECT_FALSE(control_list->LoadList(invalid_json, GpuControlList::kAllOs));
  features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
  std::vector<uint32> entries;
  control_list->GetDecisionEntries(&entries, false);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(5u, entries[0]);
  EXPECT_EQ(5u, control_list->max_entry_id());
}

TEST_F(GpuControlListTest, VendorOnAllOsEntry) {
  // ControlList a vendor on all OS.
  const std::string vendor_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "vendor_id": "0x10de",
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  // ControlList entries won't be filtered to the current OS only upon loading.
  EXPECT_TRUE(control_list->LoadList(vendor_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_OPENBSD)
  // ControlList entries will be filtered to the current OS only upon loading.
  EXPECT_TRUE(control_list->LoadList(
      vendor_json, GpuControlList::kCurrentOsOnly));
  features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
#endif
}

TEST_F(GpuControlListTest, VendorOnLinuxEntry) {
  // ControlList a vendor on Linux only.
  const std::string vendor_linux_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  EXPECT_TRUE(control_list->LoadList(
      vendor_linux_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, AllExceptNVidiaOnLinuxEntry) {
  // ControlList all cards in Linux except NVIDIA.
  const std::string linux_except_nvidia_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  EXPECT_TRUE(control_list->LoadList(
      linux_except_nvidia_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);
}

TEST_F(GpuControlListTest, AllExceptIntelOnLinuxEntry) {
  // ControlList all cards in Linux except Intel.
  const std::string linux_except_intel_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  EXPECT_TRUE(control_list->LoadList(
      linux_except_intel_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, DateOnWindowsEntry) {
  // ControlList all drivers earlier than 2010-5-8 in Windows.
  const std::string date_windows_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  GPUInfo gpu_info;
  gpu_info.driver_date = "7-14-2009";

  EXPECT_TRUE(control_list->LoadList(
      date_windows_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_date = "07-14-2009";
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_date = "1-1-2010";
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_date = "05-07-2010";
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_date = "5-8-2010";
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_date = "5-9-2010";
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_date = "6-2-2010";
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);
}

TEST_F(GpuControlListTest, MultipleDevicesEntry) {
  const std::string devices_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "vendor_id": "0x10de",
            "device_id": ["0x1023", "0x0640"],
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  EXPECT_TRUE(control_list->LoadList(devices_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, ChromeOSEntry) {
  const std::string devices_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "os": {
              "type": "chromeos"
            },
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  EXPECT_TRUE(control_list->LoadList(devices_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsChromeOS, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);
}

TEST_F(GpuControlListTest, ChromeVersionEntry) {
  const std::string browser_version_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "browser_version": {
              "op": ">=",
              "number": "10"
            },
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list9(Create());
  EXPECT_TRUE(control_list9->LoadList(
      "9.0", browser_version_json, GpuControlList::kAllOs));
  int features = control_list9->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);

  scoped_ptr<GpuControlList> control_list10(Create());
  EXPECT_TRUE(control_list10->LoadList(
      "10.0", browser_version_json, GpuControlList::kAllOs));
  features = control_list10->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, MalformedVendor) {
  // vendor_id is defined as list instead of string.
  const std::string malformed_vendor_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "vendor_id": "[0x10de]",
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  EXPECT_FALSE(control_list->LoadList(
      malformed_vendor_json, GpuControlList::kAllOs));
}

TEST_F(GpuControlListTest, UnknownField) {
  const std::string unknown_field_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "unknown_field": 0,
            "features": [
              "test_feature_1"
            ]
          },
          {
            "id": 2,
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  EXPECT_TRUE(control_list->LoadList(
      unknown_field_json, GpuControlList::kAllOs));
  EXPECT_EQ(1u, control_list->num_entries());
  EXPECT_TRUE(control_list->contains_unknown_fields());
  int features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, UnknownExceptionField) {
  const std::string unknown_exception_field_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "unknown_field": 0,
            "features": [
              "test_feature_2"
            ]
          },
          {
            "id": 2,
            "exceptions": [
              {
                "unknown_field": 0
              }
            ],
            "features": [
              "test_feature_1"
            ]
          },
          {
            "id": 3,
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  EXPECT_TRUE(control_list->LoadList(
      unknown_exception_field_json, GpuControlList::kAllOs));
  EXPECT_EQ(1u, control_list->num_entries());
  EXPECT_TRUE(control_list->contains_unknown_fields());
  int features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, UnknownFeature) {
  const std::string unknown_feature_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "features": [
              "some_unknown_feature",
              "test_feature_0"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());

  EXPECT_TRUE(control_list->LoadList(
      unknown_feature_json, GpuControlList::kAllOs));
  EXPECT_EQ(1u, control_list->num_entries());
  EXPECT_TRUE(control_list->contains_unknown_fields());
  int features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, GlVendor) {
  const std::string gl_vendor_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(gl_vendor_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, GlRenderer) {
  const std::string gl_renderer_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );

  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(gl_renderer_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, PerfGraphics) {
  const std::string json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, PerfGaming) {
  const std::string json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "perf_gaming": {
              "op": "<=",
              "value": "4.0"
            },
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(0, features);
}

TEST_F(GpuControlListTest, PerfOverall) {
  const std::string json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, DisabledEntry) {
  const std::string disabled_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "disabled": true,
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(disabled_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EQ(features, 0);
  std::vector<uint32> flag_entries;
  control_list->GetDecisionEntries(&flag_entries, false);
  EXPECT_EQ(0u, flag_entries.size());
  control_list->GetDecisionEntries(&flag_entries, true);
  EXPECT_EQ(1u, flag_entries.size());
}

TEST_F(GpuControlListTest, Optimus) {
  const std::string optimus_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.optimus = true;

  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(optimus_json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, AMDSwitchable) {
  const std::string amd_switchable_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.amd_switchable = true;

  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(amd_switchable_json,
                                          GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, LexicalDriverVersion) {
  const std::string lexical_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "os": {
              "type": "linux"
            },
            "vendor_id": "0x1002",
            "driver_version": {
              "op": "<",
              "style": "lexical",
              "number": "8.201"
            },
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;

  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(lexical_json, GpuControlList::kAllOs));

  gpu_info.driver_version = "8.001.100";
  int features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.109";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.10900";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.109.100";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.2";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.20";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.200";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.20.100";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.201";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_version = "8.2010";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_version = "8.21";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_version = "8.21.100";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_version = "9.002";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_version = "9.201";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_version = "12";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_version = "12.201";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);
}

TEST_F(GpuControlListTest, LexicalDriverVersion2) {
  const std::string lexical_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "os": {
              "type": "linux"
            },
            "vendor_id": "0x1002",
            "driver_version": {
              "op": "<",
              "style": "lexical",
              "number": "9.002"
            },
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;

  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(lexical_json, GpuControlList::kAllOs));

  gpu_info.driver_version = "8.001.100";
  int features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.109";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.10900";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.109.100";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.2";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.20";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.200";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.20.100";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.201";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.2010";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.21";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.21.100";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "9.002";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_version = "9.201";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_version = "12";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  gpu_info.driver_version = "12.201";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);
}

TEST_F(GpuControlListTest, LexicalDriverVersion3) {
  const std::string lexical_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;

  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(lexical_json, GpuControlList::kAllOs));

  gpu_info.driver_version = "8.76";
  int features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.768";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);

  gpu_info.driver_version = "8.76.8";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, MultipleGPUsAny) {
  const std::string multi_gpu_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kNvidiaVendorId;
  gpu_info.gpu.device_id = kNvidiaDeviceId;

  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(multi_gpu_json,
                                          GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  GPUInfo::GPUDevice gpu_device;
  gpu_device.vendor_id = kIntelVendorId;
  gpu_device.device_id = kIntelDeviceId;
  gpu_info.secondary_gpus.push_back(gpu_device);
  features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, MultipleGPUsSecondary) {
  const std::string multi_gpu_json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
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
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kNvidiaVendorId;
  gpu_info.gpu.device_id = kNvidiaDeviceId;

  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(multi_gpu_json,
                                          GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);

  GPUInfo::GPUDevice gpu_device;
  gpu_device.vendor_id = kIntelVendorId;
  gpu_device.device_id = kIntelDeviceId;
  gpu_info.secondary_gpus.push_back(gpu_device);
  features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);
}

TEST_F(GpuControlListTest, NeedsMoreInfo) {
  const std::string json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "os": {
              "type": "linux"
            },
            "vendor_id": "0x8086",
            "driver_version": {
              "op": "<",
              "number": "10.7"
            },
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kIntelVendorId;

  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(json, GpuControlList::kAllOs));

  // The case this entry does not apply.
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);
  EXPECT_FALSE(control_list->needs_more_info());

  // The case this entry might apply, but need more info.
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);
  EXPECT_TRUE(control_list->needs_more_info());

  // The case we have full info, and this entry applies.
  gpu_info.driver_version = "10.6";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);
  EXPECT_FALSE(control_list->needs_more_info());

  // The case we have full info, and this entry does not apply.
  gpu_info.driver_version = "10.8";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);
  EXPECT_FALSE(control_list->needs_more_info());
}

TEST_F(GpuControlListTest, NeedsMoreInfoForExceptions) {
  const std::string json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "os": {
              "type": "linux"
            },
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
              "test_feature_0"
            ]
          }
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kIntelVendorId;

  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(json, GpuControlList::kAllOs));

  // The case this entry does not apply.
  int features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);
  EXPECT_FALSE(control_list->needs_more_info());

  // The case this entry might apply, but need more info.
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);
  EXPECT_TRUE(control_list->needs_more_info());

  // The case we have full info, and the exception applies (so the entry
  // does not apply).
  gpu_info.gl_renderer = "mesa";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(0, features);
  EXPECT_FALSE(control_list->needs_more_info());

  // The case we have full info, and this entry applies.
  gpu_info.gl_renderer = "my renderer";
  features = control_list->MakeDecision(GpuControlList::kOsLinux, kOsVersion,
      gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);
  EXPECT_FALSE(control_list->needs_more_info());
}

TEST_F(GpuControlListTest, IgnorableEntries) {
  // If an entry will not change the control_list decisions, then it should not
  // trigger the needs_more_info flag.
  const std::string json = LONG_STRING_CONST(
      {
        "name": "gpu control list",
        "version": "0.1",
        "entries": [
          {
            "id": 1,
            "os": {
              "type": "linux"
            },
            "vendor_id": "0x8086",
            "features": [
              "test_feature_0"
            ]
          },
          {
            "id": 2,
            "os": {
              "type": "linux"
            },
            "vendor_id": "0x8086",
            "driver_version": {
              "op": "<",
              "number": "10.7"
            },
            "features": [
              "test_feature_0"
            ]
          }
        ]
      }
  );
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kIntelVendorId;

  scoped_ptr<GpuControlList> control_list(Create());
  EXPECT_TRUE(control_list->LoadList(json, GpuControlList::kAllOs));
  int features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EQ(TEST_FEATURE_0, features);
  EXPECT_FALSE(control_list->needs_more_info());
}

}  // namespace content

