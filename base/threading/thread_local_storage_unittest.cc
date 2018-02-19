// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_local_storage.h"

#if defined(OS_WIN)
#include <windows.h>
#include <process.h>
#endif

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
// Ignore warnings about ptr->int conversions that we use when
// storing ints into ThreadLocalStorage.
#pragma warning(disable : 4311 4312)
#endif

namespace base {

namespace {

const int kInitialTlsValue = 0x5555;
const int kFinalTlsValue = 0x7777;
// How many times must a destructor be called before we really are done.
const int kNumberDestructorCallRepetitions = 3;

void ThreadLocalStorageCleanup(void* value);

ThreadLocalStorage::Slot& TLSSlot() {
  static NoDestructor<ThreadLocalStorage::Slot> slot(
      &ThreadLocalStorageCleanup);
  return *slot;
}

class ThreadLocalStorageRunner : public DelegateSimpleThread::Delegate {
 public:
  explicit ThreadLocalStorageRunner(int* tls_value_ptr)
      : tls_value_ptr_(tls_value_ptr) {}

  ~ThreadLocalStorageRunner() override = default;

  void Run() override {
    *tls_value_ptr_ = kInitialTlsValue;
    TLSSlot().Set(tls_value_ptr_);

    int* ptr = static_cast<int*>(TLSSlot().Get());
    EXPECT_EQ(ptr, tls_value_ptr_);
    EXPECT_EQ(*ptr, kInitialTlsValue);
    *tls_value_ptr_ = 0;

    ptr = static_cast<int*>(TLSSlot().Get());
    EXPECT_EQ(ptr, tls_value_ptr_);
    EXPECT_EQ(*ptr, 0);

    *ptr = kFinalTlsValue + kNumberDestructorCallRepetitions;
  }

 private:
  int* tls_value_ptr_;
  DISALLOW_COPY_AND_ASSIGN(ThreadLocalStorageRunner);
};


void ThreadLocalStorageCleanup(void *value) {
  int *ptr = reinterpret_cast<int*>(value);
  // Destructors should never be called with a NULL.
  ASSERT_NE(reinterpret_cast<int*>(NULL), ptr);
  if (*ptr == kFinalTlsValue)
    return;  // We've been called enough times.
  ASSERT_LT(kFinalTlsValue, *ptr);
  ASSERT_GE(kFinalTlsValue + kNumberDestructorCallRepetitions, *ptr);
  --*ptr;  // Move closer to our target.
  // Tell tls that we're not done with this thread, and still need destruction.
  TLSSlot().Set(value);
}

}  // namespace

TEST(ThreadLocalStorageTest, Basics) {
  ThreadLocalStorage::Slot slot;
  slot.Set(reinterpret_cast<void*>(123));
  int value = reinterpret_cast<intptr_t>(slot.Get());
  EXPECT_EQ(value, 123);
}

#if defined(THREAD_SANITIZER) || \
    (defined(OS_WIN) && defined(ARCH_CPU_X86_64) && !defined(NDEBUG))
// Do not run the test under ThreadSanitizer. Because this test iterates its
// own TSD destructor for the maximum possible number of times, TSan can't jump
// in after the last destructor invocation, therefore the destructor remains
// unsynchronized with the following users of the same TSD slot. This results
// in race reports between the destructor and functions in other tests.
//
// It is disabled on Win x64 with incremental linking (i.e. "Debug") pending
// resolution of http://crbug.com/251251.
#define MAYBE_TLSDestructors DISABLED_TLSDestructors
#else
#define MAYBE_TLSDestructors TLSDestructors
#endif
TEST(ThreadLocalStorageTest, MAYBE_TLSDestructors) {
  // Create a TLS index with a destructor.  Create a set of
  // threads that set the TLS, while the destructor cleans it up.
  // After the threads finish, verify that the value is cleaned up.
  const int kNumThreads = 5;
  int values[kNumThreads];
  ThreadLocalStorageRunner* thread_delegates[kNumThreads];
  DelegateSimpleThread* threads[kNumThreads];

  // Spawn the threads.
  for (int index = 0; index < kNumThreads; index++) {
    values[index] = kInitialTlsValue;
    thread_delegates[index] = new ThreadLocalStorageRunner(&values[index]);
    threads[index] = new DelegateSimpleThread(thread_delegates[index],
                                              "tls thread");
    threads[index]->Start();
  }

  // Wait for the threads to finish.
  for (int index = 0; index < kNumThreads; index++) {
    threads[index]->Join();
    delete threads[index];
    delete thread_delegates[index];

    // Verify that the destructor was called and that we reset.
    EXPECT_EQ(values[index], kFinalTlsValue);
  }
}

TEST(ThreadLocalStorageTest, TLSReclaim) {
  // Creates and destroys many TLS slots and ensures they all zero-inited.
  for (int i = 0; i < 1000; ++i) {
    ThreadLocalStorage::Slot slot(nullptr);
    EXPECT_EQ(nullptr, slot.Get());
    slot.Set(reinterpret_cast<void*>(0xBAADF00D));
    EXPECT_EQ(reinterpret_cast<void*>(0xBAADF00D), slot.Get());
  }
}

}  // namespace base
