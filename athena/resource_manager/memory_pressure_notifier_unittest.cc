/// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/resource_manager/memory_pressure_notifier.h"
#include "athena/resource_manager/public/resource_manager_delegate.h"
#include "athena/test/athena_test_base.h"

namespace athena {
namespace test {

namespace {

// Our OS delegate abstraction class to override the memory fill level.
class TestResourceManagerDelegate : public ResourceManagerDelegate {
 public:
  TestResourceManagerDelegate() : memory_fill_level_percent_(0) {}
  virtual ~TestResourceManagerDelegate() {}

  virtual int GetUsedMemoryInPercent() OVERRIDE {
    timer_called_++;
    return memory_fill_level_percent_;
  }

  virtual int MemoryPressureIntervalInMS() OVERRIDE {
    return 5;
  }

  void set_memory_fill_level_percent(int memory_fill_level_percent) {
    memory_fill_level_percent_ = memory_fill_level_percent;
  }

  // Returns the number of timer calls to the GetMemoryInPercent() calls.
  int timer_called() { return timer_called_; }

 private:
  // The to be returned memory fill level value in percent.
  int memory_fill_level_percent_;

  // How often was the timer calling the GetUsedMemoryInPercent() function.
  int timer_called_;

  DISALLOW_COPY_AND_ASSIGN(TestResourceManagerDelegate);
};

// Our memory pressure observer class.
class TestMemoryPressureObserver : public MemoryPressureObserver {
 public:
  TestMemoryPressureObserver(ResourceManagerDelegate* delegate)
      : delegate_(delegate),
        number_of_calls_(0),
        pressure_(MEMORY_PRESSURE_UNKNOWN) {}
  virtual ~TestMemoryPressureObserver() {}

  // The observer.
  virtual void OnMemoryPressure(MemoryPressure pressure) OVERRIDE {
    number_of_calls_++;
    pressure_ = pressure;
  }

  virtual ResourceManagerDelegate* GetDelegate() OVERRIDE {
    return delegate_.get();
  }

  int number_of_calls() { return number_of_calls_; }
  MemoryPressureObserver::MemoryPressure pressure() { return pressure_; }

 private:
  scoped_ptr<ResourceManagerDelegate> delegate_;

  // Number of calls received.
  int number_of_calls_;

  // Last posted memory pressure.
  MemoryPressureObserver::MemoryPressure pressure_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryPressureObserver);
};

}  // namespace

// Our testing base.
class MemoryPressureTest : public AthenaTestBase {
 public:
  MemoryPressureTest() : test_resource_manager_delegate_(NULL) {}
  virtual ~MemoryPressureTest() {}

  // AthenaTestBase:
  virtual void SetUp() OVERRIDE {
    AthenaTestBase::SetUp();
    // Create and install our TestAppContentDelegate with instrumentation.
    test_resource_manager_delegate_ =
        new TestResourceManagerDelegate();
    test_memory_pressure_observer_.reset(new TestMemoryPressureObserver(
        test_resource_manager_delegate_));
    memory_pressure_notifier_.reset(
        new MemoryPressureNotifier(test_memory_pressure_observer_.get()));
  }

  virtual void TearDown() OVERRIDE {
    memory_pressure_notifier_.reset();
    RunAllPendingInMessageLoop();
    test_memory_pressure_observer_.reset();
    AthenaTestBase::TearDown();
  }

 protected:
  TestResourceManagerDelegate* test_resource_manager_delegate() {
    return test_resource_manager_delegate_;
  }

  TestMemoryPressureObserver* test_memory_pressure_observer() {
    return test_memory_pressure_observer_.get();
  }

  // Waits until a timer interrupt occurs. Returns false if no timer is
  // registered.
  bool WaitForTimer() {
    int first_counter = test_resource_manager_delegate()->timer_called();
    // Wait up to 500ms for any poll on our memory status function from the
    // MemoryPressureNotifier.
    for (int i = 0; i < 500; ++i) {
      if (test_resource_manager_delegate()->timer_called() != first_counter)
        return true;
      usleep(1);
      RunAllPendingInMessageLoop();
    }
    return false;
  }

 private:
  // Not owned: the resource manager delegate.
  TestResourceManagerDelegate* test_resource_manager_delegate_;
  scoped_ptr<TestMemoryPressureObserver> test_memory_pressure_observer_;
  scoped_ptr<MemoryPressureNotifier> memory_pressure_notifier_;
  DISALLOW_COPY_AND_ASSIGN(MemoryPressureTest);
};

// Only creates and destroys it to see that the system gets properly shut down.
TEST_F(MemoryPressureTest, SimpleTest) {
}

// Test that we get only a single call while the memory pressure is low.
TEST_F(MemoryPressureTest, OneEventOnLowPressure) {
  ASSERT_TRUE(WaitForTimer());
  // No call should have happened at this time to the
  EXPECT_FALSE(test_memory_pressure_observer()->number_of_calls());
  // Set to something below 50% and check that we still get no call.
  test_resource_manager_delegate()->set_memory_fill_level_percent(49);
  ASSERT_TRUE(WaitForTimer());
  EXPECT_EQ(1, test_memory_pressure_observer()->number_of_calls());
  EXPECT_EQ(MemoryPressureObserver::MEMORY_PRESSURE_LOW,
            test_memory_pressure_observer()->pressure());
  ASSERT_TRUE(WaitForTimer());
  EXPECT_EQ(1, test_memory_pressure_observer()->number_of_calls());
  EXPECT_EQ(MemoryPressureObserver::MEMORY_PRESSURE_LOW,
            test_memory_pressure_observer()->pressure());
}

// Test that we get a |MEMORY_PRESSURE_UNKNOWN| if it cannot be determined.
TEST_F(MemoryPressureTest, TestNoCallsOnMemoryPressureUnknown) {
  test_resource_manager_delegate()->set_memory_fill_level_percent(0);
  ASSERT_TRUE(WaitForTimer());
  // We shouldn't have gotten a single call.
  EXPECT_FALSE(test_memory_pressure_observer()->number_of_calls());
  // And the memory pressure should be unknown.
  EXPECT_EQ(MemoryPressureObserver::MEMORY_PRESSURE_UNKNOWN,
            test_memory_pressure_observer()->pressure());
}

// Test that we get a change to MODERATE if the memory pressure is at 60%.
TEST_F(MemoryPressureTest, TestModeratePressure) {
  test_resource_manager_delegate()->set_memory_fill_level_percent(60);
  ASSERT_TRUE(WaitForTimer());
  // At least one call should have happened.
  int calls = test_memory_pressure_observer()->number_of_calls();
  EXPECT_TRUE(calls);
  EXPECT_EQ(MemoryPressureObserver::MEMORY_PRESSURE_MODERATE,
            test_memory_pressure_observer()->pressure());
  // Even if the value does not change, we should get more calls.
  ASSERT_TRUE(WaitForTimer());
  EXPECT_LT(calls, test_memory_pressure_observer()->number_of_calls());
  EXPECT_EQ(MemoryPressureObserver::MEMORY_PRESSURE_MODERATE,
            test_memory_pressure_observer()->pressure());
}

// Test that increasing and decreasing the memory pressure does the right thing.
TEST_F(MemoryPressureTest, TestPressureUpAndDown) {
  test_resource_manager_delegate()->set_memory_fill_level_percent(60);
  ASSERT_TRUE(WaitForTimer());
  // At least one call should have happened.
  int calls1 = test_memory_pressure_observer()->number_of_calls();
  EXPECT_TRUE(calls1);
  EXPECT_EQ(MemoryPressureObserver::MEMORY_PRESSURE_MODERATE,
            test_memory_pressure_observer()->pressure());

  // Check to the next level.
  test_resource_manager_delegate()->set_memory_fill_level_percent(80);
  ASSERT_TRUE(WaitForTimer());
  int calls2 = test_memory_pressure_observer()->number_of_calls();
  EXPECT_LT(calls1, calls2);
  EXPECT_EQ(MemoryPressureObserver::MEMORY_PRESSURE_HIGH,
            test_memory_pressure_observer()->pressure());

  // Check to no pressure again.
  test_resource_manager_delegate()->set_memory_fill_level_percent(20);
  ASSERT_TRUE(WaitForTimer());
  int calls3 = test_memory_pressure_observer()->number_of_calls();
  EXPECT_LT(calls2, calls3);
  EXPECT_EQ(MemoryPressureObserver::MEMORY_PRESSURE_LOW,
            test_memory_pressure_observer()->pressure());

  // Even if the value does not change, we should not get any more calls.
  ASSERT_TRUE(WaitForTimer());
  EXPECT_EQ(calls3, test_memory_pressure_observer()->number_of_calls());
  EXPECT_EQ(MemoryPressureObserver::MEMORY_PRESSURE_LOW,
            test_memory_pressure_observer()->pressure());
}

}  // namespace test
}  // namespace athena
