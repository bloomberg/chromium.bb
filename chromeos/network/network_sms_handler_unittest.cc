// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_sms_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class TestObserver : public NetworkSmsHandler::Observer {
 public:
  TestObserver() : message_count_(0) {}
  virtual ~TestObserver() {}

  virtual void MessageReceived(const base::DictionaryValue& message) OVERRIDE {
    ++message_count_;
  }

  int message_count() { return message_count_; }

 private:
  int message_count_;
};

}  // namespace

class NetworkSmsHandlerTest : public testing::Test {
 public:
  NetworkSmsHandlerTest() {}
  virtual ~NetworkSmsHandlerTest() {}

  virtual void SetUp() OVERRIDE {
    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::InitializeWithStub();
  }

  virtual void TearDown() OVERRIDE {
    DBusThreadManager::Shutdown();
  }

 protected:
  MessageLoopForUI message_loop_;
};

TEST_F(NetworkSmsHandlerTest, SmsHandlerDbusStub) {
  // This relies on the stub dbus implementations for FlimflamManagerClient,
  // FlimflamDeviceClient, and GsmSMSClient.
  // Initialize a sms handler. The stub dbus clients will send the first test
  // message when Gsm.SMS.List is called in NetworkSmsHandler::Init.
  scoped_ptr<NetworkSmsHandler> sms_handler(new NetworkSmsHandler());
  scoped_ptr<TestObserver> test_observer(new TestObserver());
  sms_handler->AddObserver(test_observer.get());
  sms_handler->Init();
  message_loop_.RunAllPending();
  sms_handler->RequestUpdate();
  message_loop_.RunAllPending();
  EXPECT_GE(test_observer->message_count(), 1);
}

}  // namespace chromeos
