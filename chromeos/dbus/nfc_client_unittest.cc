// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/nfc_adapter_client.h"
#include "chromeos/dbus/nfc_client_helpers.h"
#include "chromeos/dbus/nfc_device_client.h"
#include "chromeos/dbus/nfc_manager_client.h"
#include "chromeos/dbus/nfc_tag_client.h"
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
const char kTestDevicePath0[] = "/test/nfc/device0";
const char kTestDevicePath1[] = "/test/nfc/device1";
const char kTestTagPath0[] = "/test/nfc/tag0";
const char kTestTagPath1[] = "/test/nfc/tag1";

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

class MockNfcDeviceObserver : public NfcDeviceClient::Observer {
 public:
  MOCK_METHOD1(DeviceFound, void(const dbus::ObjectPath&));
  MOCK_METHOD1(DeviceLost, void(const dbus::ObjectPath&));
  MOCK_METHOD2(DevicePropertyChanged, void(const dbus::ObjectPath&,
                                           const std::string&));
};

class MockNfcTagObserver : public NfcTagClient::Observer {
 public:
  MOCK_METHOD1(TagFound, void(const dbus::ObjectPath&));
  MOCK_METHOD1(TagLost, void(const dbus::ObjectPath&));
  MOCK_METHOD2(TagPropertyChanged, void(const dbus::ObjectPath&,
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
    mock_device0_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(),
        kTestServiceName,
        dbus::ObjectPath(kTestDevicePath0));
    mock_device1_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(),
        kTestServiceName,
        dbus::ObjectPath(kTestDevicePath1));
    mock_tag0_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(),
        kTestServiceName,
        dbus::ObjectPath(kTestTagPath0));
    mock_tag1_proxy_ = new dbus::MockObjectProxy(
        mock_bus_.get(),
        kTestServiceName,
        dbus::ObjectPath(kTestTagPath1));

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
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(nfc_device::kNfcDeviceServiceName,
                               dbus::ObjectPath(kTestDevicePath0)))
        .WillRepeatedly(Return(mock_device0_proxy_.get()));
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(nfc_device::kNfcDeviceServiceName,
                               dbus::ObjectPath(kTestDevicePath1)))
        .WillRepeatedly(Return(mock_device1_proxy_.get()));
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(nfc_tag::kNfcTagServiceName,
                               dbus::ObjectPath(kTestTagPath0)))
        .WillRepeatedly(Return(mock_tag0_proxy_.get()));
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(nfc_tag::kNfcTagServiceName,
                               dbus::ObjectPath(kTestTagPath1)))
        .WillRepeatedly(Return(mock_tag1_proxy_.get()));

    // ShutdownAndBlock will be called in TearDown.
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create the clients.
    manager_client_.reset(
        NfcManagerClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION));
    adapter_client_.reset(
        NfcAdapterClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION,
                                 manager_client_.get()));
    device_client_.reset(
        NfcDeviceClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION,
                                adapter_client_.get()));
    tag_client_.reset(
        NfcTagClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION,
                             adapter_client_.get()));
    manager_client_->Init(mock_bus_.get());
    adapter_client_->Init(mock_bus_.get());
    device_client_->Init(mock_bus_.get());
    tag_client_->Init(mock_bus_.get());
    manager_client_->AddObserver(&mock_manager_observer_);
    adapter_client_->AddObserver(&mock_adapter_observer_);
    device_client_->AddObserver(&mock_device_observer_);
    tag_client_->AddObserver(&mock_tag_observer_);

    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    tag_client_->RemoveObserver(&mock_tag_observer_);
    device_client_->RemoveObserver(&mock_device_observer_);
    adapter_client_->RemoveObserver(&mock_adapter_observer_);
    manager_client_->RemoveObserver(&mock_manager_observer_);
    mock_bus_->ShutdownAndBlock();
  }

  void SimulateAdaptersChanged(
      const std::vector<dbus::ObjectPath>& adapter_paths) {
    NfcManagerClient::Properties* properties =
        manager_client_->GetProperties();
    ASSERT_TRUE(properties);
    EXPECT_CALL(mock_manager_observer_,
                ManagerPropertyChanged(nfc_manager::kAdaptersProperty));
    SendArrayPropertyChangedSignal(
        properties,
        nfc_manager::kNfcManagerInterface,
        nfc_manager::kAdaptersProperty,
        adapter_paths);
    Mock::VerifyAndClearExpectations(&mock_manager_observer_);
  }

  void SimulateTagsChanged(const std::vector<dbus::ObjectPath>& tag_paths,
                           const dbus::ObjectPath& adapter_path) {
    NfcAdapterClient::Properties* properties =
        adapter_client_->GetProperties(adapter_path);
    ASSERT_TRUE(properties);
    EXPECT_CALL(mock_adapter_observer_,
                AdapterPropertyChanged(adapter_path,
                                       nfc_adapter::kTagsProperty));
    SendArrayPropertyChangedSignal(
        properties,
        nfc_adapter::kNfcAdapterInterface,
        nfc_adapter::kTagsProperty,
        tag_paths);
    Mock::VerifyAndClearExpectations(&mock_adapter_observer_);
  }

  void SimulateDevicesChanged(const std::vector<dbus::ObjectPath>& device_paths,
                              const dbus::ObjectPath& adapter_path) {
    NfcAdapterClient::Properties* properties =
        adapter_client_->GetProperties(adapter_path);
    ASSERT_TRUE(properties);
    EXPECT_CALL(mock_adapter_observer_,
                AdapterPropertyChanged(adapter_path,
                                       nfc_adapter::kDevicesProperty));
    SendArrayPropertyChangedSignal(
        properties,
        nfc_adapter::kNfcAdapterInterface,
        nfc_adapter::kDevicesProperty,
        device_paths);
    Mock::VerifyAndClearExpectations(&mock_adapter_observer_);
  }

  void SendArrayPropertyChangedSignal(
      dbus::PropertySet* properties,
      const std::string& interface,
      const std::string& property_name,
      std::vector<dbus::ObjectPath> object_paths) {
    dbus::Signal signal(interface, nfc_common::kPropertyChangedSignal);
    dbus::MessageWriter writer(&signal);
    writer.AppendString(property_name);
    dbus::MessageWriter variant_writer(NULL);
    writer.OpenVariant("ao", &variant_writer);
    variant_writer.AppendArrayOfObjectPaths(object_paths);
    writer.CloseContainer(&variant_writer);
    properties->ChangedReceived(&signal);
  }

  MOCK_METHOD0(SuccessCallback, void(void));
  MOCK_METHOD2(ErrorCallback, void(const std::string& error_name,
                                   const std::string& error_message));

 protected:
  // The mock object proxies.
  scoped_refptr<dbus::MockObjectProxy> mock_manager_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_adapter0_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_adapter1_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_device0_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_device1_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_tag0_proxy_;
  scoped_refptr<dbus::MockObjectProxy> mock_tag1_proxy_;
  // The mock bus.
  scoped_refptr<dbus::MockBus> mock_bus_;
  // A message loop to emulate asynchronous behavior.
  base::MessageLoop message_loop_;
  // Response returned by mock methods.
  dbus::Response* response_;
  // The D-Bus client objects under test.
  scoped_ptr<NfcManagerClient> manager_client_;
  scoped_ptr<NfcAdapterClient> adapter_client_;
  scoped_ptr<NfcDeviceClient> device_client_;
  scoped_ptr<NfcTagClient> tag_client_;
  // Mock observers.
  MockNfcManagerObserver mock_manager_observer_;
  MockNfcAdapterObserver mock_adapter_observer_;
  MockNfcDeviceObserver mock_device_observer_;
  MockNfcTagObserver mock_tag_observer_;
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
  std::vector<dbus::ObjectPath> adapter_paths;
  adapter_paths.push_back(dbus::ObjectPath(kTestAdapterPath0));
  EXPECT_CALL(mock_adapter_observer_,
              AdapterAdded(dbus::ObjectPath(kTestAdapterPath0)));
  SimulateAdaptersChanged(adapter_paths);

  // Invoking methods should succeed on adapter 0 but fail on adapter 1.
  EXPECT_CALL(*mock_adapter0_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  adapter_client_->StartPollLoop(
      dbus::ObjectPath(kTestAdapterPath0),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcClientTest::SuccessCallback, base::Unretained(this)),
      base::Bind(&NfcClientTest::ErrorCallback, base::Unretained(this)));
  Mock::VerifyAndClearExpectations(&mock_adapter0_proxy_);
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _));
  EXPECT_CALL(*mock_adapter1_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);
  adapter_client_->StartPollLoop(
      dbus::ObjectPath(kTestAdapterPath1),
      nfc_adapter::kModeInitiator,
      base::Bind(&NfcClientTest::SuccessCallback, base::Unretained(this)),
      base::Bind(&NfcClientTest::ErrorCallback, base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_adapter1_proxy_);

  // Add adapter 1.
  adapter_paths.push_back(dbus::ObjectPath(kTestAdapterPath1));
  EXPECT_CALL(mock_adapter_observer_,
              AdapterAdded(dbus::ObjectPath(kTestAdapterPath1)));
  SimulateAdaptersChanged(adapter_paths);

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
  Mock::VerifyAndClearExpectations(&mock_adapter0_proxy_);
  Mock::VerifyAndClearExpectations(&mock_adapter1_proxy_);

  // Remove adapter 0.
  adapter_paths.erase(adapter_paths.begin());
  EXPECT_CALL(mock_adapter_observer_,
              AdapterRemoved(dbus::ObjectPath(kTestAdapterPath0)));
  SimulateAdaptersChanged(adapter_paths);

  // Invoking methods should succeed on adapter 1 but fail on adapter 0.
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _));
  EXPECT_CALL(*mock_adapter0_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);
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
  Mock::VerifyAndClearExpectations(&mock_adapter0_proxy_);
  Mock::VerifyAndClearExpectations(&mock_adapter1_proxy_);

  // Remove adapter 1.
  adapter_paths.clear();
  EXPECT_CALL(mock_adapter_observer_,
              AdapterRemoved(dbus::ObjectPath(kTestAdapterPath1)));
  SimulateAdaptersChanged(adapter_paths);

  // Invoking methods should fail on both adapters.
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _))
      .Times(2);
  EXPECT_CALL(*mock_adapter0_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);
  EXPECT_CALL(*mock_adapter1_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);;
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

// Tests that when tags are added and removed through an adapter, all
// observers are notified and the proxies are created and removed
// accordingly.
TEST_F(NfcClientTest, TagsAddedAndRemoved) {
  // Invoking methods on tags that haven't been added should fail.
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _));
  NfcTagClient::RecordAttributes write_data;
  write_data[nfc_record::kTypeProperty] = nfc_record::kTypeText;
  tag_client_->Write(dbus::ObjectPath(kTestTagPath0), write_data,
                     base::Bind(&NfcClientTest::SuccessCallback,
                                base::Unretained(this)),
                     base::Bind(&NfcClientTest::ErrorCallback,
                                base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);

  // Add adapter 0.
  std::vector<dbus::ObjectPath> adapter_paths;
  adapter_paths.push_back(dbus::ObjectPath(kTestAdapterPath0));
  EXPECT_CALL(mock_adapter_observer_,
              AdapterAdded(dbus::ObjectPath(kTestAdapterPath0)));
  SimulateAdaptersChanged(adapter_paths);

  // Add tag 0.
  std::vector<dbus::ObjectPath> tag_paths;
  tag_paths.push_back(dbus::ObjectPath(kTestTagPath0));
  EXPECT_CALL(mock_tag_observer_,
              TagFound(dbus::ObjectPath(kTestTagPath0)));
  SimulateTagsChanged(tag_paths, dbus::ObjectPath(kTestAdapterPath0));
  Mock::VerifyAndClearExpectations(&mock_tag_observer_);

  // Invoking methods should succeed on tag 0 but fail on tag 1.
  EXPECT_CALL(*mock_tag0_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  tag_client_->Write(dbus::ObjectPath(kTestTagPath0), write_data,
                     base::Bind(&NfcClientTest::SuccessCallback,
                                base::Unretained(this)),
                     base::Bind(&NfcClientTest::ErrorCallback,
                                base::Unretained(this)));
  Mock::VerifyAndClearExpectations(&mock_tag0_proxy_);
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _));
  EXPECT_CALL(*mock_tag1_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);
  tag_client_->Write(dbus::ObjectPath(kTestTagPath1), write_data,
                     base::Bind(&NfcClientTest::SuccessCallback,
                                base::Unretained(this)),
                     base::Bind(&NfcClientTest::ErrorCallback,
                                base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_tag1_proxy_);

  // Add tag 1.
  tag_paths.push_back(dbus::ObjectPath(kTestTagPath1));
  EXPECT_CALL(mock_tag_observer_,
              TagFound(dbus::ObjectPath(kTestTagPath1)));
  SimulateTagsChanged(tag_paths, dbus::ObjectPath(kTestAdapterPath0));
  Mock::VerifyAndClearExpectations(&mock_tag_observer_);

  // Invoking methods should succeed on both tags.
  EXPECT_CALL(*mock_tag0_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  EXPECT_CALL(*mock_tag1_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  tag_client_->Write(dbus::ObjectPath(kTestTagPath0), write_data,
                     base::Bind(&NfcClientTest::SuccessCallback,
                                base::Unretained(this)),
                     base::Bind(&NfcClientTest::ErrorCallback,
                                base::Unretained(this)));
  tag_client_->Write(dbus::ObjectPath(kTestTagPath1), write_data,
                     base::Bind(&NfcClientTest::SuccessCallback,
                                base::Unretained(this)),
                     base::Bind(&NfcClientTest::ErrorCallback,
                                base::Unretained(this)));
  Mock::VerifyAndClearExpectations(&mock_tag0_proxy_);
  Mock::VerifyAndClearExpectations(&mock_tag1_proxy_);

  // Remove tag 0.
  tag_paths.erase(tag_paths.begin());
  EXPECT_CALL(mock_tag_observer_,
              TagLost(dbus::ObjectPath(kTestTagPath0)));
  SimulateTagsChanged(tag_paths, dbus::ObjectPath(kTestAdapterPath0));
  Mock::VerifyAndClearExpectations(&mock_tag_observer_);

  // Invoking methods should succeed on tag 1 but fail on tag 0.
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _));
  EXPECT_CALL(*mock_tag0_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);
  tag_client_->Write(dbus::ObjectPath(kTestTagPath0), write_data,
                     base::Bind(&NfcClientTest::SuccessCallback,
                                base::Unretained(this)),
                     base::Bind(&NfcClientTest::ErrorCallback,
                                base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_tag0_proxy_);
  EXPECT_CALL(*mock_tag1_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  tag_client_->Write(dbus::ObjectPath(kTestTagPath1), write_data,
                     base::Bind(&NfcClientTest::SuccessCallback,
                                base::Unretained(this)),
                     base::Bind(&NfcClientTest::ErrorCallback,
                                base::Unretained(this)));
  Mock::VerifyAndClearExpectations(&mock_tag1_proxy_);

  // Remove tag 1.
  tag_paths.clear();
  EXPECT_CALL(mock_tag_observer_,
              TagLost(dbus::ObjectPath(kTestTagPath1)));
  SimulateTagsChanged(tag_paths, dbus::ObjectPath(kTestAdapterPath0));
  Mock::VerifyAndClearExpectations(&mock_tag_observer_);

  // Invoking methods should fail on both tags.
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _))
      .Times(2);
  EXPECT_CALL(*mock_tag0_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);
  EXPECT_CALL(*mock_tag1_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);
  tag_client_->Write(dbus::ObjectPath(kTestTagPath0), write_data,
                     base::Bind(&NfcClientTest::SuccessCallback,
                                base::Unretained(this)),
                     base::Bind(&NfcClientTest::ErrorCallback,
                                base::Unretained(this)));
  tag_client_->Write(dbus::ObjectPath(kTestTagPath1), write_data,
                     base::Bind(&NfcClientTest::SuccessCallback,
                                base::Unretained(this)),
                     base::Bind(&NfcClientTest::ErrorCallback,
                                base::Unretained(this)));
}

// Tests that when devices are added and removed through an adapter, all
// observers are notified and the proxies are created and removed
// accordingly.
TEST_F(NfcClientTest, DevicesAddedAndRemoved) {
  // Invoking methods on devices that haven't been added should fail.
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _));
  NfcDeviceClient::RecordAttributes write_data;
  write_data[nfc_record::kTypeProperty] = nfc_record::kTypeText;
  device_client_->Push(dbus::ObjectPath(kTestDevicePath0), write_data,
                       base::Bind(&NfcClientTest::SuccessCallback,
                                  base::Unretained(this)),
                       base::Bind(&NfcClientTest::ErrorCallback,
                                  base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);

  // Add adapter 0.
  std::vector<dbus::ObjectPath> adapter_paths;
  adapter_paths.push_back(dbus::ObjectPath(kTestAdapterPath0));
  EXPECT_CALL(mock_adapter_observer_,
              AdapterAdded(dbus::ObjectPath(kTestAdapterPath0)));
  SimulateAdaptersChanged(adapter_paths);

  // Add device 0.
  std::vector<dbus::ObjectPath> device_paths;
  device_paths.push_back(dbus::ObjectPath(kTestDevicePath0));
  EXPECT_CALL(mock_device_observer_,
              DeviceFound(dbus::ObjectPath(kTestDevicePath0)));
  SimulateDevicesChanged(device_paths, dbus::ObjectPath(kTestAdapterPath0));
  Mock::VerifyAndClearExpectations(&mock_device_observer_);

  // Invoking methods should succeed on device 0 but fail on device 1.
  EXPECT_CALL(*mock_device0_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  device_client_->Push(dbus::ObjectPath(kTestDevicePath0), write_data,
                       base::Bind(&NfcClientTest::SuccessCallback,
                                  base::Unretained(this)),
                       base::Bind(&NfcClientTest::ErrorCallback,
                                  base::Unretained(this)));
  Mock::VerifyAndClearExpectations(&mock_device0_proxy_);
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _));
  EXPECT_CALL(*mock_device1_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);
  device_client_->Push(dbus::ObjectPath(kTestDevicePath1), write_data,
                       base::Bind(&NfcClientTest::SuccessCallback,
                                  base::Unretained(this)),
                       base::Bind(&NfcClientTest::ErrorCallback,
                                  base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_device1_proxy_);

  // Add device 1.
  device_paths.push_back(dbus::ObjectPath(kTestDevicePath1));
  EXPECT_CALL(mock_device_observer_,
              DeviceFound(dbus::ObjectPath(kTestDevicePath1)));
  SimulateDevicesChanged(device_paths, dbus::ObjectPath(kTestAdapterPath0));
  Mock::VerifyAndClearExpectations(&mock_device_observer_);

  // Invoking methods should succeed on both devices.
  EXPECT_CALL(*mock_device0_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  EXPECT_CALL(*mock_device1_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  device_client_->Push(dbus::ObjectPath(kTestDevicePath0), write_data,
                       base::Bind(&NfcClientTest::SuccessCallback,
                                  base::Unretained(this)),
                       base::Bind(&NfcClientTest::ErrorCallback,
                                  base::Unretained(this)));
  device_client_->Push(dbus::ObjectPath(kTestDevicePath1), write_data,
                       base::Bind(&NfcClientTest::SuccessCallback,
                                  base::Unretained(this)),
                       base::Bind(&NfcClientTest::ErrorCallback,
                                  base::Unretained(this)));
  Mock::VerifyAndClearExpectations(&mock_device0_proxy_);
  Mock::VerifyAndClearExpectations(&mock_device1_proxy_);

  // Remove device 0.
  device_paths.erase(device_paths.begin());
  EXPECT_CALL(mock_device_observer_,
              DeviceLost(dbus::ObjectPath(kTestDevicePath0)));
  SimulateDevicesChanged(device_paths, dbus::ObjectPath(kTestAdapterPath0));
  Mock::VerifyAndClearExpectations(&mock_device_observer_);

  // Invoking methods should succeed on device 1 but fail on device 0.
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _));
  EXPECT_CALL(*mock_device0_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);
  device_client_->Push(dbus::ObjectPath(kTestDevicePath0), write_data,
                       base::Bind(&NfcClientTest::SuccessCallback,
                                  base::Unretained(this)),
                       base::Bind(&NfcClientTest::ErrorCallback,
                                  base::Unretained(this)));
  Mock::VerifyAndClearExpectations(this);
  Mock::VerifyAndClearExpectations(&mock_device0_proxy_);
  EXPECT_CALL(*mock_device1_proxy_, CallMethodWithErrorCallback(_, _, _, _));
  device_client_->Push(dbus::ObjectPath(kTestDevicePath1), write_data,
                       base::Bind(&NfcClientTest::SuccessCallback,
                                  base::Unretained(this)),
                       base::Bind(&NfcClientTest::ErrorCallback,
                                  base::Unretained(this)));
  Mock::VerifyAndClearExpectations(&mock_device1_proxy_);

  // Remove device 1.
  device_paths.clear();
  EXPECT_CALL(mock_device_observer_,
              DeviceLost(dbus::ObjectPath(kTestDevicePath1)));
  SimulateDevicesChanged(device_paths, dbus::ObjectPath(kTestAdapterPath0));
  Mock::VerifyAndClearExpectations(&mock_device_observer_);

  // Invoking methods should fail on both devices.
  EXPECT_CALL(*this,
              ErrorCallback(nfc_client_helpers::kUnknownObjectError, _))
      .Times(2);
  EXPECT_CALL(*mock_device0_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);
  EXPECT_CALL(*mock_device1_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .Times(0);
  device_client_->Push(dbus::ObjectPath(kTestDevicePath0), write_data,
                       base::Bind(&NfcClientTest::SuccessCallback,
                                  base::Unretained(this)),
                       base::Bind(&NfcClientTest::ErrorCallback,
                                  base::Unretained(this)));
  device_client_->Push(dbus::ObjectPath(kTestDevicePath1), write_data,
                       base::Bind(&NfcClientTest::SuccessCallback,
                                  base::Unretained(this)),
                       base::Bind(&NfcClientTest::ErrorCallback,
                                  base::Unretained(this)));
}

}  // namespace chromeos
