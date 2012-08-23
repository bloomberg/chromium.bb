// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "gtest_ppapi/gtest_runner.h"

#include <cassert>

#include "gtest/gtest.h"
#include "gtest_ppapi/gtest_event_listener.h"
#include "gtest_ppapi/gtest_nacl_environment.h"

pthread_t GTestRunner::g_test_runner_thread_;
GTestRunner* GTestRunner::gtest_runner_ = NULL;

void GTestRunner::CreateGTestRunnerThread(pp::Instance* instance,
                                          int argc, char** argv) {
  assert(!gtest_runner_);
  if (!gtest_runner_) {
    gtest_runner_ = new GTestRunner();
    gtest_runner_->Init(instance, argc, argv);
    pthread_create(&g_test_runner_thread_, NULL, ThreadFunc, NULL);
  }
}

void GTestRunner::RunAllTests() {
  status_signal_.Lock();
  assert(status_ == kIdle);
  status_ = kRunTests;
  status_signal_.Signal();
  status_signal_.Unlock();
}

void* GTestRunner::ThreadFunc(void* param) {
  gtest_runner_->RunLoop();
  delete gtest_runner_;
  gtest_runner_ = NULL;
  return NULL;
}

GTestRunner::GTestRunner() : status_(kIdle) { }

void GTestRunner::Init(pp::Instance* instance, int argc, char** argv) {
  // Init gtest.
  testing::InitGoogleTest(&argc, argv);

  // Add GTestEventListener to the list of listeners. That will cause the
  // test output to go to nacltest.js through PostMessage.
  ::testing::TestEventListeners& listeners =
      ::testing::UnitTest::GetInstance()->listeners();
  delete listeners.Release(listeners.default_result_printer());
  listeners.Append(new GTestEventListener(instance));

  // We use our own gtest environment, mainly to make the nacl instance
  // available to the individual unit tests.
  GTestNaclEnvironment* test_env = new GTestNaclEnvironment();
  test_env->set_global_instance(instance);
  ::testing::AddGlobalTestEnvironment(test_env);
}

void GTestRunner::RunLoop() {
  // Stay idle until RunAlltests is called.
  status_signal_.Lock();
  while (status_ == kIdle) {
    status_signal_.Wait();
  }

  assert(status_ == kRunTests);
  status_signal_.Unlock();
  int result = RUN_ALL_TESTS();
  (void)result;
}
