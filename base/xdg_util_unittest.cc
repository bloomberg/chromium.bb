// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/xdg_util.h"

#include "base/environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;

namespace {

class MockEnvironment : public base::Environment {
 public:
  MOCK_METHOD2(GetVar, bool(const char*, std::string* result));
  MOCK_METHOD2(SetVar, bool(const char*, const std::string& new_value));
  MOCK_METHOD1(UnSetVar, bool(const char*));
};

const char* kGnome = "gnome";
const char* kKDE4 = "kde4";
const char* kKDE = "kde";
const char* kXFCE = "xfce";

}  // namespace

TEST(XDGUtilTest, GetDesktopEnvironmentGnome) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(StrEq("DESKTOP_SESSION"), _))
      .WillOnce(DoAll(SetArgumentPointee<1>(kGnome), Return(true)));

  EXPECT_EQ(base::DESKTOP_ENVIRONMENT_GNOME,
            base::GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetDesktopEnvironmentKDE4) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(StrEq("DESKTOP_SESSION"), _))
      .WillOnce(DoAll(SetArgumentPointee<1>(kKDE4), Return(true)));

  EXPECT_EQ(base::DESKTOP_ENVIRONMENT_KDE4,
            base::GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetDesktopEnvironmentKDE3) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(StrEq("DESKTOP_SESSION"), _))
      .WillOnce(DoAll(SetArgumentPointee<1>(kKDE), Return(true)));

  EXPECT_EQ(base::DESKTOP_ENVIRONMENT_KDE3,
            base::GetDesktopEnvironment(&getter));
}

TEST(XDGUtilTest, GetDesktopEnvironmentXFCE) {
  MockEnvironment getter;
  EXPECT_CALL(getter, GetVar(_, _)).WillRepeatedly(Return(false));
  EXPECT_CALL(getter, GetVar(StrEq("DESKTOP_SESSION"), _))
      .WillOnce(DoAll(SetArgumentPointee<1>(kXFCE), Return(true)));

  EXPECT_EQ(base::DESKTOP_ENVIRONMENT_XFCE,
            base::GetDesktopEnvironment(&getter));
}
