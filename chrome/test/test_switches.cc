// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/test_switches.h"

// Extra flags that the test should pass to launched browser process.
const char switches::kExtraChromeFlags[] = "extra-chrome-flags";

// Enable displaying error dialogs (for debugging).
const char switches::kEnableErrorDialogs[] = "enable-errdialogs";

// Timeout for medium tests, like browser_tests.
// TODO(phajdan.jr): Clean up the switch name.
const char switches::kMediumTestTimeout[] = "test-terminate-timeout";

// Number of iterations that page cycler tests will run.
const char switches::kPageCyclerIterations[] = "page-cycler-iterations";

// Time (in milliseconds) that the ui_tests should wait before timing out.
// TODO(phajdan.jr): Clean up the switch names.
const char switches::kUiTestActionTimeout[] = "ui-test-action-timeout";
const char switches::kUiTestActionMaxTimeout[] = "ui-test-action-max-timeout";
const char switches::kUiTestCommandExecutionTimeout[] = "ui-test-timeout";
const char switches::kUiTestSleepTimeout[] = "ui-test-sleep-timeout";
const char switches::kUiTestTerminateTimeout[] = "ui-test-terminate-timeout";
const char switches::kUiTestTimeout[] = "test-timeout";
