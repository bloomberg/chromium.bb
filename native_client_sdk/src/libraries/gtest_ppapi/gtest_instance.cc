// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest_ppapi/gtest_instance.h"
#include "gtest_ppapi/gtest_runner.h"
#include "ppapi/cpp/var.h"

#if defined(WIN32)
#undef PostMessage
#endif

GTestInstance::GTestInstance(PP_Instance instance)
  : pp::Instance(instance) {
}

GTestInstance::~GTestInstance() {
}

bool GTestInstance::Init(uint32_t /* argc */, const char* /* argn */[],
                         const char* /* argv */[]) {
  // Create a GTestRunner thread/singleton.
  int local_argc = 0;
  GTestRunner::CreateGTestRunnerThread(this, local_argc, NULL);
  return true;
}

void GTestInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_string()) {
    PostMessage("Invalid message");
    return;
  }

  std::string message = var_message.AsString();
  if (message == "RunGTest") {
    // This is our signal to start running the tests. Results from the tests
    // are posted through GTestEventListener.
    GTestRunner::gtest_runner()->RunAllTests();
  } else {
    std::string return_var;
    return_var = "Unknown message ";
    return_var += message;
    PostMessage(return_var);
  }
}
