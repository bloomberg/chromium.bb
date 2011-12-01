// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_service.h"

#include <string>

#include "base/base64.h"

#include "testing/gtest/include/gtest/gtest.h"

class MetricsServiceTest : public ::testing::Test {
};

// Ensure the ClientId is formatted as expected.
TEST(MetricsServiceTest, ClientIdCorrectlyFormatted) {
  std::string clientid = MetricsService::GenerateClientID();
  EXPECT_EQ(36U, clientid.length());
  std::string hexchars = "0123456789ABCDEF";
  for (uint32 i = 0; i < clientid.length(); i++) {
    char current = clientid.at(i);
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      EXPECT_EQ('-', current);
    } else {
      EXPECT_TRUE(std::string::npos != hexchars.find(current));
    }
  }
}

TEST(MetricsServiceTest, IsPluginProcess) {
  EXPECT_TRUE(
      MetricsService::IsPluginProcess(content::PROCESS_TYPE_PLUGIN));
  EXPECT_TRUE(
      MetricsService::IsPluginProcess(content::PROCESS_TYPE_PPAPI_PLUGIN));
  EXPECT_FALSE(
      MetricsService::IsPluginProcess(content::PROCESS_TYPE_GPU));
}
