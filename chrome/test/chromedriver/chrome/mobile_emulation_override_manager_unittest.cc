// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/device_metrics.h"
#include "chrome/test/chromedriver/chrome/mobile_emulation_override_manager.h"
#include "chrome/test/chromedriver/chrome/recorder_devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void AssertDeviceMetricsCommand(const Command& command,
                                const DeviceMetrics& device_metrics) {
  ASSERT_EQ("Page.setDeviceMetricsOverride", command.method);
  int width, height;
  double device_scale_factor, font_scale_factor;
  bool mobile, fit_window, text_autosizing;
  ASSERT_TRUE(command.params.GetInteger("width", &width));
  ASSERT_TRUE(command.params.GetInteger("height", &height));
  ASSERT_TRUE(command.params.GetDouble("deviceScaleFactor",
                                       &device_scale_factor));
  ASSERT_TRUE(command.params.GetBoolean("mobile", &mobile));
  ASSERT_TRUE(command.params.GetBoolean("fitWindow", &fit_window));
  ASSERT_TRUE(command.params.GetBoolean("textAutosizing", &text_autosizing));
  ASSERT_TRUE(command.params.GetDouble("fontScaleFactor", &font_scale_factor));
  ASSERT_EQ(device_metrics.width, width);
  ASSERT_EQ(device_metrics.height, height);
  ASSERT_EQ(device_metrics.device_scale_factor, device_scale_factor);
  ASSERT_EQ(device_metrics.mobile, mobile);
  ASSERT_EQ(device_metrics.fit_window, fit_window);
  ASSERT_EQ(device_metrics.text_autosizing, text_autosizing);
  ASSERT_EQ(device_metrics.font_scale_factor, font_scale_factor);
}

}  // namespace

TEST(MobileEmulationOverrideManager, SendsCommandWithTouchOnConnect) {
  RecorderDevToolsClient client;
  DeviceMetrics device_metrics(1, 2, 3.0, true, true);
  MobileEmulationOverrideManager manager(&client, &device_metrics);
  ASSERT_EQ(0u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());

  ASSERT_EQ(2u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());
  ASSERT_EQ(4u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertDeviceMetricsCommand(client.commands_[2], device_metrics));
}

TEST(MobileEmulationOverrideManager, SendsCommandWithoutTouchOnConnect) {
  RecorderDevToolsClient client;
  DeviceMetrics device_metrics(1, 2, 3.0, false, true);
  MobileEmulationOverrideManager manager(&client, &device_metrics);
  ASSERT_EQ(0u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());

  ASSERT_EQ(1u, client.commands_.size());
  ASSERT_EQ(kOk, manager.OnConnected(&client).code());
  ASSERT_EQ(2u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertDeviceMetricsCommand(client.commands_[1], device_metrics));
}

TEST(MobileEmulationOverrideManager, SendsCommandOnNavigation) {
  RecorderDevToolsClient client;
  DeviceMetrics device_metrics(1, 2, 3.0, true, true);
  MobileEmulationOverrideManager manager(&client, &device_metrics);
  base::DictionaryValue main_frame_params;
  ASSERT_EQ(kOk,
            manager.OnEvent(&client, "Page.frameNavigated", main_frame_params)
                .code());
  ASSERT_EQ(2u, client.commands_.size());
  ASSERT_EQ(kOk,
            manager.OnEvent(&client, "Page.frameNavigated", main_frame_params)
                .code());
  ASSERT_EQ(4u, client.commands_.size());
  ASSERT_NO_FATAL_FAILURE(
      AssertDeviceMetricsCommand(client.commands_[2], device_metrics));

  base::DictionaryValue sub_frame_params;
  sub_frame_params.SetString("frame.parentId", "id");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.frameNavigated", sub_frame_params).code());
  ASSERT_EQ(4u, client.commands_.size());
}
