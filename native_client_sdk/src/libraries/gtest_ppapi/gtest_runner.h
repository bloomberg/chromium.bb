// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef GTEST_PPAPI_GTEST_RUNNER_H_
#define GTEST_PPAPI_GTEST_RUNNER_H_

#include <pthread.h>
#include "gtest_ppapi/thread_condition.h"

namespace pp {
class Instance;
}  // namespace pp

// GTestRunner is a threaded singleton for running gtest-based unit tests.
class GTestRunner {
 public:
  // Factory function to create the background gtest thread and associated
  // GTestRunner singleton. It is an error to call the factory function more
  // than once that raises an assert in debug mode.
  //
  // Parameters:
  // instance: the NaCl instance.
  // argc: arg count from pp::Instance::Init.
  // argv: the arguments from pp::Instance::Init.
  static void CreateGTestRunnerThread(pp::Instance* instance,
                                      int argc, char** argv);

  // Get the GTestRunner singleton.
  // @return A pointer to the GTestRunner singleton.
  static GTestRunner* gtest_runner() { return gtest_runner_; }

  // Tell the GTestRunner to run all gtest unit tests.
  void RunAllTests();

 protected:
  // The pthread thread function.
  static void* ThreadFunc(void* param);

  // Don't try to create instances directly. Use the factory function instead.
  GTestRunner();
  void Init(pp::Instance* instance, int argc, char** argv);

  // The thread run loop exits once all test have run.
  void RunLoop();

 private:
  // The status and associated status signal are used to control the state of
  // the thread run loop.
  enum Status { kIdle, kRunTests };
  ThreadCondition status_signal_;
  Status status_;

  static pthread_t g_test_runner_thread_;
  static GTestRunner* gtest_runner_;
};

#endif  // GTEST_PPAPI_GTEST_RUNNER_H_
