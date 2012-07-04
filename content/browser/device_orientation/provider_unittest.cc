// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "content/browser/device_orientation/data_fetcher.h"
#include "content/browser/device_orientation/orientation.h"
#include "content/browser/device_orientation/provider.h"
#include "content/browser/device_orientation/provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_orientation {
namespace {

// Class for checking expectations on orientation updates from the Provider.
class UpdateChecker : public Provider::Observer {
 public:
  explicit UpdateChecker(int* expectations_count_ptr)
      : expectations_count_ptr_(expectations_count_ptr) {
  }

  virtual ~UpdateChecker() {}

  // From Provider::Observer.
  virtual void OnOrientationUpdate(const Orientation& orientation) {
    ASSERT_FALSE(expectations_queue_.empty());

    Orientation expected = expectations_queue_.front();
    expectations_queue_.pop();

    EXPECT_EQ(expected.can_provide_alpha(), orientation.can_provide_alpha());
    EXPECT_EQ(expected.can_provide_beta(),  orientation.can_provide_beta());
    EXPECT_EQ(expected.can_provide_gamma(), orientation.can_provide_gamma());
    EXPECT_EQ(expected.can_provide_absolute(),
              orientation.can_provide_absolute());
    if (expected.can_provide_alpha())
      EXPECT_EQ(expected.alpha(), orientation.alpha());
    if (expected.can_provide_beta())
      EXPECT_EQ(expected.beta(), orientation.beta());
    if (expected.can_provide_gamma())
      EXPECT_EQ(expected.gamma(), orientation.gamma());
    if (expected.can_provide_absolute())
      EXPECT_EQ(expected.absolute(), orientation.absolute());

    --(*expectations_count_ptr_);

    if (*expectations_count_ptr_ == 0) {
      MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    }
  }

  void AddExpectation(const Orientation& orientation) {
    expectations_queue_.push(orientation);
    ++(*expectations_count_ptr_);
  }

 private:
  // Set up by the test fixture, which then blocks while it is accessed
  // from OnOrientationUpdate which is executed on the test fixture's
  // message_loop_.
  int* expectations_count_ptr_;
  std::queue<Orientation> expectations_queue_;
};

// Class for injecting test orientation data into the Provider.
class MockOrientationFactory : public base::RefCounted<MockOrientationFactory> {
 public:
  MockOrientationFactory()
      : is_failing_(false) {
    EXPECT_FALSE(instance_);
    instance_ = this;
  }

  static DataFetcher* CreateDataFetcher() {
    EXPECT_TRUE(instance_);
    return new MockDataFetcher(instance_);
  }

  void SetOrientation(const Orientation& orientation) {
    base::AutoLock auto_lock(lock_);
    orientation_ = orientation;
  }

  void SetFailing(bool is_failing) {
    base::AutoLock auto_lock(lock_);
    is_failing_ = is_failing;
  }

 private:
  friend class base::RefCounted<MockOrientationFactory>;

  ~MockOrientationFactory() {
    instance_ = NULL;
  }

  // Owned by ProviderImpl. Holds a reference back to MockOrientationFactory.
  class MockDataFetcher : public DataFetcher {
   public:
    explicit MockDataFetcher(MockOrientationFactory* orientation_factory)
        : orientation_factory_(orientation_factory) { }

    // From DataFetcher. Called by the Provider.
    virtual bool GetOrientation(Orientation* orientation) {
      base::AutoLock auto_lock(orientation_factory_->lock_);
      if (orientation_factory_->is_failing_)
        return false;
      *orientation = orientation_factory_->orientation_;
      return true;
    }

   private:
    scoped_refptr<MockOrientationFactory> orientation_factory_;
  };

  static MockOrientationFactory* instance_;
  Orientation orientation_;
  bool is_failing_;
  base::Lock lock_;
};

MockOrientationFactory* MockOrientationFactory::instance_;

// Mock DataFetcher that always fails to provide any orientation data.
class FailingDataFetcher : public DataFetcher {
 public:
  // Factory method; passed to and called by the ProviderImpl.
  static DataFetcher* Create() {
    return new FailingDataFetcher();
  }

  // From DataFetcher.
  virtual bool GetOrientation(Orientation* orientation) {
    return false;
  }

 private:
  FailingDataFetcher() {}
};

class DeviceOrientationProviderTest : public testing::Test {
 public:
  DeviceOrientationProviderTest()
      : pending_expectations_(0) {
  }

  virtual void TearDown() {
    provider_ = NULL;

    // Make sure it is really gone.
    EXPECT_FALSE(Provider::GetInstanceForTests());

    // Clean up in any case, so as to not break subsequent test.
    Provider::SetInstanceForTests(NULL);
  }

  // Initialize the test fixture with a ProviderImpl that uses the
  // DataFetcherFactories in the null-terminated factories array.
  void Init(ProviderImpl::DataFetcherFactory* factories) {
    provider_ = new ProviderImpl(factories);
    Provider::SetInstanceForTests(provider_);
  }

  // Initialize the test fixture with a ProviderImpl that uses the
  // DataFetcherFactory factory.
  void Init(ProviderImpl::DataFetcherFactory factory) {
    ProviderImpl::DataFetcherFactory factories[] = { factory, NULL };
    Init(factories);
  }

 protected:
  // Number of pending expectations.
  int pending_expectations_;

  // Provider instance under test.
  scoped_refptr<Provider> provider_;

  // Message loop for the test thread.
  MessageLoop message_loop_;
};

TEST_F(DeviceOrientationProviderTest, FailingTest) {
  Init(FailingDataFetcher::Create);

  scoped_ptr<UpdateChecker> checker_a(
      new UpdateChecker(&pending_expectations_));
  scoped_ptr<UpdateChecker> checker_b(
      new UpdateChecker(&pending_expectations_));

  checker_a->AddExpectation(Orientation::Empty());
  provider_->AddObserver(checker_a.get());
  MessageLoop::current()->Run();

  checker_b->AddExpectation(Orientation::Empty());
  provider_->AddObserver(checker_b.get());
  MessageLoop::current()->Run();
}

TEST_F(DeviceOrientationProviderTest, ProviderIsSingleton) {
  Init(FailingDataFetcher::Create);

  scoped_refptr<Provider> provider_a(Provider::GetInstance());
  scoped_refptr<Provider> provider_b(Provider::GetInstance());

  EXPECT_EQ(provider_a.get(), provider_b.get());
}

TEST_F(DeviceOrientationProviderTest, BasicPushTest) {
  scoped_refptr<MockOrientationFactory> orientation_factory(
      new MockOrientationFactory());
  Init(MockOrientationFactory::CreateDataFetcher);
  Orientation test_orientation;
  test_orientation.set_alpha(1);
  test_orientation.set_beta(2);
  test_orientation.set_gamma(3);
  test_orientation.set_absolute(true);

  scoped_ptr<UpdateChecker> checker(new UpdateChecker(&pending_expectations_));
  checker->AddExpectation(test_orientation);
  orientation_factory->SetOrientation(test_orientation);
  provider_->AddObserver(checker.get());
  MessageLoop::current()->Run();

  provider_->RemoveObserver(checker.get());
}

TEST_F(DeviceOrientationProviderTest, MultipleObserversPushTest) {
  scoped_refptr<MockOrientationFactory> orientation_factory(
      new MockOrientationFactory());
  Init(MockOrientationFactory::CreateDataFetcher);

  Orientation test_orientations[] = {Orientation(), Orientation(),
                                     Orientation()};
  test_orientations[0].set_alpha(1);
  test_orientations[0].set_beta(2);
  test_orientations[0].set_gamma(3);
  test_orientations[0].set_absolute(true);

  test_orientations[1].set_alpha(4);
  test_orientations[1].set_beta(5);
  test_orientations[1].set_gamma(6);
  test_orientations[1].set_absolute(false);

  test_orientations[2].set_alpha(7);
  test_orientations[2].set_beta(8);
  test_orientations[2].set_gamma(9);
  // can't provide absolute

  scoped_ptr<UpdateChecker> checker_a(
      new UpdateChecker(&pending_expectations_));
  scoped_ptr<UpdateChecker> checker_b(
      new UpdateChecker(&pending_expectations_));
  scoped_ptr<UpdateChecker> checker_c(
      new UpdateChecker(&pending_expectations_));

  checker_a->AddExpectation(test_orientations[0]);
  orientation_factory->SetOrientation(test_orientations[0]);
  provider_->AddObserver(checker_a.get());
  MessageLoop::current()->Run();

  checker_a->AddExpectation(test_orientations[1]);
  checker_b->AddExpectation(test_orientations[0]);
  checker_b->AddExpectation(test_orientations[1]);
  orientation_factory->SetOrientation(test_orientations[1]);
  provider_->AddObserver(checker_b.get());
  MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_a.get());
  checker_b->AddExpectation(test_orientations[2]);
  checker_c->AddExpectation(test_orientations[1]);
  checker_c->AddExpectation(test_orientations[2]);
  orientation_factory->SetOrientation(test_orientations[2]);
  provider_->AddObserver(checker_c.get());
  MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_b.get());
  provider_->RemoveObserver(checker_c.get());
}

#if defined(OS_LINUX) || defined(OS_WIN)
// Flakily DCHECKs on Linux. See crbug.com/104950.
// FLAKY on Win. See crbug.com/104950.
#define MAYBE_ObserverNotRemoved DISABLED_ObserverNotRemoved
#else
#define MAYBE_ObserverNotRemoved ObserverNotRemoved
#endif
TEST_F(DeviceOrientationProviderTest, MAYBE_ObserverNotRemoved) {
  scoped_refptr<MockOrientationFactory> orientation_factory(
      new MockOrientationFactory());
  Init(MockOrientationFactory::CreateDataFetcher);
  Orientation test_orientation;
  test_orientation.set_alpha(1);
  test_orientation.set_beta(2);
  test_orientation.set_gamma(3);
  test_orientation.set_absolute(true);

  Orientation test_orientation2;
  test_orientation2.set_alpha(4);
  test_orientation2.set_beta(5);
  test_orientation2.set_gamma(6);
  test_orientation2.set_absolute(false);

  scoped_ptr<UpdateChecker> checker(new UpdateChecker(&pending_expectations_));
  checker->AddExpectation(test_orientation);
  orientation_factory->SetOrientation(test_orientation);
  provider_->AddObserver(checker.get());
  MessageLoop::current()->Run();

  checker->AddExpectation(test_orientation2);
  orientation_factory->SetOrientation(test_orientation2);
  MessageLoop::current()->Run();

  // Note that checker is not removed. This should not be a problem.
}

#if defined(OS_WIN)
// FLAKY on Win. See crbug.com/104950.
#define MAYBE_StartFailing DISABLED_StartFailing
#else
#define MAYBE_StartFailing StartFailing
#endif
TEST_F(DeviceOrientationProviderTest, MAYBE_StartFailing) {
  scoped_refptr<MockOrientationFactory> orientation_factory(
      new MockOrientationFactory());
  Init(MockOrientationFactory::CreateDataFetcher);
  Orientation test_orientation;
  test_orientation.set_alpha(1);
  test_orientation.set_beta(2);
  test_orientation.set_gamma(3);
  test_orientation.set_absolute(true);

  scoped_ptr<UpdateChecker> checker_a(new UpdateChecker(
      &pending_expectations_));
  scoped_ptr<UpdateChecker> checker_b(new UpdateChecker(
      &pending_expectations_));

  orientation_factory->SetOrientation(test_orientation);
  checker_a->AddExpectation(test_orientation);
  provider_->AddObserver(checker_a.get());
  MessageLoop::current()->Run();

  checker_a->AddExpectation(Orientation::Empty());
  orientation_factory->SetFailing(true);
  MessageLoop::current()->Run();

  checker_b->AddExpectation(Orientation::Empty());
  provider_->AddObserver(checker_b.get());
  MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_a.get());
  provider_->RemoveObserver(checker_b.get());
}

TEST_F(DeviceOrientationProviderTest, StartStopStart) {
  scoped_refptr<MockOrientationFactory> orientation_factory(
      new MockOrientationFactory());
  Init(MockOrientationFactory::CreateDataFetcher);

  Orientation test_orientation;
  test_orientation.set_alpha(1);
  test_orientation.set_beta(2);
  test_orientation.set_gamma(3);
  test_orientation.set_absolute(true);

  Orientation test_orientation2;
  test_orientation2.set_alpha(4);
  test_orientation2.set_beta(5);
  test_orientation2.set_gamma(6);
  test_orientation2.set_absolute(false);

  scoped_ptr<UpdateChecker> checker_a(new UpdateChecker(
      &pending_expectations_));
  scoped_ptr<UpdateChecker> checker_b(new UpdateChecker(
      &pending_expectations_));

  checker_a->AddExpectation(test_orientation);
  orientation_factory->SetOrientation(test_orientation);
  provider_->AddObserver(checker_a.get());
  MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_a.get()); // This stops the Provider.

  checker_b->AddExpectation(test_orientation2);
  orientation_factory->SetOrientation(test_orientation2);
  provider_->AddObserver(checker_b.get());
  MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_b.get());
}

TEST_F(DeviceOrientationProviderTest, SignificantlyDifferent) {
  scoped_refptr<MockOrientationFactory> orientation_factory(
      new MockOrientationFactory());
  Init(MockOrientationFactory::CreateDataFetcher);

  // Values that should be well below or above the implementation's
  // significane threshold.
  const double kInsignificantDifference = 1e-6;
  const double kSignificantDifference = 30;
  const double kAlpha = 4, kBeta = 5, kGamma = 6;

  Orientation first_orientation;
  first_orientation.set_alpha(kAlpha);
  first_orientation.set_beta(kBeta);
  first_orientation.set_gamma(kGamma);
  first_orientation.set_absolute(true);

  Orientation second_orientation;
  second_orientation.set_alpha(kAlpha + kInsignificantDifference);
  second_orientation.set_beta(kBeta + kInsignificantDifference);
  second_orientation.set_gamma(kGamma + kInsignificantDifference);
  second_orientation.set_absolute(false);

  Orientation third_orientation;
  third_orientation.set_alpha(kAlpha + kSignificantDifference);
  third_orientation.set_beta(kBeta + kSignificantDifference);
  third_orientation.set_gamma(kGamma + kSignificantDifference);
  // can't provide absolute

  scoped_ptr<UpdateChecker> checker_a(new UpdateChecker(
      &pending_expectations_));
  scoped_ptr<UpdateChecker> checker_b(new UpdateChecker(
      &pending_expectations_));


  orientation_factory->SetOrientation(first_orientation);
  checker_a->AddExpectation(first_orientation);
  provider_->AddObserver(checker_a.get());
  MessageLoop::current()->Run();

  // The observers should not see this insignificantly different orientation.
  orientation_factory->SetOrientation(second_orientation);
  checker_b->AddExpectation(first_orientation);
  provider_->AddObserver(checker_b.get());
  MessageLoop::current()->Run();

  orientation_factory->SetOrientation(third_orientation);
  checker_a->AddExpectation(third_orientation);
  checker_b->AddExpectation(third_orientation);
  MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_a.get());
  provider_->RemoveObserver(checker_b.get());
}

}  // namespace

}  // namespace device_orientation
