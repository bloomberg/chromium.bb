// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "build/build_config.h"
#include "device/hid/hid_device_filter.h"
#include "device/hid/hid_device_info.h"
#include "device/hid/public/interfaces/hid.mojom.h"
#include "device/hid/test_report_descriptors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

#if defined(OS_MACOSX)
const uint64_t kTestDeviceId = 42;
#else
const char* kTestDeviceId = "device1";
#endif

}  // namespace

class HidFilterTest : public testing::Test {
 public:
  void SetUp() override {
    device_info_ = new HidDeviceInfo(
        kTestDeviceId, 0x046d, 0xc31c, "Test Keyboard", "123ABC",
        device::mojom::HidBusType::kHIDBusTypeUSB,
        std::vector<uint8_t>(kKeyboard, kKeyboard + kKeyboardSize));
  }

 protected:
  scoped_refptr<HidDeviceInfo> device_info_;
};

TEST_F(HidFilterTest, MatchAny) {
  HidDeviceFilter filter;
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchVendorId) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchVendorIdNegative) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x18d1);
  ASSERT_FALSE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchProductId) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  filter.SetProductId(0xc31c);
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchProductIdNegative) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  filter.SetProductId(0x0801);
  ASSERT_FALSE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchUsagePage) {
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageGenericDesktop);
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchUsagePageNegative) {
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageLed);
  ASSERT_FALSE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchVendorAndUsagePage) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  filter.SetUsagePage(HidUsageAndPage::kPageGenericDesktop);
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchUsageAndPage) {
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageGenericDesktop);
  filter.SetUsage(HidUsageAndPage::kGenericDesktopKeyboard);
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchUsageAndPageNegative) {
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageGenericDesktop);
  filter.SetUsage(0x02);
  ASSERT_FALSE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchEmptyFilterListNegative) {
  std::vector<HidDeviceFilter> filters;
  ASSERT_FALSE(HidDeviceFilter::MatchesAny(*device_info_->device(), filters));
}

TEST_F(HidFilterTest, MatchFilterList) {
  std::vector<HidDeviceFilter> filters;
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageGenericDesktop);
  filters.push_back(filter);
  ASSERT_TRUE(HidDeviceFilter::MatchesAny(*device_info_->device(), filters));
}

TEST_F(HidFilterTest, MatchFilterListNegative) {
  std::vector<HidDeviceFilter> filters;
  HidDeviceFilter filter;
  filter.SetUsagePage(HidUsageAndPage::kPageLed);
  filters.push_back(filter);
  ASSERT_FALSE(HidDeviceFilter::MatchesAny(*device_info_->device(), filters));
}

}  // namespace device
