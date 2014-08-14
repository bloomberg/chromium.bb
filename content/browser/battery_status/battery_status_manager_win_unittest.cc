// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/battery_status/battery_status_manager_win.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

TEST(BatteryStatusManagerWinTest, ACLineStatusOffline) {
  SYSTEM_POWER_STATUS win_status;
  win_status.ACLineStatus = WIN_AC_LINE_STATUS_OFFLINE;
  win_status.BatteryLifePercent = 100;
  win_status.BatteryLifeTime = 200;

  blink::WebBatteryStatus status = ComputeWebBatteryStatus(win_status);
  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.chargingTime);
  EXPECT_EQ(200, status.dischargingTime);
  EXPECT_EQ(1, status.level);
}

TEST(BatteryStatusManagerWinTest, ACLineStatusOfflineDischargingTimeUnknown) {
  SYSTEM_POWER_STATUS win_status;
  win_status.ACLineStatus = WIN_AC_LINE_STATUS_OFFLINE;
  win_status.BatteryLifePercent = 100;
  win_status.BatteryLifeTime = (DWORD)-1;

  blink::WebBatteryStatus status = ComputeWebBatteryStatus(win_status);
  EXPECT_FALSE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.chargingTime);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.dischargingTime);
  EXPECT_EQ(1, status.level);
}

TEST(BatteryStatusManagerWinTest, ACLineStatusOnline) {
  SYSTEM_POWER_STATUS win_status;
  win_status.ACLineStatus = WIN_AC_LINE_STATUS_ONLINE;
  win_status.BatteryLifePercent = 50;
  win_status.BatteryLifeTime = 200;

  blink::WebBatteryStatus status = ComputeWebBatteryStatus(win_status);
  EXPECT_TRUE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.chargingTime);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.dischargingTime);
  EXPECT_EQ(.5, status.level);
}

TEST(BatteryStatusManagerWinTest, ACLineStatusOnlineFullBattery) {
  SYSTEM_POWER_STATUS win_status;
  win_status.ACLineStatus = WIN_AC_LINE_STATUS_ONLINE;
  win_status.BatteryLifePercent = 100;
  win_status.BatteryLifeTime = 200;

  blink::WebBatteryStatus status = ComputeWebBatteryStatus(win_status);
  EXPECT_TRUE(status.charging);
  EXPECT_EQ(0, status.chargingTime);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.dischargingTime);
  EXPECT_EQ(1, status.level);
}

TEST(BatteryStatusManagerWinTest, ACLineStatusUnknown) {
  SYSTEM_POWER_STATUS win_status;
  win_status.ACLineStatus = WIN_AC_LINE_STATUS_UNKNOWN;
  win_status.BatteryLifePercent = 50;
  win_status.BatteryLifeTime = 200;

  blink::WebBatteryStatus status = ComputeWebBatteryStatus(win_status);
  EXPECT_TRUE(status.charging);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.chargingTime);
  EXPECT_EQ(std::numeric_limits<double>::infinity(), status.dischargingTime);
  EXPECT_EQ(.5, status.level);
}

}  // namespace

}  // namespace content
