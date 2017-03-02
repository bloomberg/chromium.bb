// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/bind_to_current_loop.h"

#include <memory>
#include <utility>

#include "base/memory/free_deleter.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

void BoundBoolSet(bool* var, bool val) {
  *var = val;
}

void BoundBoolSetFromScopedPtr(bool* var, std::unique_ptr<bool> val) {
  *var = *val;
}

void BoundBoolSetFromScopedPtrFreeDeleter(
    bool* var,
    std::unique_ptr<bool, base::FreeDeleter> val) {
  *var = *val;
}

void BoundBoolSetFromScopedArray(bool* var, std::unique_ptr<bool[]> val) {
  *var = val[0];
}

void BoundBoolSetFromConstRef(bool* var, const bool& val) {
  *var = val;
}

void BoundIntegersSet(int* a_var, int* b_var, int a_val, int b_val) {
  *a_var = a_val;
  *b_var = b_val;
}

struct ThreadRestrictionChecker {
  ThreadRestrictionChecker() : bound_loop_(base::MessageLoop::current()) {}

  void Run() { EXPECT_EQ(bound_loop_, base::MessageLoop::current()); }

  ~ThreadRestrictionChecker() {
    EXPECT_EQ(bound_loop_, base::MessageLoop::current());
  }

  base::MessageLoop* bound_loop_;
};

void RunAndClearReference(base::Closure cb) {
  cb.Run();
}

void ClearReference(base::Closure cb) {}

// Various tests that check that the bound function is only actually executed
// on the message loop, not during the original Run.
class BindToCurrentLoopTest : public ::testing::Test {
 protected:
  base::MessageLoop loop_;
};

TEST_F(BindToCurrentLoopTest, Closure) {
  // Test the closure is run inside the loop, not outside it.
  base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::Closure cb = BindToCurrentLoop(base::Bind(
      &base::WaitableEvent::Signal, base::Unretained(&waiter)));
  cb.Run();
  EXPECT_FALSE(waiter.IsSignaled());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(waiter.IsSignaled());
}

TEST_F(BindToCurrentLoopTest, Bool) {
  bool bool_var = false;
  base::Callback<void(bool)> cb = BindToCurrentLoop(base::Bind(
      &BoundBoolSet, &bool_var));
  cb.Run(true);
  EXPECT_FALSE(bool_var);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_var);
}

TEST_F(BindToCurrentLoopTest, BoundScopedPtrBool) {
  bool bool_val = false;
  std::unique_ptr<bool> scoped_ptr_bool(new bool(true));
  base::Closure cb = BindToCurrentLoop(base::Bind(
      &BoundBoolSetFromScopedPtr, &bool_val, base::Passed(&scoped_ptr_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, PassedScopedPtrBool) {
  bool bool_val = false;
  std::unique_ptr<bool> scoped_ptr_bool(new bool(true));
  base::Callback<void(std::unique_ptr<bool>)> cb =
      BindToCurrentLoop(base::Bind(&BoundBoolSetFromScopedPtr, &bool_val));
  cb.Run(std::move(scoped_ptr_bool));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, BoundScopedArrayBool) {
  bool bool_val = false;
  std::unique_ptr<bool[]> scoped_array_bool(new bool[1]);
  scoped_array_bool[0] = true;
  base::Closure cb = BindToCurrentLoop(base::Bind(
      &BoundBoolSetFromScopedArray, &bool_val,
      base::Passed(&scoped_array_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, PassedScopedArrayBool) {
  bool bool_val = false;
  std::unique_ptr<bool[]> scoped_array_bool(new bool[1]);
  scoped_array_bool[0] = true;
  base::Callback<void(std::unique_ptr<bool[]>)> cb =
      BindToCurrentLoop(base::Bind(&BoundBoolSetFromScopedArray, &bool_val));
  cb.Run(std::move(scoped_array_bool));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, BoundScopedPtrFreeDeleterBool) {
  bool bool_val = false;
  std::unique_ptr<bool, base::FreeDeleter> scoped_ptr_free_deleter_bool(
      static_cast<bool*>(malloc(sizeof(bool))));
  *scoped_ptr_free_deleter_bool = true;
  base::Closure cb = BindToCurrentLoop(base::Bind(
      &BoundBoolSetFromScopedPtrFreeDeleter, &bool_val,
      base::Passed(&scoped_ptr_free_deleter_bool)));
  cb.Run();
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, PassedScopedPtrFreeDeleterBool) {
  bool bool_val = false;
  std::unique_ptr<bool, base::FreeDeleter> scoped_ptr_free_deleter_bool(
      static_cast<bool*>(malloc(sizeof(bool))));
  *scoped_ptr_free_deleter_bool = true;
  base::Callback<void(std::unique_ptr<bool, base::FreeDeleter>)> cb =
      BindToCurrentLoop(
          base::Bind(&BoundBoolSetFromScopedPtrFreeDeleter, &bool_val));
  cb.Run(std::move(scoped_ptr_free_deleter_bool));
  EXPECT_FALSE(bool_val);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_val);
}

TEST_F(BindToCurrentLoopTest, BoolConstRef) {
  bool bool_var = false;
  bool true_var = true;
  const bool& true_ref = true_var;
  base::Closure cb = BindToCurrentLoop(base::Bind(
      &BoundBoolSetFromConstRef, &bool_var, true_ref));
  cb.Run();
  EXPECT_FALSE(bool_var);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(bool_var);
}

TEST_F(BindToCurrentLoopTest, Integers) {
  int a = 0;
  int b = 0;
  base::Callback<void(int, int)> cb = BindToCurrentLoop(base::Bind(
      &BoundIntegersSet, &a, &b));
  cb.Run(1, -1);
  EXPECT_EQ(a, 0);
  EXPECT_EQ(b, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(a, 1);
  EXPECT_EQ(b, -1);
}

TEST_F(BindToCurrentLoopTest, DestroyedOnBoundLoop) {
  base::Thread target_thread("testing");
  ASSERT_TRUE(target_thread.Start());

  // Ensure that the bound object is also destroyed on the correct thread even
  // if the last reference to the callback is dropped on the other thread.
  // TODO(tzik): Remove RunAndClearReference once TaskRunner migrates to
  // OnceClosure. RunAndCleareReference is needed to ensure no reference to |cb|
  // is left to the original thread.
  base::Closure cb = BindToCurrentLoop(
      base::Bind(&ThreadRestrictionChecker::Run,
                 base::MakeUnique<ThreadRestrictionChecker>()));
  target_thread.task_runner()->PostTask(
      FROM_HERE, base::Bind(&RunAndClearReference, base::Passed(&cb)));
  ASSERT_FALSE(cb);
  target_thread.FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // Ensure that the bound object is destroyed on the target thread even if
  // the callback is destroyed without invocation.
  cb = BindToCurrentLoop(
      base::Bind(&ThreadRestrictionChecker::Run,
                 base::MakeUnique<ThreadRestrictionChecker>()));
  target_thread.task_runner()->PostTask(
      FROM_HERE, base::Bind(&ClearReference, base::Passed(&cb)));
  target_thread.FlushForTesting();
  ASSERT_FALSE(cb);
  base::RunLoop().RunUntilIdle();

  target_thread.Stop();
}

}  // namespace media
