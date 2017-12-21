// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/runnables.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "components/cronet/native/include/cronet_c.h"
#include "components/cronet/native/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class RunnablesTest : public ::testing::Test {
 public:
  RunnablesTest() = default;
  ~RunnablesTest() override {}

 protected:
  static void UrlRequestCallback_OnRedirectReceived(
      Cronet_UrlRequestCallbackPtr self,
      Cronet_UrlRequestPtr request,
      Cronet_UrlResponseInfoPtr info,
      CharString newLocationUrl);
  static void UrlRequestCallback_OnResponseStarted(
      Cronet_UrlRequestCallbackPtr self,
      Cronet_UrlRequestPtr request,
      Cronet_UrlResponseInfoPtr info);
  static void UrlRequestCallback_OnReadCompleted(
      Cronet_UrlRequestCallbackPtr self,
      Cronet_UrlRequestPtr request,
      Cronet_UrlResponseInfoPtr info,
      Cronet_BufferPtr buffer);

  bool callback_called() const { return callback_called_; }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  bool callback_called_ = false;
  DISALLOW_COPY_AND_ASSIGN(RunnablesTest);
};

class OnRedirectReceived_Runnable : public cronet::BaseCronet_Runnable {
 public:
  OnRedirectReceived_Runnable(Cronet_UrlRequestCallbackPtr callback,
                              CharString new_location_url)
      : callback_(callback), new_location_url_(new_location_url) {}

  ~OnRedirectReceived_Runnable() override = default;

  void Run() override {
    Cronet_UrlRequestCallback_OnRedirectReceived(
        callback_, /* request = */ nullptr, /* response_info = */ nullptr,
        new_location_url_.c_str());
  }

 private:
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback_;
  // New location to redirect to.
  std::string new_location_url_;
};

// Implementation of Cronet_UrlRequestCallback methods for testing.

// static
void RunnablesTest::UrlRequestCallback_OnRedirectReceived(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    CharString newLocationUrl) {
  CHECK(self);
  Cronet_UrlRequestCallbackContext context =
      Cronet_UrlRequestCallback_GetContext(self);
  RunnablesTest* test = static_cast<RunnablesTest*>(context);
  CHECK(test);
  test->callback_called_ = true;
  ASSERT_STREQ(newLocationUrl, "newUrl");
}

// static
void RunnablesTest::UrlRequestCallback_OnResponseStarted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info) {
  CHECK(self);
  Cronet_UrlRequestCallbackContext context =
      Cronet_UrlRequestCallback_GetContext(self);
  RunnablesTest* test = static_cast<RunnablesTest*>(context);
  CHECK(test);
  test->callback_called_ = true;
}

// static
void RunnablesTest::UrlRequestCallback_OnReadCompleted(
    Cronet_UrlRequestCallbackPtr self,
    Cronet_UrlRequestPtr request,
    Cronet_UrlResponseInfoPtr info,
    Cronet_BufferPtr buffer) {
  CHECK(self);
  CHECK(buffer);
  // Destroy the |buffer|.
  Cronet_Buffer_Destroy(buffer);
  Cronet_UrlRequestCallbackContext context =
      Cronet_UrlRequestCallback_GetContext(self);
  RunnablesTest* test = static_cast<RunnablesTest*>(context);
  CHECK(test);
  test->callback_called_ = true;
}

// Example of posting application callback to the executor.
TEST_F(RunnablesTest, TestRunCallbackOnExecutor) {
  // Executor provided by the application.
  Cronet_ExecutorPtr executor = cronet::test::CreateTestExecutor();
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback = Cronet_UrlRequestCallback_CreateStub(
      RunnablesTest::UrlRequestCallback_OnRedirectReceived,
      /* OnResponseStartedFunc = */ nullptr,
      /* OnReadCompletedFunc = */ nullptr,
      /* OnSucceededFunc = */ nullptr,
      /* OnFailedFunc = */ nullptr,
      /* OnCanceledFunc = */ nullptr);
  // New location to redirect to.
  CharString new_location_url = "newUrl";
  // Invoke Cronet_UrlRequestCallback_OnRedirectReceived
  Cronet_RunnablePtr runnable =
      new OnRedirectReceived_Runnable(callback, new_location_url);
  new_location_url = "bad";
  Cronet_UrlRequestCallback_SetContext(callback, this);
  Cronet_Executor_Execute(executor, runnable);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(callback_called());
  Cronet_Executor_Destroy(executor);
}

// Example of posting application callback to the executor using OneClosure.
TEST_F(RunnablesTest, TestRunOnceClosureOnExecutor) {
  // Executor provided by the application.
  Cronet_ExecutorPtr executor = cronet::test::CreateTestExecutor();
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback = Cronet_UrlRequestCallback_CreateStub(
      RunnablesTest::UrlRequestCallback_OnRedirectReceived,
      RunnablesTest::UrlRequestCallback_OnResponseStarted,
      /* OnReadCompletedFunc = */ nullptr,
      /* OnSucceededFunc = */ nullptr,
      /* OnFailedFunc = */ nullptr,
      /* OnCanceledFunc = */ nullptr);
  // Invoke Cronet_UrlRequestCallback_OnResponseStarted using OnceClosure
  Cronet_RunnablePtr runnable = new cronet::OnceClosureRunnable(
      base::BindOnce(Cronet_UrlRequestCallback_OnResponseStarted, callback,
                     /* request = */ nullptr, /* response_info = */ nullptr));
  Cronet_UrlRequestCallback_SetContext(callback, this);
  Cronet_Executor_Execute(executor, runnable);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(callback_called());
  Cronet_Executor_Destroy(executor);
}

// Example of posting application callback to the executor and passing
// Cronet_Buffer to it.
TEST_F(RunnablesTest, TestCronetBuffer) {
  // Executor provided by the application.
  Cronet_ExecutorPtr executor = cronet::test::CreateTestExecutor();
  // Callback provided by the application.
  Cronet_UrlRequestCallbackPtr callback = Cronet_UrlRequestCallback_CreateStub(
      RunnablesTest::UrlRequestCallback_OnRedirectReceived,
      RunnablesTest::UrlRequestCallback_OnResponseStarted,
      RunnablesTest::UrlRequestCallback_OnReadCompleted,
      /* OnSucceededFunc = */ nullptr,
      /* OnFailedFunc = */ nullptr,
      /* OnCanceledFunc = */ nullptr);
  // Create Cronet buffer and allocate buffer data.
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithAlloc(buffer, 20);
  // Invoke Cronet_UrlRequestCallback_OnReadCompleted using OnceClosure.
  Cronet_RunnablePtr runnable = new cronet::OnceClosureRunnable(base::BindOnce(
      RunnablesTest::UrlRequestCallback_OnReadCompleted, callback,
      /* request = */ nullptr,
      /* response_info = */ nullptr, buffer));
  Cronet_UrlRequestCallback_SetContext(callback, this);
  Cronet_Executor_Execute(executor, runnable);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(callback_called());
  Cronet_Executor_Destroy(executor);
}

}  // namespace
