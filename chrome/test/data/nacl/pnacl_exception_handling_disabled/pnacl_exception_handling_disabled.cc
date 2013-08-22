// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/nacl/nacl_exception.h"
#include "ppapi/native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "ppapi/native_client/tests/ppapi_test_lib/test_interface.h"

namespace {

void TestExceptionSetHandler(void) {
  int retval = nacl_exception_set_handler(NULL);
  EXPECT(retval == ENOSYS);

  TEST_PASSED;
}

void TestExceptionSetStack(void) {
  int retval = nacl_exception_set_stack(NULL, 0);
  EXPECT(retval == ENOSYS);

  TEST_PASSED;
}

void TestExceptionClearFlag(void) {
  int retval = nacl_exception_clear_flag();
  EXPECT(retval == ENOSYS);

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestExceptionSetHandler", TestExceptionSetHandler);
  RegisterTest("TestExceptionSetStack", TestExceptionSetStack);
  RegisterTest("TestExceptionClearFlag", TestExceptionClearFlag);
}

void SetupPluginInterfaces() {
  // none
}
