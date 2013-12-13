// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "ppapi/native_client/tests/ppapi_test_lib/test_interface.h"


namespace {

void CrashingCallback(void* /*user_data*/, int32_t /*result*/) {
  printf("--- CrashingCallback\n");
  CRASH;
}

void CrashInCallback() {
  printf("--- CrashInCallback\n");
  PP_CompletionCallback callback =
      PP_MakeCompletionCallback(CrashingCallback, NULL);
  PPBCore()->CallOnMainThread(0 /*delay*/, callback, PP_OK);
}

}  // namespace

void SetupTests() {
  RegisterTest("CrashInCallback", CrashInCallback);
}

void SetupPluginInterfaces() {
  // none
}
