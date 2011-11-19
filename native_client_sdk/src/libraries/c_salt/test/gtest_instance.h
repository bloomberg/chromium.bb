// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef C_SALT_TEST_GTEST_INSTANCE_H_
#define C_SALT_TEST_GTEST_INSTANCE_H_

#include "ppapi/cpp/instance.h"

namespace c_salt {

// GTestInstance is a NaCl instance specifically dedicated to running
// gtest-based unit tests. It creates a GTestRunner thread/singleton pair as
// part of its Init function and runs all registered gtests once it
// receives a 'RunGTest' message in its post-message handler. Results from the
// test are posted back to JS through GTestEventListener.
//
// All tests are run from a background thread and must be written accordingly.
// Although that may complicate the test code a little, it allows the tests
// to be synchronized. I.e. Each test is launched and the thread is blocked
// until the outcome is known and reported.
class GTestInstance : public pp::Instance {
 public:
  explicit GTestInstance(PP_Instance instance);
  virtual ~GTestInstance();

  // pp::Instance overrides.
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);
  virtual void HandleMessage(const pp::Var& var_message);
};

}  // namespace c_salt

#endif  // C_SALT_TEST_GTEST_INSTANCE_H_

