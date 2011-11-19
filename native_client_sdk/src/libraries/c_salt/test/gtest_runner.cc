// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "c_salt/test/gtest_runner.h"

#include <cassert>

#include "c_salt/test/gtest_event_listener.h"
#include "c_salt/test/gtest_nacl_environment.h"
#include "gtest/gtest.h"

namespace c_salt {

pthread_t GTestRunner::g_test_runner_thread_ = NACL_PTHREAD_ILLEGAL_THREAD_ID;
GTestRunner* GTestRunner::gtest_runner_ = NULL;

void GTestRunner::CreateGTestRunnerThread(pp::Instance* instance,
                                          int argc, char** argv) {
  assert(g_test_runner_thread_ == NACL_PTHREAD_ILLEGAL_THREAD_ID);
  if (g_test_runner_thread_ == NACL_PTHREAD_ILLEGAL_THREAD_ID) {
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
  pthread_exit(NULL);
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
  listeners.Append(new c_salt::GTestEventListener(instance));

  // We use our own gtest environment, mainly to make the nacl instance
  // available to the individual unit tests.
  c_salt::GTestNaclEnvironment* test_env = new c_salt::GTestNaclEnvironment();
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
  RUN_ALL_TESTS();
}

}  // namespace c_salt

