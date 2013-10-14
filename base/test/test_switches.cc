// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_switches.h"

// Time (in milliseconds) that the tests should wait before timing out.
// TODO(phajdan.jr): Clean up the switch names.
const char switches::kTestLargeTimeout[] = "test-large-timeout";

// Maximum number of tests to run in a single batch.
const char switches::kTestLauncherBatchLimit[] = "test-launcher-batch-limit";

// Number of parallel test launcher jobs.
const char switches::kTestLauncherJobs[] = "test-launcher-jobs";

// Path to test results file in our custom test launcher format.
const char switches::kTestLauncherOutput[] = "test-launcher-output";

// Path to test results file with all the info from the test launcher.
const char switches::kTestLauncherSummaryOutput[] =
    "test-launcher-summary-output";

// Time (in milliseconds) that the tests should wait before timing out.
const char switches::kTestLauncherTimeout[] = "test-launcher-timeout";
// TODO(phajdan.jr): Clean up the switch names.
const char switches::kTestTinyTimeout[] = "test-tiny-timeout";
const char switches::kUiTestActionTimeout[] = "ui-test-action-timeout";
const char switches::kUiTestActionMaxTimeout[] = "ui-test-action-max-timeout";
