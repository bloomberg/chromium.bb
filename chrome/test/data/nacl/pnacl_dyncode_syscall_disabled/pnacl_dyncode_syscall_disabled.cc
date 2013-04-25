// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <nacl/nacl_dyncode.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

namespace {

void TestDyncodeCreate(void) {
  EXPECT(nacl_dyncode_create(NULL, NULL, 0) == -1);
  EXPECT(errno == ENOSYS);

  TEST_PASSED;
}

void TestDyncodeModify(void) {
  EXPECT(nacl_dyncode_modify(NULL, NULL, 0) == -1);
  EXPECT(errno == ENOSYS);

  TEST_PASSED;
}

void TestDyncodeDelete(void) {
  EXPECT(nacl_dyncode_delete(NULL, 0) == -1);
  EXPECT(errno == ENOSYS);

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestDyncodeCreate", TestDyncodeCreate);
  RegisterTest("TestDyncodeModify", TestDyncodeModify);
  RegisterTest("TestDyncodeDelete", TestDyncodeDelete);
}

void SetupPluginInterfaces() {
  // none
}
