// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl/nacl_browsertest_util.h"

namespace {

// These tests fail on Linux ASAN bots: <http://crbug.com/161709>.
#if defined(OS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_SimpleLoad DISABLED_SimpleLoad
#define MAYBE_ExitStatus DISABLED_ExitStatus
#define MAYBE_PPAPICore DISABLED_PPAPICore
#define MAYBE_ProgressEvents DISABLED_ProgressEvents
#else
#define MAYBE_SimpleLoad SimpleLoad
#define MAYBE_ExitStatus ExitStatus
#define MAYBE_PPAPICore PPAPICore
#define MAYBE_ProgressEvents ProgressEvents
#endif

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_SimpleLoad, {
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_ExitStatus, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exit0&expected_exit=0"));
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exit7&expected_exit=7"));
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exit254&expected_exit=254"));
  RunNaClIntegrationTest(FILE_PATH_LITERAL(
      "pm_exit_status_test.html?trigger=exitneg2&expected_exit=254"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_PPAPICore, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_ppb_core.html"));
})

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_ProgressEvents, {
  RunNaClIntegrationTest(FILE_PATH_LITERAL("ppapi_progress_events.html"));
})

}  // namespace anonymous
