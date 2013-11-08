// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"

#include "chrome/browser/local_discovery/privet_notifications.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

using ::testing::_;
using ::testing::SaveArg;

namespace local_discovery {

namespace {

const char kExampleDeviceName[] = "test._privet._tcp.local";
const char kExampleDeviceHumanName[] = "Test device";
const char kExampleDeviceDescription[] = "Testing testing";
const char kExampleDeviceID[] = "__test__id";

class MockPrivetNotificationsListenerDeleagate
    : public PrivetNotificationsListener::Delegate {
 public:
  MOCK_METHOD2(PrivetNotify, void(bool multiple, bool added));
  MOCK_METHOD0(PrivetRemoveNotification, void());
};

// TODO(noamsml): Migrate this test to use a real privet info operation and a
// fake URL fetcher.
class MockPrivetInfoOperation : public PrivetInfoOperation {
 public:
  class DelegateForTests {
   public:
    virtual ~DelegateForTests() {}
    virtual void InfoOperationStarted(MockPrivetInfoOperation* operation) = 0;
  };

  MockPrivetInfoOperation(PrivetHTTPClient* client,
                          DelegateForTests* delegate_for_tests,
                          Delegate* delegate)
      : client_(client),
        delegate_for_tests_(delegate_for_tests),
        delegate_(delegate) {
  }

  virtual ~MockPrivetInfoOperation() {
  }

  virtual void Start() OVERRIDE {
    delegate_for_tests_->InfoOperationStarted(this);
  }

  virtual PrivetHTTPClient* GetHTTPClient() OVERRIDE {
    return client_;
  }

  Delegate* delegate() { return delegate_; }

 private:
  PrivetHTTPClient* client_;
  DelegateForTests* delegate_for_tests_;
  Delegate* delegate_;
};

class MockPrivetHTTPClient : public PrivetHTTPClient {
 public:
  MockPrivetHTTPClient(
      MockPrivetInfoOperation::DelegateForTests* delegate_for_tests,
      const std::string& name) : delegate_for_tests_(delegate_for_tests),
                                 name_(name) {
  }

  virtual scoped_ptr<PrivetRegisterOperation> CreateRegisterOperation(
      const std::string& user,
      PrivetRegisterOperation::Delegate* delegate) OVERRIDE {
    return scoped_ptr<PrivetRegisterOperation>();
  }

  virtual scoped_ptr<PrivetInfoOperation> CreateInfoOperation(
      PrivetInfoOperation::Delegate* delegate) OVERRIDE {
    return scoped_ptr<PrivetInfoOperation>(new MockPrivetInfoOperation(
        this, delegate_for_tests_, delegate));
  }

  virtual scoped_ptr<PrivetCapabilitiesOperation> CreateCapabilitiesOperation(
      PrivetCapabilitiesOperation::Delegate* delegate) OVERRIDE {
    NOTIMPLEMENTED();
    return scoped_ptr<PrivetCapabilitiesOperation>();
  }

  virtual scoped_ptr<PrivetLocalPrintOperation> CreateLocalPrintOperation(
      PrivetLocalPrintOperation::Delegate* delegate) OVERRIDE {
    NOTIMPLEMENTED();
    return scoped_ptr<PrivetLocalPrintOperation>();
  }

  virtual const std::string& GetName() OVERRIDE { return name_; }

  virtual const base::DictionaryValue* GetCachedInfo() const OVERRIDE {
    NOTIMPLEMENTED();
    return NULL;
  }

 private:
  MockPrivetInfoOperation::DelegateForTests* delegate_for_tests_;
  std::string name_;
};

class MockPrivetHttpFactory : public PrivetHTTPAsynchronousFactory {
 public:
  class MockResolution : public PrivetHTTPResolution {
   public:
    MockResolution(
        const std::string& name,
        MockPrivetInfoOperation::DelegateForTests* delegate_for_tests,
        const ResultCallback& callback)
        : name_(name), delegate_for_tests_(delegate_for_tests),
          callback_(callback) {
    }

    virtual ~MockResolution() {
    }

    virtual void Start() OVERRIDE {
      callback_.Run(scoped_ptr<PrivetHTTPClient>(
          new MockPrivetHTTPClient(delegate_for_tests_, name_)));
    }

    virtual const std::string& GetName() OVERRIDE {
      return name_;
    }

   private:
    std::string name_;
    MockPrivetInfoOperation::DelegateForTests* delegate_for_tests_;
    ResultCallback callback_;
  };

  MockPrivetHttpFactory(
      MockPrivetInfoOperation::DelegateForTests* delegate_for_tests)
      : delegate_for_tests_(delegate_for_tests) {
  }

  virtual scoped_ptr<PrivetHTTPResolution> CreatePrivetHTTP(
      const std::string& name,
      const net::HostPortPair& address,
      const ResultCallback& callback) OVERRIDE {
    return scoped_ptr<PrivetHTTPResolution>(
        new MockResolution(name, delegate_for_tests_, callback));
  }

 private:
  MockPrivetInfoOperation::DelegateForTests* delegate_for_tests_;
};

class MockDelegateForTests : public MockPrivetInfoOperation::DelegateForTests {
 public:
  MOCK_METHOD1(InfoOperationStarted, void(MockPrivetInfoOperation* operation));
};

class PrivetNotificationsListenerTest : public ::testing::Test {
 public:
  PrivetNotificationsListenerTest() {
    notification_listener_.reset(new PrivetNotificationsListener(
        scoped_ptr<PrivetHTTPAsynchronousFactory>(
            new MockPrivetHttpFactory(&mock_delegate_for_tests_)),
        &mock_delegate_));

    description_.name = kExampleDeviceHumanName;
    description_.description = kExampleDeviceDescription;
  }

  virtual ~PrivetNotificationsListenerTest() {
  }

  virtual void ExpectInfoOperation() {
    EXPECT_CALL(mock_delegate_for_tests_, InfoOperationStarted(_))
        .WillOnce(SaveArg<0>(&info_operation_));
  }

 protected:
  StrictMock<MockPrivetNotificationsListenerDeleagate> mock_delegate_;
  StrictMock<MockDelegateForTests> mock_delegate_for_tests_;
  scoped_ptr<PrivetNotificationsListener> notification_listener_;
  MockPrivetInfoOperation* info_operation_;
  DeviceDescription description_;
};

TEST_F(PrivetNotificationsListenerTest, DisappearReappearTest) {
  ExpectInfoOperation();

  EXPECT_CALL(mock_delegate_, PrivetNotify(
      false,
      true));

  notification_listener_->DeviceChanged(
      true,
      kExampleDeviceName,
      description_);

  base::DictionaryValue value;

  value.SetInteger("uptime", 20);

  info_operation_->delegate()->OnPrivetInfoDone(info_operation_,
                                                200, &value);

  EXPECT_CALL(mock_delegate_, PrivetRemoveNotification());

  notification_listener_->DeviceRemoved(
      kExampleDeviceName);

  notification_listener_->DeviceChanged(
      true,
      kExampleDeviceName,
      description_);

  description_.id = kExampleDeviceID;

  notification_listener_->DeviceChanged(
      true,
      kExampleDeviceName,
      description_);
}

TEST_F(PrivetNotificationsListenerTest, RegisterTest) {
  ExpectInfoOperation();

  EXPECT_CALL(mock_delegate_, PrivetNotify(
      false,
      true));

  notification_listener_->DeviceChanged(
      true,
      kExampleDeviceName,
      description_);

  base::DictionaryValue value;

  value.SetInteger("uptime", 20);

  info_operation_->delegate()->OnPrivetInfoDone(info_operation_,
                                                200, &value);

  EXPECT_CALL(mock_delegate_, PrivetRemoveNotification());

  description_.id = kExampleDeviceID;

  notification_listener_->DeviceChanged(
      true,
      kExampleDeviceName,
      description_);
}

TEST_F(PrivetNotificationsListenerTest, HighUptimeTest) {
  ExpectInfoOperation();

  notification_listener_->DeviceChanged(
      true,
      kExampleDeviceName,
      description_);

  base::DictionaryValue value;

  value.SetInteger("uptime", 3600);

  info_operation_->delegate()->OnPrivetInfoDone(info_operation_,
                                                200, &value);

  description_.id = kExampleDeviceID;

  notification_listener_->DeviceChanged(
      true,
      kExampleDeviceName,
      description_);
}

TEST_F(PrivetNotificationsListenerTest, HTTPErrorTest) {
  ExpectInfoOperation();

  notification_listener_->DeviceChanged(
      true,
      kExampleDeviceName,
      description_);

  info_operation_->delegate()->OnPrivetInfoDone(info_operation_,
                                                404, NULL);
}

TEST_F(PrivetNotificationsListenerTest, DictionaryErrorTest) {
  ExpectInfoOperation();

  notification_listener_->DeviceChanged(
      true,
      kExampleDeviceName,
      description_);

  base::DictionaryValue value;
  value.SetString("error", "internal_error");

  info_operation_->delegate()->OnPrivetInfoDone(info_operation_,
                                                200, &value);
}

}  // namespace

}  // namespace local_discovery
