// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/api/dial/dial_device_data.h"
#include "chrome/browser/extensions/api/dial/dial_registry.h"
#include "chrome/browser/extensions/api/dial/dial_service.h"
#include "chrome/test/base/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using ::testing::A;
using ::testing::AtLeast;
using ::testing::Return;

namespace extensions {

class MockDialObserver : public DialRegistry::Observer {
 public:
  MOCK_METHOD1(OnDialDeviceEvent,
               void(const DialRegistry::DeviceList& devices));
};

class MockDialService : public DialService {
 public:
  MOCK_METHOD0(Discover, bool());
  MOCK_METHOD1(AddObserver, void(DialService::Observer*));
  MOCK_METHOD1(RemoveObserver, void(DialService::Observer*));
  MOCK_METHOD1(HasObserver, bool(DialService::Observer*));
 private:
  virtual ~MockDialService() {}
};

class MockDialRegistry : public DialRegistry {
 public:
  MockDialRegistry(Observer *dial_api,
                   const base::TimeDelta& refresh_interval,
                   const base::TimeDelta& expiration,
                   const size_t max_devices)
    : DialRegistry(dial_api, refresh_interval, expiration, max_devices) {
    mock_service_ = new MockDialService();
    time_ = Time::Now();
  }

  virtual ~MockDialRegistry() { }

  // Returns the mock Dial service.
  MockDialService& mock_service() {
    return *mock_service_;
  }

  // Set to mock out the current time.
  Time time_;

 protected:
  virtual base::Time Now() const OVERRIDE {
    return time_;
  }

  virtual DialService* CreateDialService() OVERRIDE {
    return mock_service_.get();
  }

 private:
  scoped_refptr<MockDialService> mock_service_;
};

class DialRegistryTest : public testing::Test {
 public:
  DialRegistryTest() {
    registry_.reset(new MockDialRegistry(&mock_observer_,
                                         TimeDelta::FromSeconds(1000),
                                         TimeDelta::FromSeconds(10),
                                         10));

    first_device_.set_device_id("first");
    first_device_.set_device_description_url(GURL("http://127.0.0.1/dd.xml"));
    first_device_.set_response_time(Time::Now());

    second_device_.set_device_id("second");
    second_device_.set_device_description_url(GURL("http://127.0.0.2/dd.xml"));
    second_device_.set_response_time(Time::Now());
  }

 protected:
  scoped_ptr<MockDialRegistry> registry_;
  MockDialObserver mock_observer_;
  DialDeviceData first_device_;
  DialDeviceData second_device_;
  // Must instantiate a MessageLoop for the thread, as the registry starts a
  // RepeatingTimer when there are listeners.
  MessageLoop message_loop_;

  void SetListenerExpectations() {
    EXPECT_CALL(registry_->mock_service(),
                AddObserver(A<DialService::Observer*>()))
      .Times(1);
    EXPECT_CALL(registry_->mock_service(),
                RemoveObserver(A<DialService::Observer*>()))
      .Times(1);
  }
};

TEST_F(DialRegistryTest, TestAddRemoveListeners) {
  SetListenerExpectations();
  EXPECT_CALL(registry_->mock_service(), Discover())
    .Times(1);

  EXPECT_FALSE(registry_->repeating_timer_.IsRunning());
  registry_->OnListenerAdded();
  EXPECT_TRUE(registry_->repeating_timer_.IsRunning());
  registry_->OnListenerAdded();
  EXPECT_TRUE(registry_->repeating_timer_.IsRunning());
  registry_->OnListenerRemoved();
  EXPECT_TRUE(registry_->repeating_timer_.IsRunning());
  registry_->OnListenerRemoved();
  EXPECT_FALSE(registry_->repeating_timer_.IsRunning());
}

TEST_F(DialRegistryTest, TestNoDevicesDiscovered) {
  DialRegistry::DeviceList empty_list;
  SetListenerExpectations();
  EXPECT_CALL(registry_->mock_service(), Discover())
    .Times(1);

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDiscoveryFinished(NULL);
  registry_->OnListenerRemoved();
};

TEST_F(DialRegistryTest, TestDevicesDiscovered) {
  DialRegistry::DeviceList empty_list;

  DialRegistry::DeviceList expected_list1;
  expected_list1.push_back(first_device_);

  DialRegistry::DeviceList expected_list2;
  expected_list2.push_back(first_device_);
  expected_list2.push_back(second_device_);

  SetListenerExpectations();
  EXPECT_CALL(registry_->mock_service(), Discover())
    .Times(2);
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list)).
    Times(1);
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(expected_list1)).
    Times(2);
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(expected_list2)).
    Times(1);

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, first_device_);
  registry_->OnDiscoveryFinished(NULL);

  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, second_device_);
  registry_->OnDiscoveryFinished(NULL);
  registry_->OnListenerRemoved();
}

TEST_F(DialRegistryTest, TestDeviceExpires) {
  DialRegistry::DeviceList empty_list;

  DialRegistry::DeviceList expected_list1;
  expected_list1.push_back(first_device_);

  SetListenerExpectations();
  EXPECT_CALL(registry_->mock_service(), Discover())
    .Times(2);
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(empty_list)).
    Times(2);
  EXPECT_CALL(mock_observer_, OnDialDeviceEvent(expected_list1)).
    Times(2);

  registry_->OnListenerAdded();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDeviceDiscovered(NULL, first_device_);
  registry_->OnDiscoveryFinished(NULL);

  registry_->time_ = Time::Now() + TimeDelta::FromSeconds(30);

  registry_->DoDiscovery();
  registry_->OnDiscoveryRequest(NULL);
  registry_->OnDiscoveryFinished(NULL);
  registry_->OnListenerRemoved();
}

}  // namespace extensions
