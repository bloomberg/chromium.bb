// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "c_salt/test/gtest_instance.h"
#include "c_salt/test/gtest_runner.h"
#include "ppapi/cpp/var.h"

namespace c_salt {

GTestInstance::GTestInstance(PP_Instance instance)
  : pp::Instance(instance) {
}

GTestInstance::~GTestInstance() {
}

bool GTestInstance::Init(uint32_t /* argc */, const char* /* argn */[],
                         const char* /* argv */[]) {
  // Create a GTestRunner thread/singleton.
  int local_argc = 0;
  c_salt::GTestRunner::CreateGTestRunnerThread(this, local_argc, NULL);
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
    c_salt::GTestRunner::gtest_runner()->RunAllTests();
  } else {
    std::string return_var;
    return_var = "Unknown message ";
    return_var += message;
    PostMessage(return_var);
  }
}

}  // namespace c_salt

