// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"
#include "chrome/browser/usb/usb_blocklist.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

class UsbBlocklistTest : public testing::Test {
 public:
  UsbBlocklistTest()
      : field_trial_list_(new base::FieldTrialList(nullptr)),
        list_(UsbBlocklist::Get()) {}

  void TearDown() override {
    // Because UsbBlocklist is a singleton it must be cleared after tests run
    // to prevent leakage between tests.
    field_trial_list_.reset();
    list_.ResetToDefaultValuesForTest();
  }

  std::unique_ptr<base::FieldTrialList> field_trial_list_;
  UsbBlocklist& list_;
};

TEST_F(UsbBlocklistTest, BasicExclusions) {
  list_.Exclude({0x18D1, 0x58F0, 0x0100});
  EXPECT_TRUE(list_.IsExcluded({0x18D1, 0x58F0, 0x0100}));
  EXPECT_FALSE(list_.IsExcluded({0x18D1, 0x58F1, 0x0100}));
  EXPECT_FALSE(list_.IsExcluded({0x18D0, 0x58F0, 0x0100}));
  EXPECT_FALSE(list_.IsExcluded({0x18D1, 0x58F0, 0x0200}));
}

TEST_F(UsbBlocklistTest, StringsWithNoValidEntries) {
  size_t previous_list_size = list_.size();
  list_.Exclude("");
  list_.Exclude("~!@#$%^&*()-_=+[]{}/*-");
  list_.Exclude(":");
  list_.Exclude("::");
  list_.Exclude(",");
  list_.Exclude(",,");
  list_.Exclude(",::,");
  list_.Exclude("1:2:3");
  list_.Exclude("18D1:2:3000");
  list_.Exclude("☯");
  EXPECT_EQ(previous_list_size, list_.size());
}

TEST_F(UsbBlocklistTest, StringsWithOneValidEntry) {
  size_t previous_list_size = list_.size();
  list_.Exclude("18D1:58F0:0101");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded({0x18D1, 0x58F0, 0x0101}));

  list_.Exclude(" 18D1:58F0:0200  ");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded({0x18D1, 0x58F0, 0x0200}));

  list_.Exclude(", 18D1:58F0:0201,  ");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded({0x18D1, 0x58F0, 0x0201}));

  list_.Exclude("18D1:58F0:0202, 0000:1:0000");
  EXPECT_EQ(++previous_list_size, list_.size());
  EXPECT_TRUE(list_.IsExcluded({0x18D1, 0x58F0, 0x0202}));
}

TEST_F(UsbBlocklistTest, ServerProvidedBlocklist) {
  if (base::FieldTrialList::TrialExists("WebUSBBlocklist")) {
    // This code checks to make sure that when a field trial is launched it
    // still contains our test data.
    LOG(INFO) << "WebUSBBlocklist field trial already configured.";
    ASSERT_NE(variations::GetVariationParamValue("WebUSBBlocklist",
                                                 "blocklist_additions")
                  .find("18D1:58F0:1BAD"),
              std::string::npos)
        << "ERROR: A WebUSBBlocklist field trial has been configured in\n"
           "testing/variations/fieldtrial_testing_config.json and must\n"
           "include this test's excluded device ID '18D1:58F0:1BAD' in\n"
           "blocklist_additions.\n";
  } else {
    LOG(INFO) << "Creating WebUSBBlocklist field trial for test.";
    // Create a field trial with test parameter.
    std::map<std::string, std::string> params;
    params["blocklist_additions"] = "18D1:58F0:1BAD";
    variations::AssociateVariationParams("WebUSBBlocklist", "TestGroup",
                                         params);
    base::FieldTrialList::CreateFieldTrial("WebUSBBlocklist", "TestGroup");
    // Refresh the blocklist based on the new field trial.
    list_.ResetToDefaultValuesForTest();
  }

  EXPECT_TRUE(list_.IsExcluded({0x18D1, 0x58F0, 0x1BAD}));
  EXPECT_FALSE(list_.IsExcluded({0x18D1, 0x58F0, 0x0100}));
}
