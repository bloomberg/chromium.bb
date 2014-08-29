// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_device_filter.h"
#include "device/hid/hid_device_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class HidFilterTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    device_info_.vendor_id = 0x046d;
    device_info_.product_id = 0xc31c;

    HidCollectionInfo collection;
    collection.usage.usage_page = HidUsageAndPage::kPageKeyboard;
    collection.usage.usage = 0x01;
    device_info_.collections.push_back(collection);
  }

 protected:
  HidDeviceInfo device_info_;
};

TEST_F(HidFilterTest, MatchAny) {
  HidDeviceFilter filter;
  ASSERT_TRUE(filter.Matches(device_info_));
}

TEST_F(HidFilterTest, MatchVendorId) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  ASSERT_TRUE(filter.Matches(device_info_));
}

TEST_F(HidFilterTest, MatchVendorIdNegative) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x18d1);
  ASSERT_FALSE(filter.Matches(device_info_));
}

TEST_F(HidFilterTest, MatchProductId) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  filter.SetProductId(0xc31c);
  ASSERT_TRUE(filter.Matches(device_info_));
}

TEST_F(HidFilterTest, MatchProductIdNegative) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  filter.SetProductId(0x0801);
  ASSERT_FALSE(filter.Matches(device_info_));
}

TEST_F(HidFilterTest, MatchUsagePage) {
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageKeyboard);
  ASSERT_TRUE(filter.Matches(device_info_));
}

TEST_F(HidFilterTest, MatchUsagePageNegative) {
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageLed);
  ASSERT_FALSE(filter.Matches(device_info_));
}

TEST_F(HidFilterTest, MatchVendorAndUsagePage) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  filter.SetUsagePage(HidUsageAndPage::kPageKeyboard);
  ASSERT_TRUE(filter.Matches(device_info_));
}

TEST_F(HidFilterTest, MatchUsageAndPage) {
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageKeyboard);
  filter.SetUsage(0x01);
  ASSERT_TRUE(filter.Matches(device_info_));
}

TEST_F(HidFilterTest, MatchUsageAndPageNegative) {
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageKeyboard);
  filter.SetUsage(0x02);
  ASSERT_FALSE(filter.Matches(device_info_));
}

TEST_F(HidFilterTest, MatchEmptyFilterListNegative) {
  std::vector<HidDeviceFilter> filters;
  ASSERT_FALSE(HidDeviceFilter::MatchesAny(device_info_, filters));
}

TEST_F(HidFilterTest, MatchFilterList) {
  std::vector<HidDeviceFilter> filters;
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageKeyboard);
  filters.push_back(filter);
  ASSERT_TRUE(HidDeviceFilter::MatchesAny(device_info_, filters));
}

TEST_F(HidFilterTest, MatchFilterListNegative) {
  std::vector<HidDeviceFilter> filters;
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageLed);
  filters.push_back(filter);
  ASSERT_FALSE(HidDeviceFilter::MatchesAny(device_info_, filters));
}

}  // namespace

}  // namespace device
