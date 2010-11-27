// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for the CEEE IE utilities.

#include "ceee/ie/common/ie_util.h"

#include "base/scoped_ptr.h"
#include "ceee/testing/utils/mock_static.h"
#include "ceee/testing/utils/test_utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "toolband.h"  // NOLINT

namespace ie_util {

using testing::_;
using testing::Return;

MOCK_STATIC_CLASS_BEGIN(MockIeUtil)
MOCK_STATIC_INIT_BEGIN(MockIeUtil)
  MOCK_STATIC_INIT2(ie_util::GetIeVersion, GetIeVersion);
MOCK_STATIC_INIT_END()
MOCK_STATIC0(IeVersion, , GetIeVersion);
MOCK_STATIC_CLASS_END(MockIeUtil)

static const wchar_t kAddonStatsRegPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Ext\\Stats\\"
    L"{4B2BDD16-152A-4693-9456-2B8EB834041D}\\iexplore";

static const GUID kAddonGuid =
    { 0x4b2bdd16, 0x152a, 0x4693,
         { 0x94, 0x56, 0x2b, 0x8e, 0xb8, 0x34, 0x4, 0x1d } };

class IeUtilTest : public testing::Test {
 protected:
  virtual void SetUp() {
    registry_override_.reset(new testing::ScopedRegistryOverride());
  }

  virtual void TearDown() {
    registry_override_.reset();
  }

  scoped_ptr<testing::ScopedRegistryOverride> registry_override_;
};

TEST_F(IeUtilTest, Ie7) {
  MockIeUtil ie_util_mock;
  EXPECT_CALL(ie_util_mock, GetIeVersion())
      .Times(1)
      .WillOnce(Return(ie_util::IEVERSION_IE7));

  EXPECT_EQ(-1, GetAverageAddonLoadTimeMs(kAddonGuid));
}

TEST_F(IeUtilTest, Ie8) {
  MockIeUtil ie_util_mock;
  EXPECT_CALL(ie_util_mock, GetIeVersion())
      .WillRepeatedly(Return(ie_util::IEVERSION_IE8));

  EXPECT_EQ(-1, GetAverageAddonLoadTimeMs(kAddonGuid));

  base::win::RegKey stats_key(HKEY_CURRENT_USER, kAddonStatsRegPath, KEY_WRITE);
  ASSERT_TRUE(stats_key.Valid());

  ASSERT_TRUE(stats_key.WriteValue(L"LoadTime", L"time"));
  EXPECT_EQ(-1, GetAverageAddonLoadTimeMs(kAddonGuid));

  DWORD time = 11;
  ASSERT_TRUE(stats_key.WriteValue(L"LoadTime", time));

  EXPECT_EQ(11, GetAverageAddonLoadTimeMs(kAddonGuid));
}

TEST_F(IeUtilTest, Ie9) {
  MockIeUtil ie_util_mock;
  EXPECT_CALL(ie_util_mock, GetIeVersion())
      .WillRepeatedly(Return(ie_util::IEVERSION_IE9));

  EXPECT_EQ(-1, GetAverageAddonLoadTimeMs(kAddonGuid));

  base::win::RegKey stats_key(HKEY_CURRENT_USER, kAddonStatsRegPath, KEY_WRITE);
  ASSERT_TRUE(stats_key.Valid());

  ASSERT_TRUE(stats_key.WriteValue(L"LoadTimeArray", L"time"));
  EXPECT_EQ(-1, GetAverageAddonLoadTimeMs(kAddonGuid));

  DWORD time[] = {1, 2, 3, 4, -1, 10};

  ASSERT_TRUE(stats_key.WriteValue(L"LoadTimeArray", &time, sizeof(time[0]),
                                   REG_BINARY));
  EXPECT_EQ(-1, GetAverageAddonLoadTimeMs(kAddonGuid));

  ASSERT_TRUE(stats_key.WriteValue(L"LoadTimeArray", &time, 10, REG_BINARY));
  EXPECT_EQ(-1, GetAverageAddonLoadTimeMs(kAddonGuid));

  ASSERT_TRUE(stats_key.WriteValue(L"LoadTimeArray", &time, 4 * sizeof(time[0]),
                                   REG_BINARY));
  EXPECT_EQ(2, GetAverageAddonLoadTimeMs(kAddonGuid));

  ASSERT_TRUE(stats_key.WriteValue(L"LoadTimeArray", time, sizeof(time),
                                   REG_BINARY));
  EXPECT_EQ(4, GetAverageAddonLoadTimeMs(kAddonGuid));
}

}  // namespace
