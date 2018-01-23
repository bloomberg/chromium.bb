// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/at_exit.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/atomic_flag.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class ConstructAndDestructLogger {
 public:
  ConstructAndDestructLogger() {
    CHECK(!constructed_->IsSet());
    constructed_->Set();
  }
  ~ConstructAndDestructLogger() {
    CHECK(false) << "This should be leaked and never reached!";
  }

  class ScopedSetup {
   public:
    ScopedSetup() {
      CHECK_EQ(nullptr, ConstructAndDestructLogger::constructed_);
      ConstructAndDestructLogger::constructed_ = &constructed_;
    }

    ~ScopedSetup() {
      CHECK_EQ(&constructed_, ConstructAndDestructLogger::constructed_);
      ConstructAndDestructLogger::constructed_ = nullptr;
    }

    AtomicFlag& constructed() { return constructed_; }

   private:
    AtomicFlag constructed_;
  };

 private:
  static AtomicFlag* constructed_;

  DISALLOW_COPY_AND_ASSIGN(ConstructAndDestructLogger);
};

AtomicFlag* ConstructAndDestructLogger::constructed_ = nullptr;

LazyInstance<ConstructAndDestructLogger>::Leaky lazy_logger =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

TEST(LazyInstanceTest, Basic) {
  ConstructAndDestructLogger::ScopedSetup test_scope;

  EXPECT_FALSE(lazy_logger.IsCreated());
  EXPECT_FALSE(test_scope.constructed().IsSet());

  lazy_logger.Get();
  EXPECT_TRUE(lazy_logger.IsCreated());
  EXPECT_TRUE(test_scope.constructed().IsSet());

  lazy_logger.Pointer();
  EXPECT_TRUE(lazy_logger.IsCreated());
  EXPECT_TRUE(test_scope.constructed().IsSet());
}

namespace {

class SlowConstructor {
 public:
  SlowConstructor() : some_int_(0) {
    // Sleep for 1 second to try to cause a race.
    PlatformThread::Sleep(TimeDelta::FromSeconds(1));
    CHECK(!*constructed);
    *constructed = true;
    some_int_ = 12;
  }

  int some_int() const { return some_int_; }

  class ScopedSetup {
   public:
    ScopedSetup() {
      CHECK_EQ(nullptr, SlowConstructor::constructed);
      SlowConstructor::constructed = &constructed_;
    }

    ~ScopedSetup() {
      CHECK_EQ(&constructed_, SlowConstructor::constructed);
      SlowConstructor::constructed = nullptr;
    }

    bool constructed() const { return constructed_; }

   private:
    bool constructed_ = false;
  };

 private:
  static bool* constructed;
  int some_int_;

  DISALLOW_COPY_AND_ASSIGN(SlowConstructor);
};

// static
bool* SlowConstructor::constructed = nullptr;

class SlowDelegate : public DelegateSimpleThread::Delegate {
 public:
  explicit SlowDelegate(LazyInstance<SlowConstructor>::DestructorAtExit* lazy)
      : lazy_(lazy) {}

  void Run() override {
    EXPECT_EQ(12, lazy_->Get().some_int());
    EXPECT_EQ(12, lazy_->Pointer()->some_int());
  }

 private:
  LazyInstance<SlowConstructor>::DestructorAtExit* lazy_;

  DISALLOW_COPY_AND_ASSIGN(SlowDelegate);
};

LazyInstance<SlowConstructor>::DestructorAtExit lazy_slow =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

TEST(LazyInstanceTest, ConstructorThreadSafety) {
  SlowConstructor::ScopedSetup test_scope;

  SlowDelegate delegate(&lazy_slow);
  EXPECT_FALSE(test_scope.constructed());

  DelegateSimpleThreadPool pool("lazy_instance_cons", 5);
  pool.AddWork(&delegate, 20);
  EXPECT_FALSE(test_scope.constructed());

  pool.Start();
  pool.JoinAll();
  EXPECT_TRUE(test_scope.constructed());
}

namespace {

// DeleteLogger is an object which sets a flag when it's destroyed.
// It accepts a bool* and sets the bool to true when the dtor runs.
class DeleteLogger {
 public:
  DeleteLogger() : deleted_(nullptr) {}
  ~DeleteLogger() { *deleted_ = true; }

  void SetDeletedPtr(bool* deleted) {
    deleted_ = deleted;
  }

 private:
  bool* deleted_;
};

}  // anonymous namespace

TEST(LazyInstanceTest, LeakyLazyInstance) {
  // Check that using a *DestructorAtExit* LazyInstance makes the dtor not run
  // when the AtExitManager finishes, since it is secretly the same thing.
  bool deleted1 = false;
  {
    ShadowingAtExitManager shadow;
    static LazyInstance<DeleteLogger>::DestructorAtExit test =
        LAZY_INSTANCE_INITIALIZER;
    test.Get().SetDeletedPtr(&deleted1);
  }
  EXPECT_FALSE(deleted1);

  // Check that using a *leaky* LazyInstance makes the dtor not run
  // when the AtExitManager finishes.
  bool deleted2 = false;
  {
    ShadowingAtExitManager shadow;
    static LazyInstance<DeleteLogger>::Leaky test = LAZY_INSTANCE_INITIALIZER;
    test.Get().SetDeletedPtr(&deleted2);
  }
  EXPECT_FALSE(deleted2);
}

namespace {

template <size_t alignment>
class AlignedData {
 public:
  AlignedData() = default;
  ~AlignedData() = default;
  alignas(alignment) char data_[alignment];
};

}  // namespace

#define EXPECT_ALIGNED(ptr, align) \
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(ptr) & (align - 1))

TEST(LazyInstanceTest, Alignment) {
  // Create some static instances with increasing sizes and alignment
  // requirements. By ordering this way, the linker will need to do some work to
  // ensure proper alignment of the static data.
  static LazyInstance<AlignedData<4>>::DestructorAtExit align4 =
      LAZY_INSTANCE_INITIALIZER;
  static LazyInstance<AlignedData<32>>::DestructorAtExit align32 =
      LAZY_INSTANCE_INITIALIZER;
  static LazyInstance<AlignedData<4096>>::DestructorAtExit align4096 =
      LAZY_INSTANCE_INITIALIZER;

  EXPECT_ALIGNED(align4.Pointer(), 4);
  EXPECT_ALIGNED(align32.Pointer(), 32);
  EXPECT_ALIGNED(align4096.Pointer(), 4096);
}

namespace {

// A class whose constructor busy-loops until it is told to complete
// construction.
class BlockingConstructor {
 public:
  BlockingConstructor() {
    EXPECT_FALSE(WasConstructorCalled());
    constructor_called_->Set();
    EXPECT_TRUE(WasConstructorCalled());
    while (!complete_construction_->IsSet())
      PlatformThread::YieldCurrentThread();
    done_construction_ = true;
  }

  // Returns true if BlockingConstructor() was entered.
  static bool WasConstructorCalled() { return constructor_called_->IsSet(); }

  // Instructs BlockingConstructor() that it may now unblock its construction.
  static void CompleteConstructionNow() { complete_construction_->Set(); }

  bool done_construction() { return done_construction_; }

  class ScopedSetup {
   public:
    ScopedSetup() {
      CHECK_EQ(nullptr, BlockingConstructor::constructor_called_);
      CHECK_EQ(nullptr, BlockingConstructor::complete_construction_);
      BlockingConstructor::constructor_called_ = &constructor_called_;
      BlockingConstructor::complete_construction_ = &complete_construction_;
    }

    ~ScopedSetup() {
      CHECK_EQ(&constructor_called_, BlockingConstructor::constructor_called_);
      CHECK_EQ(&complete_construction_,
               BlockingConstructor::complete_construction_);
      BlockingConstructor::constructor_called_ = nullptr;
      BlockingConstructor::complete_construction_ = nullptr;
    }

   private:
    AtomicFlag constructor_called_;
    AtomicFlag complete_construction_;
  };

 private:
  static AtomicFlag* constructor_called_;
  static AtomicFlag* complete_construction_;

  bool done_construction_ = false;

  DISALLOW_COPY_AND_ASSIGN(BlockingConstructor);
};

// A SimpleThread running at |thread_priority| which invokes |before_get|
// (optional) and then invokes Get() on the LazyInstance it's assigned.
class BlockingConstructorThread : public SimpleThread {
 public:
  BlockingConstructorThread(
      ThreadPriority thread_priority,
      LazyInstance<BlockingConstructor>::DestructorAtExit* lazy,
      OnceClosure before_get)
      : SimpleThread("BlockingConstructorThread", Options(thread_priority)),
        lazy_(lazy),
        before_get_(std::move(before_get)) {}

  void Run() override {
    if (before_get_)
      std::move(before_get_).Run();
    EXPECT_TRUE(lazy_->Get().done_construction());
  }

 private:
  LazyInstance<BlockingConstructor>::DestructorAtExit* lazy_;
  OnceClosure before_get_;

  DISALLOW_COPY_AND_ASSIGN(BlockingConstructorThread);
};

// static
AtomicFlag* BlockingConstructor::constructor_called_ = nullptr;
// static
AtomicFlag* BlockingConstructor::complete_construction_ = nullptr;

LazyInstance<BlockingConstructor>::DestructorAtExit lazy_blocking =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// Tests that if the thread assigned to construct the LazyInstance runs at
// background priority : the foreground threads will yield to it enough for it
// to eventually complete construction.
// This is a regression test for https://crbug.com/797129.
TEST(LazyInstanceTest, PriorityInversionAtInitializationResolves) {
  BlockingConstructor::ScopedSetup test_scope;

  TimeTicks test_begin = TimeTicks::Now();

  // Construct BlockingConstructor from a background thread.
  BlockingConstructorThread background_getter(ThreadPriority::BACKGROUND,
                                              &lazy_blocking, OnceClosure());
  background_getter.Start();

  while (!BlockingConstructor::WasConstructorCalled())
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));

  // Spin 4 foreground thread per core contending to get the already under
  // construction LazyInstance. When they are all running and poking at it :
  // allow the background thread to complete its work.
  const int kNumForegroundThreads = 4 * SysInfo::NumberOfProcessors();
  std::vector<std::unique_ptr<SimpleThread>> foreground_threads;
  RepeatingClosure foreground_thread_ready_callback =
      BarrierClosure(kNumForegroundThreads,
                     BindOnce(&BlockingConstructor::CompleteConstructionNow));
  for (int i = 0; i < kNumForegroundThreads; ++i) {
    foreground_threads.push_back(std::make_unique<BlockingConstructorThread>(
        ThreadPriority::NORMAL, &lazy_blocking,
        foreground_thread_ready_callback));
    foreground_threads.back()->Start();
  }

  // This test will hang if the foreground threads become stuck in
  // LazyInstance::Get() per the background thread never being scheduled to
  // complete construction.
  for (auto& foreground_thread : foreground_threads)
    foreground_thread->Join();
  background_getter.Join();

  // Fail if this test takes more than 5 seconds (it takes 5-10 seconds on a
  // Z840 without r527445 but is expected to be fast (~30ms) with the fix).
  EXPECT_LT(TimeTicks::Now() - test_begin, TimeDelta::FromSeconds(5));
}

TEST(LazyInstanceTest, ConstExprConstructible) {
  // Mostly just verifying that this compiles, the runtime test itself won't do
  // much... Declaring the variable constexpr forces the compiler to verify that
  // this is possible but renders the LazyInstance unusable as none of its
  // methods are const.
  static constexpr LazyInstance<SlowConstructor> kTestConstExprConstructible;
  static constexpr LazyInstance<SlowConstructor>
      kTestConstExprConstructibleWithExplicitInitializer =
          LAZY_INSTANCE_INITIALIZER;
  ALLOW_UNUSED_LOCAL(kTestConstExprConstructible);
  ALLOW_UNUSED_LOCAL(kTestConstExprConstructibleWithExplicitInitializer);
}

}  // namespace base
