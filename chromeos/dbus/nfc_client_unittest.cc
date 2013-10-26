// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/nfc_adapter_client.h"
#include "chromeos/dbus/nfc_client_helpers.h"
#include "chromeos/dbus/nfc_manager_client.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Return;

namespace chromeos {

namespace {

// D-Bus service name used by the test.
const char kTestServiceName[] = "test.service.name";

// Object paths that are used for testing.
const char kTestManagerPath[] = "/test/nfc/manager";
const char kTestAdapterPath0[] = "/test/nfc/adapter0";
const char kTestAdapterPath1[] = "/test/nfc/adapter1";

class MockNfcManagerObserver : public NfcManagerClient::Observer {
 public:
  MOCK_METHOD1(AdapterAdded, void(const dbus::ObjectPath&));
  MOCK_METHOD1(AdapterRemoved, void(const dbus::ObjectPath&));
  MOCK_METHOD1(ManagerPropertyChanged, void(const std::string&));
};

class MockNfcAdapterObserver : public NfcAdapterClient::Observer {
 public:
  MOCK_METHOD1(AdapterAdded, void(const dbus::ObjectPath&));
  MOCK_METHOD1(AdapterRemoved, void(const dbus::ObjectPath&));
  MOCK_METHOD2(AdapterPropertyChanged, void(const dbus::ObjectPath&,
                                            const std::string&));
};

}  // namespace

class NfcClientTest : public testing::Test {
 public:
  NfcClientTest() : response_(NULL) {}
  virtual ~NfcClientTest() {}

  virtual void SetUp() OVERRIDE {
    // Create the mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create the mock proxies.
    mock_manager_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(),
        kTestServiceName,
        dbus::ObjectPath(kTestManagerPath));
    mock_adapter0_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(),
        kTestServiceName,
        dbus::ObjectPath(kTestAdapterPath0));
    mock_adapter1_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(),
        kTestServiceName,
        dbus::ObjectPath(kTestAdapterPath1));

    // Set expectations that use NfcClientTest::OnConnectToSignal when the
    // client connect signals on the mock proxies.
    EXPECT_CALL(*mock_manager_proxy_.get(), ConnectToSignal(_, _, _, _))
        .WillRepeatedly(Invoke(this, &NfcClientTest::OnConnectToSignal));
    EXPECT_CALL(*mock_adapter0_proxy_.get(), ConnectToSignal(_, _, _, _))
        .WillRepeatedly(Invoke(this, &NfcClientTest::OnConnectToSignal));
    EXPECT_CALL(*mock_adapter1_proxy_.get(), ConnectToSignal(_, _, _, _))
        .WillRepeatedly(Invoke(this, &NfcClientTest::OnConnectToSignal));

    // Set expectations that return our mock proxies on demand.
    EXPECT_CALL(
        *mock_bus_.get(),
        GetObjectProxy(nfc_manager::kNfcManagerServiceName,
                       dbus::ObjectPath(nfc_manager::kNfcManagerServicePath)))
        .WillRepeatedly(Return(mock_manager_proxy_.get()));
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(nfc_adapter::kNfcAdapterServiceName,
                               dbus::ObjectPath(kTestAdapterPath0)))
        .WillRepeatedly(Return(mock_adapter0_proxy_.get()));
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(nfc_adapter::kNfcAdapterServiceName,
                               dbus::ObjectPath(kTestAdapterPath1)))
        .WillRepeatedly(Return(mock_adapter1_proxy_.get()));

    // ShutdownAndBlock will be called in TearDown.
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create the clients.
    manager_client_.reset(
        NfcManagerClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION));
    adapter_client_.reset(
        NfcAdapterClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION,
                                 manager_client_.get()));
    manager_client_->Init(mock_bus_.get());
    adapter_client_->Init(mock_bus_.get());
    manager_client_->AddObserver(&mock_manager_observer_);
    adapter_client_->AddObserver(&mock_adapter_observer_);

    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    adapter_client_->RemoveObserver(&mock_adapter_observer_);
    manager_client_->RemoveObserver(&mock_manager_observer_);
    mock_bus_->ShutdownAndBlock();
  }

  void SendManagerAdapterAddedSignal(const dbus::ObjectPath& object_path) {
    dbus::Signal signal(nfc_manager::kNfcManagerInterface,
                        nfc_manager::kAdapterAddedSignal);
    dbus::MessageWriter writer(&signal);
    writer.AppendObjectPath(object_path);
    ASSERT_FALSE(manager_adapter_added_signal_callback_.is_null());
    manager_adapter_added_signal_callback_.Run(&signal);
  }

  void SendManagerAdapterRemovedSignal(const dbus::ObjectPath& object_path) {
    dbus::Signal signal(nfc_manager::kNfcManagerInterface,
                        nfc_manager::kAdapterRemovedSignal);
    dbus::MessageWriter writer(&signal);
    writer.AppendObjectPath(object_path);
    ASSERT_FALSE(manager_adapter_removed_signal_callback_.is_null());
    manager_adapter_removed_signal_callback_.Run(&signal);
  }

  MOCK_METHOD0(SuccessCallback, void(void));
  MOCK_METHOD2(ErrorCallback, void(const std::string& error_name,
                                   const std::string& error_message));

 protected:
  // The mock bus.
  scoped_refptr<dbus::MockBus> mock_bus_;
  // A message loop to emulate asynchronous behavior.
  base::MessageLoop message_loop_;
  // Response returned by mock methods.
  dbus::Response* response_;
  // The D-Bus client objects under test.
  scoped_ptr<NfcManagerClient> manager_client_;
  scoped_ptr<NfcAdapterClient> adapter_client_;
  // Mock observers.
  MockNfcManagerObserver mock_manager_observer_;
  MockNfcAdapterObserver mock_adapter_observer_;
  // The mock object proxies.
  scoped_refptr<dbus::MockObjectProxy> mock_manager_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_adapter0_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_adapter1_proxy_;
  // The signal callbacks used to simulate asychronous signals.
  dbus::ObjectProxy::SignalCallback manager_adapter_added_signal_callback_;
  dbus::ObjectProxy::SignalCallback manager_adapter_removed_signal_callback_;

 private:
  // Used to implement the mock proxy.
  void OnConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      const dbus::ObjectProxy::OnConnectedCallback& on_connected_callback) {
    if (interface_name == nfc_manager::kNfcManagerInterface) {
      if (signal_name == nfc_manager::kAdapterAddedSignal)
        manager_adapter_added_signal_callback_ = signal_callback;
      else if (signal_name == nfc_manager::kAdapterRemovedSignal)
        manager_adapter_removed_signal_callback_ = signal_callback;
    }
    message_loop_.PostTask(FROM_HERE, base::Bind(on_connected_callback,
                                                 interface_name,
                                                 signal_name,
                                                 true));
  }
};

// Tests that when adapters are added and removed through the manager, all
// observers are notified and the proxies are created and removed
// accordingly.
TEST_F(NfcClientTest, AdaptersAddedAndRemoved) {
  // Invoking methods on adapters that haven't been added should fail.
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _));
  adapter_client_->StartPollLoop(
      dbus::ObjectPath(kTestAdapterPath0),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcClientTest::SuccessCallback, base::Unretained(this)),
      base::Bind(&NfcClientTest::ErrorCallback, base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);

  // Add adapter 0.
  EXPECT_CALL(mock_manager_observer_,
              AdapterAdded(dbus::ObjectPath(kTestAdapterPath0)));
  EXPECT_CALL(mock_adapter_observer_,
              AdapterAdded(dbus::ObjectPath(kTestAdapterPath0)));
  SendManagerAdapterAddedSignal(dbus::ObjectPath(kTestAdapterPath0));
  Mock::VerifyAndClearExpectations(this);

  // Invoking methods should succeed on adapter 0 but fail on adapter 1.
  EXPECT_CALL(*mock_adapter0_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  adapter_client_->StartPollLoop(
      dbus::ObjectPath(kTestAdapterPath0),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcClientTest::SuccessCallback, base::Unretained(this)),
      base::Bind(&NfcClientTest::ErrorCallback, base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _));
  adapter_client_->StartPollLoop(
      dbus::ObjectPath(kTestAdapterPath1),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcClientTest::SuccessCallback, base::Unretained(this)),
      base::Bind(&NfcClientTest::ErrorCallback, base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);

  // Add adapter 1.
  EXPECT_CALL(mock_manager_observer_,
              AdapterAdded(dbus::ObjectPath(kTestAdapterPath1)));
  EXPECT_CALL(mock_adapter_observer_,
              AdapterAdded(dbus::ObjectPath(kTestAdapterPath1)));
  SendManagerAdapterAddedSignal(dbus::ObjectPath(kTestAdapterPath1));
  Mock::VerifyAndClearExpectations(this);

  // Invoking methods should succeed on both adapters.
  EXPECT_CALL(*mock_adapter0_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  EXPECT_CALL(*mock_adapter1_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  adapter_client_->StartPollLoop(
      dbus::ObjectPath(kTestAdapterPath0),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcClientTest::SuccessCallback, base::Unretained(this)),
      base::Bind(&NfcClientTest::ErrorCallback, base::Unretained(this)));
  adapter_client_->StartPollLoop(
      dbus::ObjectPath(kTestAdapterPath1),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcClientTest::SuccessCallback, base::Unretained(this)),
      base::Bind(&NfcClientTest::ErrorCallback, base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);

  // Remove adapter 0.
  EXPECT_CALL(mock_manager_observer_,
              AdapterRemoved(dbus::ObjectPath(kTestAdapterPath0)));
  EXPECT_CALL(mock_adapter_observer_,
              AdapterRemoved(dbus::ObjectPath(kTestAdapterPath0)));
  SendManagerAdapterRemovedSignal(dbus::ObjectPath(kTestAdapterPath0));
  Mock::VerifyAndClearExpectations(this);

  // Invoking methods should succeed on adapter 1 but fail on adapter 0.
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _));
  adapter_client_->StartPollLoop(
      dbus::ObjectPath(kTestAdapterPath0),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcClientTest::SuccessCallback, base::Unretained(this)),
      base::Bind(&NfcClientTest::ErrorCallback, base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*mock_adapter1_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  adapter_client_->StartPollLoop(
      dbus::ObjectPath(kTestAdapterPath1),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcClientTest::SuccessCallback, base::Unretained(this)),
      base::Bind(&NfcClientTest::ErrorCallback, base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);

  // Remove adapter 1.
  EXPECT_CALL(mock_manager_observer_,
              AdapterRemoved(dbus::ObjectPath(kTestAdapterPath1)));
  EXPECT_CALL(mock_adapter_observer_,
              AdapterRemoved(dbus::ObjectPath(kTestAdapterPath1)));
  SendManagerAdapterRemovedSignal(dbus::ObjectPath(kTestAdapterPath1));
  Mock::VerifyAndClearExpectations(this);

  // Invoking methods should fail on both adapters.
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _))
      .Times(2);
  adapter_client_->StartPollLoop(
      dbus::ObjectPath(kTestAdapterPath0),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcClientTest::SuccessCallback, base::Unretained(this)),
      base::Bind(&NfcClientTest::ErrorCallback, base::Unretained(this)));
  adapter_client_->StartPollLoop(
      dbus::ObjectPath(kTestAdapterPath1),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcClientTest::SuccessCallback, base::Unretained(this)),
      base::Bind(&NfcClientTest::ErrorCallback, base::Unretained(this)));
}

}  // namespace chromeos
