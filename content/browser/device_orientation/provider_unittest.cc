// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <queue>

#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "content/browser/device_orientation/data_fetcher.h"
#include "content/browser/device_orientation/device_data.h"
#include "content/browser/device_orientation/motion.h"
#include "content/browser/device_orientation/orientation.h"
#include "content/browser/device_orientation/provider.h"
#include "content/browser/device_orientation/provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

// Class for testing multiple types of device data.
class TestData : public DeviceData {
 public:
  TestData()
      : value_(0) {
  }

  // From DeviceData.
  virtual IPC::Message* CreateIPCMessage(int render_view_id) const OVERRIDE {
    NOTREACHED();
    return NULL;
  }
  virtual bool ShouldFireEvent(const DeviceData* old_data) const OVERRIDE {
    return true;
  }

  void set_value(double value) { value_ = value; }
  double value() const { return value_; }

 private:
  virtual ~TestData() { }

  double value_;
};

// Class for checking expectations on device_data updates from the Provider.
class UpdateChecker : public Provider::Observer {
 public:
  UpdateChecker(DeviceData::Type device_data_type,
                int *expectations_count_ptr)
      : Observer(device_data_type),
        expectations_count_ptr_(expectations_count_ptr) {
  }
  virtual ~UpdateChecker() {}

  // From Provider::Observer.
  virtual void OnDeviceDataUpdate(const DeviceData* device_data,
      DeviceData::Type device_data_type) OVERRIDE = 0;

  void AddExpectation(const DeviceData* device_data) {
    scoped_refptr<const DeviceData> expected_device_data(device_data);
    expectations_queue_.push(expected_device_data);
    ++(*expectations_count_ptr_);
  }

 protected:
  // Set up by the test fixture, which then blocks while it is accessed
  // from OnDeviceDataUpdate which is executed on the test fixture's
  // message_loop_.
  int* expectations_count_ptr_;
  std::queue<scoped_refptr<const DeviceData> > expectations_queue_;
};

// Class for checking expectations on motion updates from the Provider.
class MotionUpdateChecker : public UpdateChecker {
 public:
  explicit MotionUpdateChecker(int* expectations_count_ptr)
      : UpdateChecker(DeviceData::kTypeMotion, expectations_count_ptr) {
  }

  virtual ~MotionUpdateChecker() {}

  // From UpdateChecker.
  virtual void OnDeviceDataUpdate(const DeviceData* device_data,
      DeviceData::Type device_data_type) OVERRIDE {
    ASSERT_FALSE(expectations_queue_.empty());
    ASSERT_EQ(DeviceData::kTypeMotion, device_data_type);

    scoped_refptr<const Motion> motion(static_cast<const Motion*>(device_data));
    if (motion == NULL)
      motion = new Motion();

    scoped_refptr<const Motion> expected(static_cast<const Motion*>(
        (expectations_queue_.front().get())));
    expectations_queue_.pop();

    EXPECT_EQ(expected->can_provide_acceleration_x(),
              motion->can_provide_acceleration_x());
    EXPECT_EQ(expected->can_provide_acceleration_y(),
              motion->can_provide_acceleration_y());
    EXPECT_EQ(expected->can_provide_acceleration_z(),
              motion->can_provide_acceleration_z());

    EXPECT_EQ(expected->can_provide_acceleration_including_gravity_x(),
              motion->can_provide_acceleration_including_gravity_x());
    EXPECT_EQ(expected->can_provide_acceleration_including_gravity_y(),
              motion->can_provide_acceleration_including_gravity_y());
    EXPECT_EQ(expected->can_provide_acceleration_including_gravity_z(),
              motion->can_provide_acceleration_including_gravity_z());

    EXPECT_EQ(expected->can_provide_rotation_rate_alpha(),
              motion->can_provide_rotation_rate_alpha());
    EXPECT_EQ(expected->can_provide_rotation_rate_beta(),
              motion->can_provide_rotation_rate_beta());
    EXPECT_EQ(expected->can_provide_rotation_rate_gamma(),
              motion->can_provide_rotation_rate_gamma());

    EXPECT_EQ(expected->can_provide_interval(), motion->can_provide_interval());

    if (expected->can_provide_acceleration_x())
      EXPECT_EQ(expected->acceleration_x(), motion->acceleration_x());
    if (expected->can_provide_acceleration_y())
      EXPECT_EQ(expected->acceleration_y(), motion->acceleration_y());
    if (expected->can_provide_acceleration_z())
      EXPECT_EQ(expected->acceleration_z(), motion->acceleration_z());

    if (expected->can_provide_acceleration_including_gravity_x())
      EXPECT_EQ(expected->acceleration_including_gravity_x(),
                motion->acceleration_including_gravity_x());
    if (expected->can_provide_acceleration_including_gravity_y())
      EXPECT_EQ(expected->acceleration_including_gravity_y(),
                motion->acceleration_including_gravity_y());
    if (expected->can_provide_acceleration_including_gravity_z())
      EXPECT_EQ(expected->acceleration_including_gravity_z(),
                motion->acceleration_including_gravity_z());

    if (expected->can_provide_rotation_rate_alpha())
      EXPECT_EQ(expected->rotation_rate_alpha(),
                motion->rotation_rate_alpha());
    if (expected->can_provide_rotation_rate_beta())
      EXPECT_EQ(expected->rotation_rate_beta(),
                motion->rotation_rate_beta());
    if (expected->can_provide_rotation_rate_gamma())
      EXPECT_EQ(expected->rotation_rate_gamma(),
                motion->rotation_rate_gamma());

    if (expected->can_provide_interval())
      EXPECT_EQ(expected->interval(), motion->interval());

    --(*expectations_count_ptr_);

    if (*expectations_count_ptr_ == 0) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::MessageLoop::QuitClosure());
    }
  }
};

// Class for checking expectations on orientation updates from the Provider.
class OrientationUpdateChecker : public UpdateChecker {
 public:
  explicit OrientationUpdateChecker(int* expectations_count_ptr)
      : UpdateChecker(DeviceData::kTypeOrientation, expectations_count_ptr) {
  }

  virtual ~OrientationUpdateChecker() {}

  // From UpdateChecker.
  virtual void OnDeviceDataUpdate(const DeviceData* device_data,
      DeviceData::Type device_data_type) OVERRIDE {
    ASSERT_FALSE(expectations_queue_.empty());
    ASSERT_EQ(DeviceData::kTypeOrientation, device_data_type);

    scoped_refptr<const Orientation> orientation(
        static_cast<const Orientation*>(device_data));
    if (orientation == NULL)
      orientation = new Orientation();

    scoped_refptr<const Orientation> expected(static_cast<const Orientation*>(
        (expectations_queue_.front().get())));
    expectations_queue_.pop();

    EXPECT_EQ(expected->can_provide_alpha(), orientation->can_provide_alpha());
    EXPECT_EQ(expected->can_provide_beta(),  orientation->can_provide_beta());
    EXPECT_EQ(expected->can_provide_gamma(), orientation->can_provide_gamma());
    EXPECT_EQ(expected->can_provide_absolute(),
              orientation->can_provide_absolute());
    if (expected->can_provide_alpha())
      EXPECT_EQ(expected->alpha(), orientation->alpha());
    if (expected->can_provide_beta())
      EXPECT_EQ(expected->beta(), orientation->beta());
    if (expected->can_provide_gamma())
      EXPECT_EQ(expected->gamma(), orientation->gamma());
    if (expected->can_provide_absolute())
      EXPECT_EQ(expected->absolute(), orientation->absolute());

    --(*expectations_count_ptr_);

    if (*expectations_count_ptr_ == 0) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::MessageLoop::QuitClosure());
    }
  }
};

// Class for checking expectations on test_data updates from the Provider.
class TestDataUpdateChecker : public UpdateChecker {
 public:
  explicit TestDataUpdateChecker(int* expectations_count_ptr)
      : UpdateChecker(DeviceData::kTypeTest, expectations_count_ptr) {
  }

  // From UpdateChecker.
  virtual void OnDeviceDataUpdate(const DeviceData* device_data,
      DeviceData::Type device_data_type) OVERRIDE {
    ASSERT_FALSE(expectations_queue_.empty());
    ASSERT_EQ(DeviceData::kTypeTest, device_data_type);

    scoped_refptr<const TestData> test_data(
        static_cast<const TestData*>(device_data));
    if (test_data == NULL)
      test_data = new TestData();

    scoped_refptr<const TestData> expected(static_cast<const TestData*>(
        (expectations_queue_.front().get())));
    expectations_queue_.pop();

    EXPECT_EQ(expected->value(), test_data->value());

    --(*expectations_count_ptr_);

    if (*expectations_count_ptr_ == 0) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::MessageLoop::QuitClosure());
    }
  }
};

// Class for injecting test device data into the Provider.
class MockDeviceDataFactory
    : public base::RefCountedThreadSafe<MockDeviceDataFactory> {
 public:
  MockDeviceDataFactory()
      : is_failing_(false) {
  }

  static void SetCurInstance(MockDeviceDataFactory* instance) {
    if (instance) {
      EXPECT_FALSE(instance_);
    }
    else {
      EXPECT_TRUE(instance_);
    }
    instance_ = instance;
  }

  static DataFetcher* CreateDataFetcher() {
    EXPECT_TRUE(instance_);
    return new MockDataFetcher(instance_);
  }

  void SetDeviceData(const DeviceData* device_data, DeviceData::Type type) {
    base::AutoLock auto_lock(lock_);
    device_data_map_[type] = device_data;
  }

  void SetFailing(bool is_failing) {
    base::AutoLock auto_lock(lock_);
    is_failing_ = is_failing;
  }

 private:
  friend class base::RefCountedThreadSafe<MockDeviceDataFactory>;

  ~MockDeviceDataFactory() {
  }

  // Owned by ProviderImpl. Holds a reference back to MockDeviceDataFactory.
  class MockDataFetcher : public DataFetcher {
   public:
    explicit MockDataFetcher(MockDeviceDataFactory* device_data_factory)
        : device_data_factory_(device_data_factory) { }

    // From DataFetcher. Called by the Provider.
    virtual const DeviceData* GetDeviceData(
        DeviceData::Type device_data_type) OVERRIDE {
      base::AutoLock auto_lock(device_data_factory_->lock_);
      if (device_data_factory_->is_failing_)
        return NULL;
      return device_data_factory_->device_data_map_[device_data_type];
    }

   private:
    scoped_refptr<MockDeviceDataFactory> device_data_factory_;
  };

  static MockDeviceDataFactory* instance_;
  std::map<DeviceData::Type, scoped_refptr<const DeviceData> > device_data_map_;
  bool is_failing_;
  base::Lock lock_;
};

MockDeviceDataFactory* MockDeviceDataFactory::instance_;

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
  // DataFetcherFactory factory.
  void Init(ProviderImpl::DataFetcherFactory factory) {
    provider_ = new ProviderImpl(factory);
    Provider::SetInstanceForTests(provider_);
  }

 protected:
  // Number of pending expectations.
  int pending_expectations_;

  // Provider instance under test.
  scoped_refptr<Provider> provider_;

  // Message loop for the test thread.
  base::MessageLoop message_loop_;
};

TEST_F(DeviceOrientationProviderTest, FailingTest) {
  scoped_refptr<MockDeviceDataFactory> device_data_factory(
      new MockDeviceDataFactory());
  MockDeviceDataFactory::SetCurInstance(device_data_factory.get());
  Init(MockDeviceDataFactory::CreateDataFetcher);

  scoped_ptr<OrientationUpdateChecker> checker_a(
      new OrientationUpdateChecker(&pending_expectations_));
  scoped_ptr<OrientationUpdateChecker> checker_b(
      new OrientationUpdateChecker(&pending_expectations_));

  checker_a->AddExpectation(new Orientation());
  provider_->AddObserver(checker_a.get());
  base::MessageLoop::current()->Run();

  checker_b->AddExpectation(new Orientation());
  provider_->AddObserver(checker_b.get());
  base::MessageLoop::current()->Run();

  MockDeviceDataFactory::SetCurInstance(NULL);
}

TEST_F(DeviceOrientationProviderTest, ProviderIsSingleton) {
  scoped_refptr<MockDeviceDataFactory> device_data_factory(
      new MockDeviceDataFactory());
  MockDeviceDataFactory::SetCurInstance(device_data_factory.get());
  Init(MockDeviceDataFactory::CreateDataFetcher);

  scoped_refptr<Provider> provider_a(Provider::GetInstance());
  scoped_refptr<Provider> provider_b(Provider::GetInstance());

  EXPECT_EQ(provider_a.get(), provider_b.get());
  MockDeviceDataFactory::SetCurInstance(NULL);
}

TEST_F(DeviceOrientationProviderTest, BasicPushTest) {
  scoped_refptr<MockDeviceDataFactory> device_data_factory(
      new MockDeviceDataFactory());
  MockDeviceDataFactory::SetCurInstance(device_data_factory.get());
  Init(MockDeviceDataFactory::CreateDataFetcher);
  scoped_refptr<Orientation> test_orientation(new Orientation());
  test_orientation->set_alpha(1);
  test_orientation->set_beta(2);
  test_orientation->set_gamma(3);
  test_orientation->set_absolute(true);

  scoped_ptr<OrientationUpdateChecker> checker(
      new OrientationUpdateChecker(&pending_expectations_));
  checker->AddExpectation(test_orientation);
  device_data_factory->SetDeviceData(test_orientation,
      DeviceData::kTypeOrientation);
  provider_->AddObserver(checker.get());
  base::MessageLoop::current()->Run();

  provider_->RemoveObserver(checker.get());
  MockDeviceDataFactory::SetCurInstance(NULL);
}

// Tests multiple observers observing the same type of data.
TEST_F(DeviceOrientationProviderTest, MultipleObserversPushTest) {
  scoped_refptr<MockDeviceDataFactory> device_data_factory(
      new MockDeviceDataFactory());
  MockDeviceDataFactory::SetCurInstance(device_data_factory.get());
  Init(MockDeviceDataFactory::CreateDataFetcher);

  scoped_refptr<Orientation> test_orientations[] = {new Orientation(),
      new Orientation(), new Orientation()};
  test_orientations[0]->set_alpha(1);
  test_orientations[0]->set_beta(2);
  test_orientations[0]->set_gamma(3);
  test_orientations[0]->set_absolute(true);

  test_orientations[1]->set_alpha(4);
  test_orientations[1]->set_beta(5);
  test_orientations[1]->set_gamma(6);
  test_orientations[1]->set_absolute(false);

  test_orientations[2]->set_alpha(7);
  test_orientations[2]->set_beta(8);
  test_orientations[2]->set_gamma(9);
  // can't provide absolute

  scoped_ptr<OrientationUpdateChecker> checker_a(
      new OrientationUpdateChecker(&pending_expectations_));
  scoped_ptr<OrientationUpdateChecker> checker_b(
      new OrientationUpdateChecker(&pending_expectations_));
  scoped_ptr<OrientationUpdateChecker> checker_c(
      new OrientationUpdateChecker(&pending_expectations_));

  checker_a->AddExpectation(test_orientations[0]);
  device_data_factory->SetDeviceData(test_orientations[0],
      DeviceData::kTypeOrientation);
  provider_->AddObserver(checker_a.get());
  base::MessageLoop::current()->Run();

  checker_a->AddExpectation(test_orientations[1]);
  checker_b->AddExpectation(test_orientations[0]);
  checker_b->AddExpectation(test_orientations[1]);
  device_data_factory->SetDeviceData(test_orientations[1],
      DeviceData::kTypeOrientation);
  provider_->AddObserver(checker_b.get());
  base::MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_a.get());
  checker_b->AddExpectation(test_orientations[2]);
  checker_c->AddExpectation(test_orientations[1]);
  checker_c->AddExpectation(test_orientations[2]);
  device_data_factory->SetDeviceData(test_orientations[2],
      DeviceData::kTypeOrientation);
  provider_->AddObserver(checker_c.get());
  base::MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_b.get());
  provider_->RemoveObserver(checker_c.get());
  MockDeviceDataFactory::SetCurInstance(NULL);
}

// Test for when the fetcher cannot provide the first type of data but can
// provide the second type.
TEST_F(DeviceOrientationProviderTest, FailingFirstDataTypeTest) {

  scoped_refptr<MockDeviceDataFactory> device_data_factory(
      new MockDeviceDataFactory());
  MockDeviceDataFactory::SetCurInstance(device_data_factory.get());
  Init(MockDeviceDataFactory::CreateDataFetcher);

  scoped_ptr<TestDataUpdateChecker> test_data_checker(
      new TestDataUpdateChecker(&pending_expectations_));
  scoped_ptr<OrientationUpdateChecker> orientation_checker(
      new OrientationUpdateChecker(&pending_expectations_));

  scoped_refptr<Orientation> test_orientation(new Orientation());
  test_orientation->set_alpha(1);
  test_orientation->set_beta(2);
  test_orientation->set_gamma(3);
  test_orientation->set_absolute(true);

  test_data_checker->AddExpectation(new TestData());
  provider_->AddObserver(test_data_checker.get());
  base::MessageLoop::current()->Run();

  orientation_checker->AddExpectation(test_orientation);
  device_data_factory->SetDeviceData(test_orientation,
      DeviceData::kTypeOrientation);
  provider_->AddObserver(orientation_checker.get());
  base::MessageLoop::current()->Run();

  provider_->RemoveObserver(test_data_checker.get());
  provider_->RemoveObserver(orientation_checker.get());
  MockDeviceDataFactory::SetCurInstance(NULL);
}

#if defined(OS_LINUX) || defined(OS_WIN)
// Flakily DCHECKs on Linux. See crbug.com/104950.
// FLAKY on Win. See crbug.com/104950.
#define MAYBE_ObserverNotRemoved DISABLED_ObserverNotRemoved
#else
#define MAYBE_ObserverNotRemoved ObserverNotRemoved
#endif
TEST_F(DeviceOrientationProviderTest, MAYBE_ObserverNotRemoved) {
  scoped_refptr<MockDeviceDataFactory> device_data_factory(
      new MockDeviceDataFactory());
  MockDeviceDataFactory::SetCurInstance(device_data_factory.get());
  Init(MockDeviceDataFactory::CreateDataFetcher);
  scoped_refptr<Orientation> test_orientation(new Orientation());
  test_orientation->set_alpha(1);
  test_orientation->set_beta(2);
  test_orientation->set_gamma(3);
  test_orientation->set_absolute(true);

  scoped_refptr<Orientation> test_orientation2(new Orientation());
  test_orientation2->set_alpha(4);
  test_orientation2->set_beta(5);
  test_orientation2->set_gamma(6);
  test_orientation2->set_absolute(false);

  scoped_ptr<OrientationUpdateChecker> checker(
      new OrientationUpdateChecker(&pending_expectations_));
  checker->AddExpectation(test_orientation);
  device_data_factory->SetDeviceData(test_orientation,
      DeviceData::kTypeOrientation);
  provider_->AddObserver(checker.get());
  base::MessageLoop::current()->Run();

  checker->AddExpectation(test_orientation2);
  device_data_factory->SetDeviceData(test_orientation2,
      DeviceData::kTypeOrientation);
  base::MessageLoop::current()->Run();

  MockDeviceDataFactory::SetCurInstance(NULL);

  // Note that checker is not removed. This should not be a problem.
}

#if defined(OS_WIN)
// FLAKY on Win. See crbug.com/104950.
#define MAYBE_StartFailing DISABLED_StartFailing
#else
#define MAYBE_StartFailing StartFailing
#endif
TEST_F(DeviceOrientationProviderTest, MAYBE_StartFailing) {
  scoped_refptr<MockDeviceDataFactory> device_data_factory(
      new MockDeviceDataFactory());
  MockDeviceDataFactory::SetCurInstance(device_data_factory.get());
  Init(MockDeviceDataFactory::CreateDataFetcher);
  scoped_refptr<Orientation> test_orientation(new Orientation());
  test_orientation->set_alpha(1);
  test_orientation->set_beta(2);
  test_orientation->set_gamma(3);
  test_orientation->set_absolute(true);

  scoped_ptr<OrientationUpdateChecker> checker_a(new OrientationUpdateChecker(
      &pending_expectations_));
  scoped_ptr<OrientationUpdateChecker> checker_b(new OrientationUpdateChecker(
      &pending_expectations_));

  device_data_factory->SetDeviceData(test_orientation,
      DeviceData::kTypeOrientation);
  checker_a->AddExpectation(test_orientation);
  provider_->AddObserver(checker_a.get());
  base::MessageLoop::current()->Run();

  checker_a->AddExpectation(new Orientation());
  device_data_factory->SetFailing(true);
  base::MessageLoop::current()->Run();

  checker_b->AddExpectation(new Orientation());
  provider_->AddObserver(checker_b.get());
  base::MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_a.get());
  provider_->RemoveObserver(checker_b.get());
  MockDeviceDataFactory::SetCurInstance(NULL);
}

TEST_F(DeviceOrientationProviderTest, StartStopStart) {
  scoped_refptr<MockDeviceDataFactory> device_data_factory(
      new MockDeviceDataFactory());
  MockDeviceDataFactory::SetCurInstance(device_data_factory.get());
  Init(MockDeviceDataFactory::CreateDataFetcher);

  scoped_refptr<Orientation> test_orientation(new Orientation());
  test_orientation->set_alpha(1);
  test_orientation->set_beta(2);
  test_orientation->set_gamma(3);
  test_orientation->set_absolute(true);

  scoped_refptr<Orientation> test_orientation2(new Orientation());
  test_orientation2->set_alpha(4);
  test_orientation2->set_beta(5);
  test_orientation2->set_gamma(6);
  test_orientation2->set_absolute(false);

  scoped_ptr<OrientationUpdateChecker> checker_a(new OrientationUpdateChecker(
      &pending_expectations_));
  scoped_ptr<OrientationUpdateChecker> checker_b(new OrientationUpdateChecker(
      &pending_expectations_));

  checker_a->AddExpectation(test_orientation);
  device_data_factory->SetDeviceData(test_orientation,
      DeviceData::kTypeOrientation);
  provider_->AddObserver(checker_a.get());
  base::MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_a.get()); // This stops the Provider.

  checker_b->AddExpectation(test_orientation2);
  device_data_factory->SetDeviceData(test_orientation2,
      DeviceData::kTypeOrientation);
  provider_->AddObserver(checker_b.get());
  base::MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_b.get());
  MockDeviceDataFactory::SetCurInstance(NULL);
}

// Tests that Motion events always fire, even if the motion is unchanged.
TEST_F(DeviceOrientationProviderTest, FLAKY_MotionAlwaysFires) {
  scoped_refptr<MockDeviceDataFactory> device_data_factory(
      new MockDeviceDataFactory());
  MockDeviceDataFactory::SetCurInstance(device_data_factory.get());
  Init(MockDeviceDataFactory::CreateDataFetcher);

  scoped_refptr<Motion> test_motion(new Motion());
  test_motion->set_acceleration_x(1);
  test_motion->set_acceleration_y(2);
  test_motion->set_acceleration_z(3);
  test_motion->set_acceleration_including_gravity_x(4);
  test_motion->set_acceleration_including_gravity_y(5);
  test_motion->set_acceleration_including_gravity_z(6);
  test_motion->set_rotation_rate_alpha(7);
  test_motion->set_rotation_rate_beta(8);
  test_motion->set_rotation_rate_gamma(9);
  test_motion->set_interval(10);

  scoped_ptr<MotionUpdateChecker> checker(new MotionUpdateChecker(
      &pending_expectations_));

  device_data_factory->SetDeviceData(test_motion, DeviceData::kTypeMotion);
  checker->AddExpectation(test_motion);
  provider_->AddObserver(checker.get());
  base::MessageLoop::current()->Run();

  // The observer should receive the same motion again.
  device_data_factory->SetDeviceData(test_motion, DeviceData::kTypeMotion);
  checker->AddExpectation(test_motion);
  base::MessageLoop::current()->Run();

  provider_->RemoveObserver(checker.get());
  MockDeviceDataFactory::SetCurInstance(NULL);
}

// Tests that Orientation events only fire if the change is significant.
TEST_F(DeviceOrientationProviderTest, OrientationSignificantlyDifferent) {
  scoped_refptr<MockDeviceDataFactory> device_data_factory(
      new MockDeviceDataFactory());
  MockDeviceDataFactory::SetCurInstance(device_data_factory.get());
  Init(MockDeviceDataFactory::CreateDataFetcher);

  // Values that should be well below or above the implementation's
  // significane threshold.
  const double kInsignificantDifference = 1e-6;
  const double kSignificantDifference = 30;
  const double kAlpha = 4, kBeta = 5, kGamma = 6;

  scoped_refptr<Orientation> first_orientation(new Orientation());
  first_orientation->set_alpha(kAlpha);
  first_orientation->set_beta(kBeta);
  first_orientation->set_gamma(kGamma);
  first_orientation->set_absolute(true);

  scoped_refptr<Orientation> second_orientation(new Orientation());
  second_orientation->set_alpha(kAlpha + kInsignificantDifference);
  second_orientation->set_beta(kBeta + kInsignificantDifference);
  second_orientation->set_gamma(kGamma + kInsignificantDifference);
  second_orientation->set_absolute(false);

  scoped_refptr<Orientation> third_orientation(new Orientation());
  third_orientation->set_alpha(kAlpha + kSignificantDifference);
  third_orientation->set_beta(kBeta + kSignificantDifference);
  third_orientation->set_gamma(kGamma + kSignificantDifference);
  // can't provide absolute

  scoped_ptr<OrientationUpdateChecker> checker_a(new OrientationUpdateChecker(
      &pending_expectations_));
  scoped_ptr<OrientationUpdateChecker> checker_b(new OrientationUpdateChecker(
      &pending_expectations_));

  device_data_factory->SetDeviceData(first_orientation,
      DeviceData::kTypeOrientation);
  checker_a->AddExpectation(first_orientation);
  provider_->AddObserver(checker_a.get());
  base::MessageLoop::current()->Run();

  // The observers should not see this insignificantly different orientation.
  device_data_factory->SetDeviceData(second_orientation,
      DeviceData::kTypeOrientation);
  checker_b->AddExpectation(first_orientation);
  provider_->AddObserver(checker_b.get());
  base::MessageLoop::current()->Run();

  device_data_factory->SetDeviceData(third_orientation,
      DeviceData::kTypeOrientation);
  checker_a->AddExpectation(third_orientation);
  checker_b->AddExpectation(third_orientation);
  base::MessageLoop::current()->Run();

  provider_->RemoveObserver(checker_a.get());
  provider_->RemoveObserver(checker_b.get());
  MockDeviceDataFactory::SetCurInstance(NULL);
}

}  // namespace

}  // namespace content
