// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cronet_c.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "components/cronet/native/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BufferTest : public ::testing::Test {
 public:
  BufferTest() = default;
  ~BufferTest() override {}

 protected:
  static void BufferCallback_OnDestroy(Cronet_BufferCallbackPtr self,
                                       Cronet_BufferPtr buffer);
  bool on_destroy_called() const { return on_destroy_called_; }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  void set_on_destroy_called(bool value) { on_destroy_called_ = value; }

  bool on_destroy_called_ = false;
  DISALLOW_COPY_AND_ASSIGN(BufferTest);
};

const uint64_t kTestBufferSize = 20;

// static
void BufferTest::BufferCallback_OnDestroy(Cronet_BufferCallbackPtr self,
                                          Cronet_BufferPtr buffer) {
  CHECK(self);
  Cronet_BufferCallbackContext context = Cronet_BufferCallback_GetContext(self);
  BufferTest* test = static_cast<BufferTest*>(context);
  CHECK(test);
  test->set_on_destroy_called(true);
  // Free buffer_data.
  void* buffer_data = Cronet_Buffer_GetData(buffer);
  CHECK(buffer_data);
  free(buffer_data);
}

// Test on_destroy that destroys the buffer set in context.
void TestRunnable_DestroyBuffer(Cronet_RunnablePtr self) {
  CHECK(self);
  Cronet_RunnableContext context = Cronet_Runnable_GetContext(self);
  Cronet_BufferPtr buffer = static_cast<Cronet_BufferPtr>(context);
  CHECK(buffer);
  // Destroy buffer. TestCronet_BufferCallback_OnDestroy should be invoked.
  Cronet_Buffer_Destroy(buffer);
}

// Example of allocating buffer with reasonable size.
TEST_F(BufferTest, TestInitWithAlloc) {
  // Create Cronet buffer and allocate buffer data.
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithAlloc(buffer, kTestBufferSize);
  ASSERT_TRUE(Cronet_Buffer_GetData(buffer));
  ASSERT_EQ(Cronet_Buffer_GetSize(buffer), kTestBufferSize);
  Cronet_Buffer_Destroy(buffer);
  ASSERT_FALSE(on_destroy_called());
}

// Example of allocating buffer with hugereasonable size.
TEST_F(BufferTest, TestInitWithHugeAllocFails) {
  // Create Cronet buffer and allocate buffer data.
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithAlloc(buffer, 1000 * 1000 * 1000 * 1000);
  ASSERT_FALSE(Cronet_Buffer_GetData(buffer));
  ASSERT_EQ(Cronet_Buffer_GetSize(buffer), 0ull);
  Cronet_Buffer_Destroy(buffer);
  ASSERT_FALSE(on_destroy_called());
}

// Example of intializing buffer with app-allocated data.
TEST_F(BufferTest, TestInitWithDataAndCallback) {
  Cronet_BufferCallbackPtr buffer_callback =
      Cronet_BufferCallback_CreateStub(BufferCallback_OnDestroy);
  Cronet_BufferCallback_SetContext(buffer_callback, this);
  // Create Cronet buffer and allocate buffer data.
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithDataAndCallback(buffer, malloc(kTestBufferSize),
                                        kTestBufferSize, buffer_callback);
  ASSERT_TRUE(Cronet_Buffer_GetData(buffer));
  ASSERT_EQ(Cronet_Buffer_GetSize(buffer), kTestBufferSize);
  Cronet_Buffer_Destroy(buffer);
  ASSERT_TRUE(on_destroy_called());
}

// Example of posting application on_destroy to the executor and passing
// buffer to it, expecting buffer to be destroyed and freed.
TEST_F(BufferTest, TestCronetBufferAsync) {
  // Executor provided by the application.
  Cronet_ExecutorPtr executor = cronet::test::CreateTestExecutor();
  Cronet_BufferCallbackPtr buffer_callback =
      Cronet_BufferCallback_CreateStub(BufferCallback_OnDestroy);
  Cronet_BufferCallback_SetContext(buffer_callback, this);
  // Create Cronet buffer and allocate buffer data.
  Cronet_BufferPtr buffer = Cronet_Buffer_Create();
  Cronet_Buffer_InitWithDataAndCallback(buffer, malloc(kTestBufferSize),
                                        kTestBufferSize, buffer_callback);
  Cronet_RunnablePtr runnable =
      Cronet_Runnable_CreateStub(TestRunnable_DestroyBuffer);
  Cronet_Runnable_SetContext(runnable, buffer);
  Cronet_Executor_Execute(executor, runnable);
  scoped_task_environment_.RunUntilIdle();
  ASSERT_TRUE(on_destroy_called());
  Cronet_Executor_Destroy(executor);
}

}  // namespace
