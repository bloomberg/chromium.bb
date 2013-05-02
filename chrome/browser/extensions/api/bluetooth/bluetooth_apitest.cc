// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "base/stringprintf.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_event_router.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_out_of_band_pairing_data.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_profile.h"
#include "device/bluetooth/test/mock_bluetooth_socket.h"
#include "testing/gmock/include/gmock/gmock.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothOutOfBandPairingData;
using device::BluetoothProfile;
using device::MockBluetoothAdapter;
using device::MockBluetoothDevice;
using device::MockBluetoothProfile;
using extensions::Extension;

namespace utils = extension_function_test_utils;
namespace api = extensions::api;

namespace {

static const char* kAdapterAddress = "A1:A2:A3:A4:A5:A6";
static const char* kName = "whatsinaname";

class BluetoothApiTest : public ExtensionApiTest {
 public:
  BluetoothApiTest() : empty_extension_(utils::CreateEmptyExtension()) {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    SetUpMockAdapter();
    profile1_.reset(new testing::NiceMock<MockBluetoothProfile>());
    profile2_.reset(new testing::NiceMock<MockBluetoothProfile>());
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_));
  }

  void SetUpMockAdapter() {
    // The browser will clean this up when it is torn down
    mock_adapter_ = new testing::StrictMock<MockBluetoothAdapter>();
    event_router()->SetAdapterForTest(mock_adapter_);

    device1_.reset(new testing::NiceMock<MockBluetoothDevice>(
        mock_adapter_, 0, "d1", "11:12:13:14:15:16",
        true /* paired */, true /* connected */));
    device2_.reset(new testing::NiceMock<MockBluetoothDevice>(
        mock_adapter_, 0, "d2", "21:22:23:24:25:26",
        false /* paired */, false /* connected */));
  }

  template <class T>
  T* setupFunction(T* function) {
    function->set_extension(empty_extension_.get());
    function->set_has_callback(true);
    return function;
  }

 protected:
  testing::StrictMock<MockBluetoothAdapter>* mock_adapter_;
  scoped_ptr<testing::NiceMock<MockBluetoothDevice> > device1_;
  scoped_ptr<testing::NiceMock<MockBluetoothDevice> > device2_;
  scoped_ptr<testing::NiceMock<MockBluetoothProfile> > profile1_;
  scoped_ptr<testing::NiceMock<MockBluetoothProfile> > profile2_;

  extensions::ExtensionBluetoothEventRouter* event_router() {
    return extensions::BluetoothAPI::Get(browser()->profile())
        ->bluetooth_event_router();
  }

 private:
  scoped_refptr<Extension> empty_extension_;
};

class TestBluetoothAddProfileFunction
    : public api::BluetoothAddProfileFunction {
 public:
  explicit TestBluetoothAddProfileFunction(BluetoothProfile* profile)
      : BluetoothAddProfileFunction(), profile_(profile) {
  }

 protected:
  virtual ~TestBluetoothAddProfileFunction() {
  }

  // BluetoothAddProfileFunction override.
  virtual void RegisterProfile(
      const device::BluetoothProfile::Options& options,
      const device::BluetoothProfile::ProfileCallback& callback) OVERRIDE {
    callback.Run(profile_);
  }

 private:
  // TestBluetoothAddProfileFunction does not own |profile_|.
  BluetoothProfile* profile_;
};

// This is the canonical UUID for the short UUID 0010.
static const char kOutOfBandPairingDataHash[] = "0123456789ABCDEh";
static const char kOutOfBandPairingDataRandomizer[] = "0123456789ABCDEr";

static BluetoothOutOfBandPairingData GetOutOfBandPairingData() {
  BluetoothOutOfBandPairingData data;
  memcpy(&(data.hash), kOutOfBandPairingDataHash,
      device::kBluetoothOutOfBandPairingDataSize);
  memcpy(&(data.randomizer), kOutOfBandPairingDataRandomizer,
      device::kBluetoothOutOfBandPairingDataSize);
  return data;
}

static bool CallClosure(const base::Closure& callback) {
  callback.Run();
  return true;
}

static void CallConnectToProfileCallback(
    BluetoothProfile* profile,
    const base::Closure& callback,
    const BluetoothDevice::ErrorCallback& error_callback) {
  callback.Run();
}

static void CallDiscoveryCallback(
    const base::Closure& callback,
    const BluetoothAdapter::ErrorCallback& error_callback) {
  callback.Run();
}

static void CallDiscoveryErrorCallback(
    const base::Closure& callback,
    const BluetoothAdapter::ErrorCallback& error_callback) {
  error_callback.Run();
}

static void CallOutOfBandPairingDataCallback(
    const BluetoothAdapter::BluetoothOutOfBandPairingDataCallback& callback,
    const BluetoothAdapter::ErrorCallback& error_callback) {
  callback.Run(GetOutOfBandPairingData());
}

}  // namespace

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, Profiles) {
  EXPECT_CALL(*profile1_, SetConnectionCallback(testing::_));
  scoped_refptr<TestBluetoothAddProfileFunction> add_profile_function;
  add_profile_function = setupFunction(
      new TestBluetoothAddProfileFunction(profile1_.get()));
  std::string error(
      utils::RunFunctionAndReturnError(
          add_profile_function,
          "[{\"uuid\": \"1234\"}]",
          browser()));
  ASSERT_TRUE(error.empty());

  // Registering the profile for the same uuid again will throw an error.
  add_profile_function = setupFunction(
      new TestBluetoothAddProfileFunction(profile2_.get()));
  error = utils::RunFunctionAndReturnError(
      add_profile_function,
      "[{\"uuid\": \"1234\"}]",
      browser());
  ASSERT_FALSE(error.empty());

  add_profile_function = setupFunction(
      new TestBluetoothAddProfileFunction(profile2_.get()));
  error = utils::RunFunctionAndReturnError(
      add_profile_function,
      "[{\"uuid\": \"5678\"}]",
      browser());
  ASSERT_TRUE(error.empty());

  scoped_refptr<api::BluetoothRemoveProfileFunction> remove_profile_function;
  remove_profile_function = setupFunction(
      new api::BluetoothRemoveProfileFunction());
  error = utils::RunFunctionAndReturnError(
      remove_profile_function,
      "[{\"uuid\": \"1234\"}]",
      browser());
  ASSERT_TRUE(error.empty());

  remove_profile_function = setupFunction(
      new api::BluetoothRemoveProfileFunction());
  error = utils::RunFunctionAndReturnError(
      remove_profile_function,
      "[{\"uuid\": \"5678\"}]",
      browser());
  ASSERT_TRUE(error.empty());

  // Removing the same profile again will throw an error.
  remove_profile_function = setupFunction(
      new api::BluetoothRemoveProfileFunction());
  error = utils::RunFunctionAndReturnError(
      remove_profile_function,
      "[{\"uuid\": \"5678\"}]",
      browser());
  ASSERT_FALSE(error.empty());
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, GetAdapterState) {
  EXPECT_CALL(*mock_adapter_, GetAddress())
      .WillOnce(testing::Return(kAdapterAddress));
  EXPECT_CALL(*mock_adapter_, GetName())
      .WillOnce(testing::Return(kName));
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsDiscovering())
      .WillOnce(testing::Return(false));

  scoped_refptr<api::BluetoothGetAdapterStateFunction> get_adapter_state;
  get_adapter_state = setupFunction(new api::BluetoothGetAdapterStateFunction);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        get_adapter_state, "[]", browser()));
  ASSERT_TRUE(result.get() != NULL);
  api::bluetooth::AdapterState state;
  ASSERT_TRUE(api::bluetooth::AdapterState::Populate(*result, &state));

  EXPECT_FALSE(state.available);
  EXPECT_TRUE(state.powered);
  EXPECT_FALSE(state.discovering);
  EXPECT_EQ(kName, state.name);
  EXPECT_EQ(kAdapterAddress, state.address);
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, GetLocalOutOfBandPairingData) {
  EXPECT_CALL(*mock_adapter_,
              ReadLocalOutOfBandPairingData(testing::_, testing::_))
      .WillOnce(testing::Invoke(CallOutOfBandPairingDataCallback));

  scoped_refptr<api::BluetoothGetLocalOutOfBandPairingDataFunction>
      get_oob_function(setupFunction(
            new api::BluetoothGetLocalOutOfBandPairingDataFunction));

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      get_oob_function, "[]", browser()));

  base::DictionaryValue* dict;
  EXPECT_TRUE(result->GetAsDictionary(&dict));

  base::BinaryValue* binary_value;
  EXPECT_TRUE(dict->GetBinary("hash", &binary_value));
  EXPECT_STREQ(kOutOfBandPairingDataHash,
      std::string(binary_value->GetBuffer(), binary_value->GetSize()).c_str());
  EXPECT_TRUE(dict->GetBinary("randomizer", &binary_value));
  EXPECT_STREQ(kOutOfBandPairingDataRandomizer,
      std::string(binary_value->GetBuffer(), binary_value->GetSize()).c_str());

  // Try again with an error
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  EXPECT_CALL(*mock_adapter_,
              ReadLocalOutOfBandPairingData(
                  testing::_,
                  testing::Truly(CallClosure)));

  get_oob_function =
      setupFunction(new api::BluetoothGetLocalOutOfBandPairingDataFunction);

  std::string error(
      utils::RunFunctionAndReturnError(get_oob_function, "[]", browser()));
  EXPECT_FALSE(error.empty());
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, SetOutOfBandPairingData) {
  EXPECT_CALL(*mock_adapter_, GetDevice(device1_->GetAddress()))
      .WillOnce(testing::Return(device1_.get()));
  EXPECT_CALL(*device1_,
              ClearOutOfBandPairingData(testing::Truly(CallClosure),
                                        testing::_));

  std::string params = base::StringPrintf(
      "[{\"deviceAddress\":\"%s\"}]", device1_->GetAddress().c_str());

  scoped_refptr<api::BluetoothSetOutOfBandPairingDataFunction> set_oob_function;
  set_oob_function = setupFunction(
      new api::BluetoothSetOutOfBandPairingDataFunction);
  // There isn't actually a result.
  (void)utils::RunFunctionAndReturnSingleResult(
      set_oob_function, params, browser());

  // Try again with an error
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  testing::Mock::VerifyAndClearExpectations(device1_.get());
  EXPECT_CALL(*mock_adapter_, GetDevice(device1_->GetAddress()))
      .WillOnce(testing::Return(device1_.get()));
  EXPECT_CALL(*device1_,
              ClearOutOfBandPairingData(testing::_,
                                        testing::Truly(CallClosure)));

  set_oob_function = setupFunction(
      new api::BluetoothSetOutOfBandPairingDataFunction);
  std::string error(
      utils::RunFunctionAndReturnError(set_oob_function, params, browser()));
  EXPECT_FALSE(error.empty());

  // TODO(bryeung): Also test setting the data when there is support for
  // ArrayBuffers in the arguments to the RunFunctionAnd* methods.
  // crbug.com/132796
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, Discovery) {
  // Try with a failure to start
  EXPECT_CALL(*mock_adapter_, StartDiscovering(testing::_, testing::_))
      .WillOnce(testing::Invoke(CallDiscoveryErrorCallback));
  // StartDiscovery failure will remove the adapter that is no longer used.
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_));
  scoped_refptr<api::BluetoothStartDiscoveryFunction> start_function;
  start_function = setupFunction(new api::BluetoothStartDiscoveryFunction);
  std::string error(
      utils::RunFunctionAndReturnError(start_function, "[]", browser()));
  ASSERT_FALSE(error.empty());

  // Reset for a successful start
  SetUpMockAdapter();
  EXPECT_CALL(*mock_adapter_, StartDiscovering(testing::_, testing::_))
      .WillOnce(testing::Invoke(CallDiscoveryCallback));

  start_function = setupFunction(new api::BluetoothStartDiscoveryFunction);
  (void)utils::RunFunctionAndReturnError(start_function, "[]", browser());

  // Reset to try stopping
  testing::Mock::VerifyAndClearExpectations(mock_adapter_);
  EXPECT_CALL(*mock_adapter_, StopDiscovering(testing::_, testing::_))
      .WillOnce(testing::Invoke(CallDiscoveryCallback));
  // StopDiscovery success will remove the adapter that is no longer used.
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_));
  scoped_refptr<api::BluetoothStopDiscoveryFunction> stop_function;
  stop_function = setupFunction(new api::BluetoothStopDiscoveryFunction);
  (void)utils::RunFunctionAndReturnSingleResult(stop_function, "[]", browser());

  // Reset to try stopping with an error
  SetUpMockAdapter();
  EXPECT_CALL(*mock_adapter_, StopDiscovering(testing::_, testing::_))
      .WillOnce(testing::Invoke(CallDiscoveryErrorCallback));
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_));
  stop_function = setupFunction(new api::BluetoothStopDiscoveryFunction);
  error = utils::RunFunctionAndReturnError(stop_function, "[]", browser());
  ASSERT_FALSE(error.empty());
  SetUpMockAdapter();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, DiscoveryCallback) {
  EXPECT_CALL(*mock_adapter_, StartDiscovering(testing::_, testing::_))
      .WillOnce(testing::Invoke(CallDiscoveryCallback));
  EXPECT_CALL(*mock_adapter_, StopDiscovering(testing::_, testing::_))
      .WillOnce(testing::Invoke(CallDiscoveryCallback));

  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ExtensionTestMessageListener discovery_started("ready", true);
  ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("bluetooth/discovery_callback")));
  EXPECT_TRUE(discovery_started.WaitUntilSatisfied());

  event_router()->DeviceAdded(mock_adapter_, device1_.get());

  discovery_started.Reply("go");
  ExtensionTestMessageListener discovery_stopped("ready", true);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_));
  EXPECT_TRUE(discovery_stopped.WaitUntilSatisfied());

  SetUpMockAdapter();
  event_router()->DeviceAdded(mock_adapter_, device2_.get());
  discovery_stopped.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, DiscoveryInProgress) {
  EXPECT_CALL(*mock_adapter_, GetAddress())
      .WillOnce(testing::Return(kAdapterAddress));
  EXPECT_CALL(*mock_adapter_, GetName())
      .WillOnce(testing::Return(kName));
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillOnce(testing::Return(true));

  // Fake that the adapter is discovering
  EXPECT_CALL(*mock_adapter_, IsDiscovering())
      .WillOnce(testing::Return(true));
  event_router()->AdapterDiscoveringChanged(mock_adapter_, true);

  // Cache a device before the extension starts discovering
  event_router()->DeviceAdded(mock_adapter_, device1_.get());

  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  EXPECT_CALL(*mock_adapter_, StartDiscovering(testing::_, testing::_))
      .WillOnce(testing::Invoke(CallDiscoveryCallback));
  EXPECT_CALL(*mock_adapter_, StopDiscovering(testing::_, testing::_))
      .WillOnce(testing::Invoke(CallDiscoveryCallback));

  ExtensionTestMessageListener discovery_started("ready", true);
  ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("bluetooth/discovery_in_progress")));
  EXPECT_TRUE(discovery_started.WaitUntilSatisfied());

  // This should be received in addition to the cached device above.
  event_router()->DeviceAdded(mock_adapter_, device2_.get());

  discovery_started.Reply("go");
  ExtensionTestMessageListener discovery_stopped("ready", true);
  EXPECT_CALL(*mock_adapter_, RemoveObserver(testing::_));
  EXPECT_TRUE(discovery_stopped.WaitUntilSatisfied());

  SetUpMockAdapter();
  // This should never be received.
  event_router()->DeviceAdded(mock_adapter_, device2_.get());
  discovery_stopped.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, OnAdapterStateChanged) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  // Load and wait for setup
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(
      LoadExtension(
          test_data_dir_.AppendASCII("bluetooth/on_adapter_state_changed")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  EXPECT_CALL(*mock_adapter_, GetAddress())
      .WillOnce(testing::Return(kAdapterAddress));
  EXPECT_CALL(*mock_adapter_, GetName())
      .WillOnce(testing::Return(kName));
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(*mock_adapter_, IsDiscovering())
      .WillOnce(testing::Return(false));
  event_router()->AdapterPoweredChanged(mock_adapter_, false);

  EXPECT_CALL(*mock_adapter_, GetAddress())
      .WillOnce(testing::Return(kAdapterAddress));
  EXPECT_CALL(*mock_adapter_, GetName())
      .WillOnce(testing::Return(kName));
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsDiscovering())
      .WillOnce(testing::Return(true));
  event_router()->AdapterPresentChanged(mock_adapter_, true);

  EXPECT_CALL(*mock_adapter_, GetAddress())
      .WillOnce(testing::Return(kAdapterAddress));
  EXPECT_CALL(*mock_adapter_, GetName())
      .WillOnce(testing::Return(kName));
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsPowered())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*mock_adapter_, IsDiscovering())
      .WillOnce(testing::Return(true));
  event_router()->AdapterDiscoveringChanged(mock_adapter_, true);

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, OnConnection) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  // Load and wait for setup
  ExtensionTestMessageListener listener("ready", true);
  scoped_refptr<const Extension> extension(
      LoadExtension(test_data_dir_.AppendASCII("bluetooth/on_connection")));
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  scoped_refptr<device::MockBluetoothSocket> socket =
      new device::MockBluetoothSocket();

  event_router()->AddProfile("1234", profile1_.get());
  event_router()->DispatchConnectionEvent(
      extension->id(), "1234", device1_.get(), socket);

  listener.Reply("go");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  event_router()->RemoveProfile("1234");
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, GetProfiles) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  BluetoothDevice::ServiceList service_list;
  service_list.push_back("1234");
  service_list.push_back("5678");

  EXPECT_CALL(*device1_, GetServices())
      .WillOnce(testing::Return(service_list));

  EXPECT_CALL(*mock_adapter_, GetDevice(device1_->GetAddress()))
      .WillOnce(testing::Return(device1_.get()));

  // Load and wait for setup
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bluetooth/get_profiles")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, GetDevices) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  BluetoothAdapter::ConstDeviceList devices;
  devices.push_back(device1_.get());
  devices.push_back(device2_.get());

  EXPECT_CALL(*device1_, ProvidesServiceWithUUID(testing::_))
      .WillOnce(testing::Return(false));

  EXPECT_CALL(*device2_, ProvidesServiceWithUUID(testing::_))
      .WillOnce(testing::Return(true));

  EXPECT_CALL(*mock_adapter_, GetDevices())
      .Times(2)
      .WillRepeatedly(testing::Return(devices));

  // Load and wait for setup
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bluetooth/get_devices")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, GetDevicesError) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  // Load and wait for setup
  ExtensionTestMessageListener listener("ready", true);
  ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("bluetooth/get_devices_error")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  listener.Reply("go");

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(BluetoothApiTest, Permissions) {
  PermissionsRequestFunction::SetAutoConfirmForTests(true);
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);

  event_router()->AddProfile(
      "00001101-0000-1000-8000-00805f9b34fb", profile1_.get());

  EXPECT_CALL(*mock_adapter_, GetDevice(device1_->GetAddress()))
      .WillOnce(testing::Return(device1_.get()));
  EXPECT_CALL(*device1_,
              ConnectToProfile(testing::_, testing::_, testing::_))
      .WillOnce(testing::Invoke(CallConnectToProfileCallback));

  EXPECT_TRUE(RunExtensionTest("bluetooth/permissions")) << message_;
}
