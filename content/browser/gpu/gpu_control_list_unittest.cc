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

#define EXPECT_EMPTY_SET(feature_set) EXPECT_EQ(0u, feature_set.size())
#define EXPECT_SINGLE_FEATURE(feature_set, feature) \
    EXPECT_TRUE(feature_set.size() == 1 && feature_set.count(feature) == 1)

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
    rt->AddSupportedFeature("test_feature_0", TEST_FEATURE_0);
    rt->AddSupportedFeature("test_feature_1", TEST_FEATURE_1);
    rt->AddSupportedFeature("test_feature_2", TEST_FEATURE_2);
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
  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EMPTY_SET(features);
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
  EXPECT_EQ("2.5", control_list->version());
  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_EMPTY_SET(features);
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
  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);

  // Invalid json input should not change the current control_list settings.
  const std::string invalid_json = "invalid";

  EXPECT_FALSE(control_list->LoadList(invalid_json, GpuControlList::kAllOs));
  features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
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
  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info());
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX) || \
    defined(OS_OPENBSD)
  // ControlList entries will be filtered to the current OS only upon loading.
  EXPECT_TRUE(control_list->LoadList(
      vendor_json, GpuControlList::kCurrentOsOnly));
  features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info());
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info());
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
#endif
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
  std::set<int> features = control_list9->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EMPTY_SET(features);

  scoped_ptr<GpuControlList> control_list10(Create());
  EXPECT_TRUE(control_list10->LoadList(
      "10.0", browser_version_json, GpuControlList::kAllOs));
  features = control_list10->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
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
  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
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
  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
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
  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info());
  EXPECT_EMPTY_SET(features);
  std::vector<uint32> flag_entries;
  control_list->GetDecisionEntries(&flag_entries, false);
  EXPECT_EQ(0u, flag_entries.size());
  control_list->GetDecisionEntries(&flag_entries, true);
  EXPECT_EQ(1u, flag_entries.size());
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
  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info);
  EXPECT_EMPTY_SET(features);
  EXPECT_FALSE(control_list->needs_more_info());

  // The case this entry might apply, but need more info.
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EMPTY_SET(features);
  EXPECT_TRUE(control_list->needs_more_info());

  // The case we have full info, and the exception applies (so the entry
  // does not apply).
  gpu_info.gl_renderer = "mesa";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EMPTY_SET(features);
  EXPECT_FALSE(control_list->needs_more_info());

  // The case we have full info, and this entry applies.
  gpu_info.gl_renderer = "my renderer";
  features = control_list->MakeDecision(GpuControlList::kOsLinux, kOsVersion,
      gpu_info);
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
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
  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_FALSE(control_list->needs_more_info());
}

}  // namespace content

