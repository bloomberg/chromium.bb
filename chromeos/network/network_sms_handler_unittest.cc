// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_sms_handler.h"

#include <set>
#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class TestObserver : public NetworkSmsHandler::Observer {
 public:
  TestObserver() {}
  virtual ~TestObserver() {}

  virtual void MessageReceived(const base::DictionaryValue& message) OVERRIDE {
    std::string text;
    if (message.GetStringWithoutPathExpansion(
            NetworkSmsHandler::kTextKey, &text)) {
      messages_.insert(text);
    }
  }

  void ClearMessages() {
    messages_.clear();
  }

  int message_count() { return messages_.size(); }
  const std::set<std::string>& messages() const {
    return messages_;
  }

 private:
  std::set<std::string> messages_;
};

}  // namespace

class NetworkSmsHandlerTest : public testing::Test {
 public:
  NetworkSmsHandlerTest() {}
  virtual ~NetworkSmsHandlerTest() {}

  virtual void SetUp() OVERRIDE {
    // Initialize DBusThreadManager with a stub implementation.
    DBusThreadManager::InitializeWithStub();
    ShillManagerClient::TestInterface* manager_test =
        DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface();
    ASSERT_TRUE(manager_test);
    manager_test->AddDevice("stub_cellular_device2");
    ShillDeviceClient::TestInterface* device_test =
        DBusThreadManager::Get()->GetShillDeviceClient()->GetTestInterface();
    ASSERT_TRUE(device_test);
    device_test->AddDevice("stub_cellular_device2", flimflam::kTypeCellular,
                           "/org/freedesktop/ModemManager1/stub/0");
  }

  virtual void TearDown() OVERRIDE {
    DBusThreadManager::Shutdown();
  }

 protected:
  MessageLoopForUI message_loop_;
};

TEST_F(NetworkSmsHandlerTest, SmsHandlerDbusStub) {
  // Append '--sms-test-messages' to the command line to tell SMSClientStubImpl
  // to generate a series of test SMS messages.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(chromeos::switches::kSmsTestMessages);

  // This relies on the stub dbus implementations for ShillManagerClient,
  // ShillDeviceClient, GsmSMSClient, ModemMessagingClient and SMSClient.
  // Initialize a sms handler. The stub dbus clients will not send the
  // first test message until RequestUpdate has been called.
  scoped_ptr<NetworkSmsHandler> sms_handler(new NetworkSmsHandler());
  scoped_ptr<TestObserver> test_observer(new TestObserver());
  sms_handler->AddObserver(test_observer.get());
  sms_handler->Init();
  message_loop_.RunUntilIdle();
  EXPECT_EQ(test_observer->message_count(), 0);

  // Test that no messages have been received yet
  const std::set<std::string>& messages(test_observer->messages());
  // Note: The following string corresponds to values in
  // ModemMessagingClientStubImpl and SmsClientStubImpl.
  // TODO(stevenjb): Use a TestInterface to set this up to remove dependency.
  const char kMessage1[] = "SMSClientStubImpl: Test Message: /SMS/0";
  EXPECT_EQ(messages.find(kMessage1), messages.end());

  // Test for messages delivered by signals.
  test_observer->ClearMessages();
  sms_handler->RequestUpdate();
  message_loop_.RunUntilIdle();
  EXPECT_GE(test_observer->message_count(), 1);
  EXPECT_NE(messages.find(kMessage1), messages.end());
}

}  // namespace chromeos
