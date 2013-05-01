// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/logging.h"

#include "base/values.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/devtools_event_logger.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(Logging, CreatePerformanceLogger) {
  Capabilities capabilities;
  capabilities.logging_prefs.reset(new base::DictionaryValue());
  capabilities.logging_prefs->SetString("performance", "INFO");

  ScopedVector<DevToolsEventLogger> loggers;
  Status status = CreateLoggers(capabilities, &loggers);
  ASSERT_TRUE(status.IsOk());
  ASSERT_EQ(1u, loggers.size());
  ASSERT_STREQ("performance", loggers[0]->GetLogType().c_str());
}

TEST(Logging, InvalidLogType) {
  Capabilities capabilities;
  capabilities.logging_prefs.reset(new base::DictionaryValue());
  capabilities.logging_prefs->SetString("gaga", "INFO");

  ScopedVector<DevToolsEventLogger> loggers;
  Status status = CreateLoggers(capabilities, &loggers);
  EXPECT_FALSE(status.IsOk());
  ASSERT_EQ(0u, loggers.size());
}
