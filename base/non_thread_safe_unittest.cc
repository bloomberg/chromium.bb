// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/non_thread_safe.h"
#include "base/scoped_ptr.h"
#include "base/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifndef NDEBUG

// Simple class to exersice the basics of NonThreadSafe.
// Both the destructor and DoStuff should verify that they were
// called on the same thread as the constructor.
class NonThreadSafeClass : public NonThreadSafe {
 public:
  NonThreadSafeClass() {}

  // Verifies that it was called on the same thread as the constructor.
  void DoStuff() {
    DCHECK(CalledOnValidThread());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NonThreadSafeClass);
};

// Calls NonThreadSafeClass::DoStuff on another thread.
class CallDoStuffOnThread : public base::SimpleThread {
 public:
  CallDoStuffOnThread(NonThreadSafeClass* non_thread_safe_class)
      : SimpleThread("call_do_stuff_on_thread"),
        non_thread_safe_class_(non_thread_safe_class) {
  }

  virtual void Run() {
    non_thread_safe_class_->DoStuff();
  }

 private:
  NonThreadSafeClass* non_thread_safe_class_;

  DISALLOW_COPY_AND_ASSIGN(CallDoStuffOnThread);
};

// Deletes NonThreadSafeClass on a different thread.
class DeleteNonThreadSafeClassOnThread : public base::SimpleThread {
 public:
  DeleteNonThreadSafeClassOnThread(NonThreadSafeClass* non_thread_safe_class)
      : SimpleThread("delete_non_thread_safe_class_on_thread"),
        non_thread_safe_class_(non_thread_safe_class) {
  }

  virtual void Run() {
    non_thread_safe_class_.reset();
  }

 private:
  scoped_ptr<NonThreadSafeClass> non_thread_safe_class_;

  DISALLOW_COPY_AND_ASSIGN(DeleteNonThreadSafeClassOnThread);
};

TEST(NonThreadSafeTest, CallsAllowedOnSameThread) {
  scoped_ptr<NonThreadSafeClass> non_thread_safe_class(
      new NonThreadSafeClass);

  // Verify that DoStuff doesn't assert.
  non_thread_safe_class->DoStuff();

  // Verify that the destructor doesn't assert.
  non_thread_safe_class.reset();
}

#if GTEST_HAS_DEATH_TEST

TEST(NonThreadSafeDeathTest, MethodNotAllowedOnDifferentThread) {
  ASSERT_DEBUG_DEATH({
      scoped_ptr<NonThreadSafeClass> non_thread_safe_class(
          new NonThreadSafeClass);

      // Verify that DoStuff asserts when called on a different thread.
      CallDoStuffOnThread call_on_thread(non_thread_safe_class.get());

      call_on_thread.Start();
      call_on_thread.Join();
    }, "");
}

TEST(NonThreadSafeDeathTest, DestructorNotAllowedOnDifferentThread) {
  ASSERT_DEBUG_DEATH({
      scoped_ptr<NonThreadSafeClass> non_thread_safe_class(
          new NonThreadSafeClass);

      // Verify that the destructor asserts when called on a different thread.
      DeleteNonThreadSafeClassOnThread delete_on_thread(
          non_thread_safe_class.release());

      delete_on_thread.Start();
      delete_on_thread.Join();
    }, "");
}

#endif  // GTEST_HAS_DEATH_TEST

#endif  // NDEBUG
