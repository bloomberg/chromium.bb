// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
#include <string>

#include "base/message_loop.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/common/chrome_process_type.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

// Ensure the ClientId is formatted as expected.
TEST(MetricsServiceTest, ClientIdCorrectlyFormatted) {
  std::string clientid = MetricsService::GenerateClientID();
  EXPECT_EQ(36U, clientid.length());

  for (size_t i = 0; i < clientid.length(); ++i) {
    char current = clientid[i];
    if (i == 8 || i == 13 || i == 18 || i == 23)
      EXPECT_EQ('-', current);
    else
      EXPECT_TRUE(isxdigit(current));
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

TEST(MetricsServiceTest, LowEntropySource0NotReset) {
  MessageLoopForUI message_loop;
  content::TestBrowserThread ui_thread(content::BrowserThread::UI,
                                       &message_loop);
  ScopedTestingLocalState testing_local_state(
      TestingBrowserProcess::GetGlobal());
  MetricsService service;

  // Get the low entropy source once, to initialize it.
  service.GetLowEntropySource();

  // Now, set it to 0 and ensure it doesn't get reset.
  service.low_entropy_source_ = 0;
  EXPECT_EQ(0, service.GetLowEntropySource());
  // Call it another time, just to make sure.
  EXPECT_EQ(0, service.GetLowEntropySource());
}
