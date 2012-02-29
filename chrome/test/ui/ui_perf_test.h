// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_UI_PERF_TEST_H_
#define CHROME_TEST_UI_UI_PERF_TEST_H_
#pragma once

#include "chrome/test/ui/ui_test.h"

class UIPerfTest : public UITest {
 protected:
  // Overrides UITestBase.
  virtual void SetLaunchSwitches() OVERRIDE;

  // Prints IO performance data for use by perf graphs.
  void PrintIOPerfInfo(const char* test_name);

  // Prints memory usage data for use by perf graphs.
  void PrintMemoryUsageInfo(const char* test_name);

  // Configures the test to use the reference build.
  void UseReferenceBuild();
};

#endif  // CHROME_TEST_UI_UI_PERF_TEST_H_
