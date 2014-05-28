// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/ppapi/ppapi_test.h"
#include "ppapi/shared_impl/test_harness_utils.h"

// This file lists tests for Pepper APIs (without NaCl) against content_shell.
// TODO(teravest): Move more tests here. http://crbug.com/371873

// See chrome/test/ppapi/ppapi_browsertests.cc for Pepper testing that's
// covered in browser_tests.

namespace content {
namespace {

// This macro finesses macro expansion to do what we want.
#define STRIP_PREFIXES(test_name) ppapi::StripTestPrefixes(#test_name)

#define TEST_PPAPI_IN_PROCESS(test_name) \
  IN_PROC_BROWSER_TEST_F(PPAPITest, test_name) { \
    RunTest(STRIP_PREFIXES(test_name)); \
  }

#define TEST_PPAPI_OUT_OF_PROCESS(test_name) \
  IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, test_name) { \
    RunTest(STRIP_PREFIXES(test_name)); \
  }

TEST_PPAPI_IN_PROCESS(Crypto)
TEST_PPAPI_OUT_OF_PROCESS(Crypto)

TEST_PPAPI_IN_PROCESS(TraceEvent)
TEST_PPAPI_OUT_OF_PROCESS(TraceEvent)

}  // namespace
}  // namespace content
