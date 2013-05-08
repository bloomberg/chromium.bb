// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_device_client.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_service_provider.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Default interval between simulated events.
const int kSimulationIntervalMs = 750;

}

namespace chromeos {

const char FakeBluetoothDeviceClient::kPairedDevicePath[] =
    "/fake/hci0/dev0";
const char FakeBluetoothDeviceClient::kPairedDeviceAddress[] =
    "00:11:22:33:44:55";
const char FakeBluetoothDeviceClient::kPairedDeviceName[] =
    "Fake Device";
const uint32 FakeBluetoothDeviceClient::kPairedDeviceClass = 0x000104;

const char FakeBluetoothDeviceClient::kAppleMousePath[] =
    "/fake/hci0/dev1";
const char FakeBluetoothDeviceClient::kAppleMouseAddress[] =
    "28:CF:DA:00:00:00";
const char FakeBluetoothDeviceClient::kAppleMouseName[] =
    "Apple Magic Mouse";
const uint32 FakeBluetoothDeviceClient::kAppleMouseClass = 0x002580;

const char FakeBluetoothDeviceClient::kAppleKeyboardPath[] =
    "/fake/hci0/dev2";
const char FakeBluetoothDeviceClient::kAppleKeyboardAddress[] =
    "28:37:37:00:00:00";
const char FakeBluetoothDeviceClient::kAppleKeyboardName[] =
    "Apple Wireless Keyboard";
const uint32 FakeBluetoothDeviceClient::kAppleKeyboardClass = 0x002540;

const char FakeBluetoothDeviceClient::kVanishingDevicePath[] =
    "/fake/hci0/dev3";
const char FakeBluetoothDeviceClient::kVanishingDeviceAddress[] =
    "01:02:03:04:05:06";
const char FakeBluetoothDeviceClient::kVanishingDeviceName[] =
    "Vanishing Device";
const uint32 FakeBluetoothDeviceClient::kVanishingDeviceClass = 0x000104;

const char FakeBluetoothDeviceClient::kMicrosoftMousePath[] =
    "/fake/hci0/dev4";
const char FakeBluetoothDeviceClient::kMicrosoftMouseAddress[] =
    "7C:ED:8D:00:00:00";
const char FakeBluetoothDeviceClient::kMicrosoftMouseName[] =
    "Microsoft Mouse";
const uint32 FakeBluetoothDeviceClient::kMicrosoftMouseClass = 0x002580;

const char FakeBluetoothDeviceClient::kMotorolaKeyboardPath[] =
    "/fake/hci0/dev5";
const char FakeBluetoothDeviceClient::kMotorolaKeyboardAddress[] =
    "00:0F:F6:00:00:00";
const char FakeBluetoothDeviceClient::kMotorolaKeyboardName[] =
    "Motorola Keyboard";
const uint32 FakeBluetoothDeviceClient::kMotorolaKeyboardClass = 0x002540;

const char FakeBluetoothDeviceClient::kSonyHeadphonesPath[] =
    "/fake/hci0/dev6";
const char FakeBluetoothDeviceClient::kSonyHeadphonesAddress[] =
    "00:24:BE:00:00:00";
const char FakeBluetoothDeviceClient::kSonyHeadphonesName[] =
    "Sony BT-00";
const uint32 FakeBluetoothDeviceClient::kSonyHeadphonesClass = 0x240408;

const char FakeBluetoothDeviceClient::kPhonePath[] =
    "/fake/hci0/dev7";
const char FakeBluetoothDeviceClient::kPhoneAddress[] =
    "20:7D:74:00:00:00";
const char FakeBluetoothDeviceClient::kPhoneName[] =
    "Phone";
const uint32 FakeBluetoothDeviceClient::kPhoneClass = 0x7a020c;

const char FakeBluetoothDeviceClient::kWeirdDevicePath[] =
    "/fake/hci0/dev8";
const char FakeBluetoothDeviceClient::kWeirdDeviceAddress[] =
    "20:7D:74:00:00:01";
const char FakeBluetoothDeviceClient::kWeirdDeviceName[] =
    "Weird Device";
const uint32 FakeBluetoothDeviceClient::kWeirdDeviceClass = 0x7a020c;

const char FakeBluetoothDeviceClient::kUnconnectableDevicePath[] =
    "/fake/hci0/dev9";
const char FakeBluetoothDeviceClient::kUnconnectableDeviceAddress[] =
    "20:7D:74:00:00:02";
const char FakeBluetoothDeviceClient::kUnconnectableDeviceName[] =
    "Unconnectable Device";
const uint32 FakeBluetoothDeviceClient::kUnconnectableDeviceClass = 0x7a020c;

const char FakeBluetoothDeviceClient::kUnpairableDevicePath[] =
    "/fake/hci0/devA";
const char FakeBluetoothDeviceClient::kUnpairableDeviceAddress[] =
    "20:7D:74:00:00:03";
const char FakeBluetoothDeviceClient::kUnpairableDeviceName[] =
    "Unpairable Device";
const uint32 FakeBluetoothDeviceClient::kUnpairableDeviceClass = 0x002540;

FakeBluetoothDeviceClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : ExperimentalBluetoothDeviceClient::Properties(
          NULL,
          bluetooth_device::kExperimentalBluetoothDeviceInterface,
          callback) {
}

FakeBluetoothDeviceClient::Properties::~Properties() {
}

void FakeBluetoothDeviceClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeBluetoothDeviceClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeBluetoothDeviceClient::Properties::Set(
    dbus::PropertyBase *property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  if (property->name() == trusted.name()) {
    callback.Run(true);
    property->ReplaceValueWithSetValue();
  } else {
    callback.Run(false);
  }
}


FakeBluetoothDeviceClient::FakeBluetoothDeviceClient()
    : simulation_interval_ms_(kSimulationIntervalMs),
      discovery_simulation_step_(0),
      pairing_cancelled_(false) {
  Properties* properties = new Properties(base::Bind(
      &FakeBluetoothDeviceClient::OnPropertyChanged,
      base::Unretained(this),
      dbus::ObjectPath(kPairedDevicePath)));
  properties->address.ReplaceValue(kPairedDeviceAddress);
  properties->bluetooth_class.ReplaceValue(kPairedDeviceClass);
  properties->name.ReplaceValue("Fake Device (Name)");
  properties->alias.ReplaceValue(kPairedDeviceName);
  properties->paired.ReplaceValue(true);
  properties->trusted.ReplaceValue(true);
  properties->adapter.ReplaceValue(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  std::vector<std::string> uuids;
  uuids.push_back("00001800-0000-1000-8000-00805f9b34fb");
  uuids.push_back("00001801-0000-1000-8000-00805f9b34fb");
  properties->uuids.ReplaceValue(uuids);

  properties->modalias.ReplaceValue("usb:v05ACp030Dd0306");

  properties_map_[dbus::ObjectPath(kPairedDevicePath)] = properties;
  device_list_.push_back(dbus::ObjectPath(kPairedDevicePath));
}

FakeBluetoothDeviceClient::~FakeBluetoothDeviceClient() {
  // Clean up Properties structures
  STLDeleteValues(&properties_map_);
}

void FakeBluetoothDeviceClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothDeviceClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath> FakeBluetoothDeviceClient::GetDevicesForAdapter(
    const dbus::ObjectPath& adapter_path) {
  if (adapter_path ==
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath))
    return device_list_;
  else
    return std::vector<dbus::ObjectPath>();
}

FakeBluetoothDeviceClient::Properties*
FakeBluetoothDeviceClient::GetProperties(const dbus::ObjectPath& object_path) {
  PropertiesMap::iterator iter = properties_map_.find(object_path);
  if (iter != properties_map_.end())
    return iter->second;
  return NULL;
}

void FakeBluetoothDeviceClient::Connect(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Connect: " << object_path.value();
  Properties* properties = GetProperties(object_path);

  if (properties->connected.value() == true) {
    // Already connected.
    callback.Run();
    return;
  }

  if (properties->paired.value() != true &&
      object_path != dbus::ObjectPath(kMicrosoftMousePath)) {
    // Must be paired.
    error_callback.Run(bluetooth_adapter::kErrorFailed, "Not paired");
    return;
  } else if (properties->paired.value() == true &&
             object_path == dbus::ObjectPath(kUnconnectableDevicePath)) {
    // Must not be paired
    error_callback.Run(bluetooth_adapter::kErrorFailed,
                       "Connection fails while paired");
    return;
  }

  // The device can be connected.
  properties->connected.ReplaceValue(true);
  callback.Run();

  AddInputDeviceIfNeeded(object_path, properties);
}

void FakeBluetoothDeviceClient::Disconnect(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Disconnect: " << object_path.value();
  Properties* properties = GetProperties(object_path);

  if (properties->connected.value() == true) {
    callback.Run();
    properties->connected.ReplaceValue(false);
  } else {
    error_callback.Run("org.bluez.Error.NotConnected", "Not Connected");
  }
}

void FakeBluetoothDeviceClient::ConnectProfile(
    const dbus::ObjectPath& object_path,
    const std::string& uuid,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "ConnectProfile: " << object_path.value() << " " << uuid;
  error_callback.Run(kNoResponseError, "");
}

void FakeBluetoothDeviceClient::DisconnectProfile(
    const dbus::ObjectPath& object_path,
    const std::string& uuid,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "DisconnectProfile: " << object_path.value() << " " << uuid;
  error_callback.Run(kNoResponseError, "");
}

void FakeBluetoothDeviceClient::Pair(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Pair: " << object_path.value();
  Properties* properties = GetProperties(object_path);

  if (properties->paired.value() == true) {
    // Already paired.
    callback.Run();
    return;
  }

  pairing_cancelled_ = false;

  FakeBluetoothAgentManagerClient* fake_bluetooth_agent_manager_client =
      static_cast<FakeBluetoothAgentManagerClient*>(
          DBusThreadManager::Get()->
              GetExperimentalBluetoothAgentManagerClient());
  FakeBluetoothAgentServiceProvider* agent_service_provider =
      fake_bluetooth_agent_manager_client->GetAgentServiceProvider();
  if (agent_service_provider == NULL) {
    error_callback.Run(kNoResponseError, "Missing agent");
    return;
  }

  if (object_path == dbus::ObjectPath(kAppleMousePath) ||
      object_path == dbus::ObjectPath(kMicrosoftMousePath) ||
      object_path == dbus::ObjectPath(kUnconnectableDevicePath)) {
    // No need to call anything on the pairing delegate, just wait 3 times
    // the interval before acting as if the other end accepted it.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(3 * simulation_interval_ms_));

  } else if (object_path == dbus::ObjectPath(kAppleKeyboardPath)) {
    // Display a Pincode, and wait 7 times the interval before acting as
    // if the other end accepted it.
    agent_service_provider->DisplayPinCode(object_path, "123456");

    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(7 * simulation_interval_ms_));

  } else if (object_path == dbus::ObjectPath(kVanishingDevicePath)) {
    // The vanishing device simulates being too far away, and thus times out.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::TimeoutSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(4 * simulation_interval_ms_));

  } else if (object_path == dbus::ObjectPath(kMotorolaKeyboardPath)) {
    // Display a passkey, and each interval act as if another key was entered
    // for it.
    agent_service_provider->DisplayPasskey(object_path, 123456, 0);

    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::SimulateKeypress,
                   base::Unretained(this),
                   1, object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (object_path == dbus::ObjectPath(kSonyHeadphonesPath)) {
    // Request a Pincode.
    agent_service_provider->RequestPinCode(
        object_path,
        base::Bind(&FakeBluetoothDeviceClient::PinCodeCallback,
                   base::Unretained(this),
                   object_path,
                   callback,
                   error_callback));

  } else if (object_path == dbus::ObjectPath(kPhonePath)) {
    // Request confirmation of a Passkey.
    agent_service_provider->RequestConfirmation(
        object_path, 123456,
        base::Bind(&FakeBluetoothDeviceClient::ConfirmationCallback,
                   base::Unretained(this),
                   object_path,
                   callback,
                   error_callback));

  } else if (object_path == dbus::ObjectPath(kWeirdDevicePath)) {
    // Request a Passkey from the user.
    agent_service_provider->RequestPasskey(
        object_path,
        base::Bind(&FakeBluetoothDeviceClient::PasskeyCallback,
                   base::Unretained(this),
                   object_path,
                   callback,
                   error_callback));

  } else if (object_path == dbus::ObjectPath(kUnpairableDevicePath)) {
    // Fails the pairing with an org.bluez.Error.Failed error.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::FailSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else {
    error_callback.Run(kNoResponseError, "No pairing fake");
  }
}

void FakeBluetoothDeviceClient::CancelPairing(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "CancelPairing: " << object_path.value();
  pairing_cancelled_ = true;
  callback.Run();
}


void FakeBluetoothDeviceClient::BeginDiscoverySimulation(
    const dbus::ObjectPath& adapter_path) {
  VLOG(1) << "starting discovery simulation";

  discovery_simulation_step_ = 1;

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeBluetoothDeviceClient::DiscoverySimulationTimer,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
}

void FakeBluetoothDeviceClient::EndDiscoverySimulation(
    const dbus::ObjectPath& adapter_path) {
  VLOG(1) << "stopping discovery simulation";
  discovery_simulation_step_ = 0;
}

void FakeBluetoothDeviceClient::SetSimulationIntervalMs(int interval_ms) {
  simulation_interval_ms_ = interval_ms;
}

void FakeBluetoothDeviceClient::RemoveDevice(
    const dbus::ObjectPath& adapter_path,
    const dbus::ObjectPath& device_path) {
  std::vector<dbus::ObjectPath>::iterator listiter =
      std::find(device_list_.begin(), device_list_.end(), device_path);
  if (listiter == device_list_.end())
    return;

  PropertiesMap::iterator iter = properties_map_.find(device_path);
  Properties* properties = iter->second;

  VLOG(1) << "removing device: " << properties->alias.value();
  device_list_.erase(listiter);

  // Remove the Input interface if it exists. This should be called before the
  // ExperimentalBluetoothDeviceClient::Observer::DeviceRemoved because it
  // deletes the BluetoothDeviceExperimentalChromeOS object, including the
  // device_path referenced here.
  FakeBluetoothInputClient* fake_bluetooth_input_client =
      static_cast<FakeBluetoothInputClient*>(
          DBusThreadManager::Get()->GetExperimentalBluetoothInputClient());
  fake_bluetooth_input_client->RemoveInputDevice(device_path);

  FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer, observers_,
                    DeviceRemoved(device_path));

  delete properties;
  properties_map_.erase(iter);
}

void FakeBluetoothDeviceClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer, observers_,
                    DevicePropertyChanged(object_path, property_name));
}

void FakeBluetoothDeviceClient::DiscoverySimulationTimer() {
  if (!discovery_simulation_step_)
    return;

  // Timer fires every .75s, the numbers below are arbitrary to give a feel
  // for a discovery process.
  VLOG(1) << "discovery simulation, step " << discovery_simulation_step_;
  if (discovery_simulation_step_ == 2) {
    if (std::find(device_list_.begin(), device_list_.end(),
                  dbus::ObjectPath(kAppleMousePath)) == device_list_.end()) {
      Properties* properties = new Properties(base::Bind(
          &FakeBluetoothDeviceClient::OnPropertyChanged,
          base::Unretained(this),
          dbus::ObjectPath(kAppleMousePath)));
      properties->address.ReplaceValue(kAppleMouseAddress);
      properties->bluetooth_class.ReplaceValue(kAppleMouseClass);
      properties->name.ReplaceValue("Fake Apple Magic Mouse");
      properties->alias.ReplaceValue(kAppleMouseName);
      properties->adapter.ReplaceValue(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

      std::vector<std::string> uuids;
      uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
      properties->uuids.ReplaceValue(uuids);

      properties_map_[dbus::ObjectPath(kAppleMousePath)] = properties;
      device_list_.push_back(dbus::ObjectPath(kAppleMousePath));
      FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer, observers_,
                        DeviceAdded(dbus::ObjectPath(kAppleMousePath)));
    }

  } else if (discovery_simulation_step_ == 4) {
    if (std::find(device_list_.begin(), device_list_.end(),
                  dbus::ObjectPath(kAppleKeyboardPath)) == device_list_.end()) {
      Properties *properties = new Properties(base::Bind(
          &FakeBluetoothDeviceClient::OnPropertyChanged,
          base::Unretained(this),
          dbus::ObjectPath(kAppleKeyboardPath)));
      properties->address.ReplaceValue(kAppleKeyboardAddress);
      properties->bluetooth_class.ReplaceValue(kAppleKeyboardClass);
      properties->name.ReplaceValue("Fake Apple Wireless Keyboard");
      properties->alias.ReplaceValue(kAppleKeyboardName);
      properties->adapter.ReplaceValue(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

      std::vector<std::string> uuids;
      uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
      properties->uuids.ReplaceValue(uuids);

      properties_map_[dbus::ObjectPath(kAppleKeyboardPath)] = properties;
      device_list_.push_back(dbus::ObjectPath(kAppleKeyboardPath));
      FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer, observers_,
                        DeviceAdded(dbus::ObjectPath(kAppleKeyboardPath)));
    }

    if (std::find(device_list_.begin(), device_list_.end(),
                  dbus::ObjectPath(kVanishingDevicePath)) ==
        device_list_.end()) {
      Properties* properties = new Properties(base::Bind(
          &FakeBluetoothDeviceClient::OnPropertyChanged,
          base::Unretained(this),
          dbus::ObjectPath(kVanishingDevicePath)));
      properties->address.ReplaceValue(kVanishingDeviceAddress);
      properties->bluetooth_class.ReplaceValue(kVanishingDeviceClass);
      properties->name.ReplaceValue("Fake Vanishing Device");
      properties->alias.ReplaceValue(kVanishingDeviceName);
      properties->adapter.ReplaceValue(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

      properties_map_[dbus::ObjectPath(kVanishingDevicePath)] = properties;
      device_list_.push_back(dbus::ObjectPath(kVanishingDevicePath));
      FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer, observers_,
                        DeviceAdded(dbus::ObjectPath(kVanishingDevicePath)));
    }

  } else if (discovery_simulation_step_ == 7) {
    if (std::find(device_list_.begin(), device_list_.end(),
                  dbus::ObjectPath(kMicrosoftMousePath)) ==
        device_list_.end()) {
      Properties* properties = new Properties(base::Bind(
          &FakeBluetoothDeviceClient::OnPropertyChanged,
          base::Unretained(this),
          dbus::ObjectPath(kMicrosoftMousePath)));
      properties->address.ReplaceValue(kMicrosoftMouseAddress);
      properties->bluetooth_class.ReplaceValue(kMicrosoftMouseClass);
      properties->name.ReplaceValue("Fake Microsoft Mouse");
      properties->alias.ReplaceValue(kMicrosoftMouseName);
      properties->adapter.ReplaceValue(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

      std::vector<std::string> uuids;
      uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
      properties->uuids.ReplaceValue(uuids);

      properties_map_[dbus::ObjectPath(kMicrosoftMousePath)] = properties;
      device_list_.push_back(dbus::ObjectPath(kMicrosoftMousePath));
      FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer, observers_,
                        DeviceAdded(dbus::ObjectPath(kMicrosoftMousePath)));
    }

  } else if (discovery_simulation_step_ == 8) {
    if (std::find(device_list_.begin(), device_list_.end(),
                  dbus::ObjectPath(kMotorolaKeyboardPath)) ==
        device_list_.end()) {
      Properties* properties = new Properties(base::Bind(
          &FakeBluetoothDeviceClient::OnPropertyChanged,
          base::Unretained(this),
          dbus::ObjectPath(kMotorolaKeyboardPath)));
      properties->address.ReplaceValue(kMotorolaKeyboardAddress);
      properties->bluetooth_class.ReplaceValue(kMotorolaKeyboardClass);
      properties->name.ReplaceValue("Fake Motorola Keyboard");
      properties->alias.ReplaceValue(kMotorolaKeyboardName);
      properties->adapter.ReplaceValue(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

      std::vector<std::string> uuids;
      uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
      properties->uuids.ReplaceValue(uuids);

      properties_map_[dbus::ObjectPath(kMotorolaKeyboardPath)] = properties;
      device_list_.push_back(dbus::ObjectPath(kMotorolaKeyboardPath));
      FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer, observers_,
                        DeviceAdded(dbus::ObjectPath(kMotorolaKeyboardPath)));
    }

    if (std::find(device_list_.begin(), device_list_.end(),
                  dbus::ObjectPath(kSonyHeadphonesPath)) ==
        device_list_.end()) {
      Properties* properties = new Properties(base::Bind(
          &FakeBluetoothDeviceClient::OnPropertyChanged,
          base::Unretained(this),
          dbus::ObjectPath(kSonyHeadphonesPath)));
      properties->address.ReplaceValue(kSonyHeadphonesAddress);
      properties->bluetooth_class.ReplaceValue(kSonyHeadphonesClass);
      properties->name.ReplaceValue("Fake Sony Headphones");
      properties->alias.ReplaceValue(kSonyHeadphonesName);
      properties->adapter.ReplaceValue(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

      properties_map_[dbus::ObjectPath(kSonyHeadphonesPath)] = properties;
      device_list_.push_back(dbus::ObjectPath(kSonyHeadphonesPath));
      FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer, observers_,
                        DeviceAdded(dbus::ObjectPath(kSonyHeadphonesPath)));
    }

  } else if (discovery_simulation_step_ == 10) {
    if (std::find(device_list_.begin(), device_list_.end(),
                  dbus::ObjectPath(kPhonePath)) == device_list_.end()) {
      Properties* properties = new Properties(base::Bind(
          &FakeBluetoothDeviceClient::OnPropertyChanged,
          base::Unretained(this),
          dbus::ObjectPath(kPhonePath)));
      properties->address.ReplaceValue(kPhoneAddress);
      properties->bluetooth_class.ReplaceValue(kPhoneClass);
      properties->name.ReplaceValue("Fake Phone");
      properties->alias.ReplaceValue(kPhoneName);
      properties->adapter.ReplaceValue(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

      properties_map_[dbus::ObjectPath(kPhonePath)] = properties;
      device_list_.push_back(dbus::ObjectPath(kPhonePath));
      FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer, observers_,
                        DeviceAdded(dbus::ObjectPath(kPhonePath)));
    }

    if (std::find(device_list_.begin(), device_list_.end(),
                  dbus::ObjectPath(kWeirdDevicePath)) == device_list_.end()) {
      Properties* properties = new Properties(base::Bind(
          &FakeBluetoothDeviceClient::OnPropertyChanged,
          base::Unretained(this),
          dbus::ObjectPath(kWeirdDevicePath)));
      properties->address.ReplaceValue(kWeirdDeviceAddress);
      properties->bluetooth_class.ReplaceValue(kWeirdDeviceClass);
      properties->name.ReplaceValue("Fake Weird Device");
      properties->alias.ReplaceValue(kWeirdDeviceName);
      properties->adapter.ReplaceValue(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

      properties_map_[dbus::ObjectPath(kWeirdDevicePath)] = properties;
      device_list_.push_back(dbus::ObjectPath(kWeirdDevicePath));
      FOR_EACH_OBSERVER(ExperimentalBluetoothDeviceClient::Observer, observers_,
                        DeviceAdded(dbus::ObjectPath(kWeirdDevicePath)));
    }

    if (std::find(device_list_.begin(), device_list_.end(),
                  dbus::ObjectPath(kUnconnectableDevicePath)) ==
        device_list_.end()) {
      Properties* properties = new Properties(base::Bind(
          &FakeBluetoothDeviceClient::OnPropertyChanged,
          base::Unretained(this),
          dbus::ObjectPath(kUnconnectableDevicePath)));
      properties->address.ReplaceValue(kUnconnectableDeviceAddress);
      properties->bluetooth_class.ReplaceValue(kUnconnectableDeviceClass);
      properties->name.ReplaceValue("Fake Unconnectable Device");
      properties->alias.ReplaceValue(kUnconnectableDeviceName);
      properties->adapter.ReplaceValue(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

      properties_map_[dbus::ObjectPath(kUnconnectableDevicePath)] = properties;
      device_list_.push_back(dbus::ObjectPath(kUnconnectableDevicePath));
      FOR_EACH_OBSERVER(
          ExperimentalBluetoothDeviceClient::Observer, observers_,
          DeviceAdded(dbus::ObjectPath(kUnconnectableDevicePath)));
    }

    if (std::find(device_list_.begin(), device_list_.end(),
                  dbus::ObjectPath(kUnpairableDevicePath)) ==
        device_list_.end()) {
      Properties* properties = new Properties(base::Bind(
          &FakeBluetoothDeviceClient::OnPropertyChanged,
          base::Unretained(this),
          dbus::ObjectPath(kUnpairableDevicePath)));
      properties->address.ReplaceValue(kUnpairableDeviceAddress);
      properties->bluetooth_class.ReplaceValue(kUnpairableDeviceClass);
      properties->name.ReplaceValue("Fake Unpairable Device");
      properties->alias.ReplaceValue(kUnpairableDeviceName);
      properties->adapter.ReplaceValue(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

      properties_map_[dbus::ObjectPath(kUnpairableDevicePath)] = properties;
      device_list_.push_back(dbus::ObjectPath(kUnpairableDevicePath));
      FOR_EACH_OBSERVER(
          ExperimentalBluetoothDeviceClient::Observer, observers_,
          DeviceAdded(dbus::ObjectPath(kUnpairableDevicePath)));
    }

  } else if (discovery_simulation_step_ == 13) {
    RemoveDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kVanishingDevicePath));

  } else if (discovery_simulation_step_ == 14) {
    return;

  }

  ++discovery_simulation_step_;
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeBluetoothDeviceClient::DiscoverySimulationTimer,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
}


void FakeBluetoothDeviceClient::CompleteSimulatedPairing(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "CompleteSimulatedPairing: " << object_path.value();
  if (pairing_cancelled_) {
    pairing_cancelled_ = false;

    error_callback.Run(bluetooth_adapter::kErrorAuthenticationCanceled,
                       "Cancaled");
  } else {
    Properties* properties = GetProperties(object_path);

    properties->paired.ReplaceValue(true);
    callback.Run();

    AddInputDeviceIfNeeded(object_path, properties);
  }
}

void FakeBluetoothDeviceClient::TimeoutSimulatedPairing(
    const dbus::ObjectPath& object_path,
    const ErrorCallback& error_callback) {
  VLOG(1) << "TimeoutSimulatedPairing: " << object_path.value();

  error_callback.Run(bluetooth_adapter::kErrorAuthenticationTimeout,
                     "Timed out");
}

void FakeBluetoothDeviceClient::CancelSimulatedPairing(
    const dbus::ObjectPath& object_path,
    const ErrorCallback& error_callback) {
  VLOG(1) << "CancelSimulatedPairing: " << object_path.value();

  error_callback.Run(bluetooth_adapter::kErrorAuthenticationCanceled,
                     "Canceled");
}

void FakeBluetoothDeviceClient::RejectSimulatedPairing(
    const dbus::ObjectPath& object_path,
    const ErrorCallback& error_callback) {
  VLOG(1) << "RejectSimulatedPairing: " << object_path.value();

  error_callback.Run(bluetooth_adapter::kErrorAuthenticationRejected,
                     "Rejected");
}

void FakeBluetoothDeviceClient::FailSimulatedPairing(
    const dbus::ObjectPath& object_path,
    const ErrorCallback& error_callback) {
  VLOG(1) << "FailSimulatedPairing: " << object_path.value();

  error_callback.Run(bluetooth_adapter::kErrorFailed, "Failed");
}

void FakeBluetoothDeviceClient::AddInputDeviceIfNeeded(
    const dbus::ObjectPath& object_path,
    Properties* properties) {
  // If the paired device is a HID device based on it's bluetooth class,
  // simulate the Input interface.
  FakeBluetoothInputClient* fake_bluetooth_input_client =
      static_cast<FakeBluetoothInputClient*>(
          DBusThreadManager::Get()->GetExperimentalBluetoothInputClient());

  if ((properties->bluetooth_class.value() & 0x001f03) == 0x000500)
    fake_bluetooth_input_client->AddInputDevice(object_path);
}

void FakeBluetoothDeviceClient::PinCodeCallback(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    ExperimentalBluetoothAgentServiceProvider::Delegate::Status status,
    const std::string& pincode) {
  VLOG(1) << "PinCodeCallback: " << object_path.value();

  if (status == ExperimentalBluetoothAgentServiceProvider::Delegate::SUCCESS) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(3 * simulation_interval_ms_));

  } else if (status ==
             ExperimentalBluetoothAgentServiceProvider::Delegate::CANCELLED) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (status ==
             ExperimentalBluetoothAgentServiceProvider::Delegate::REJECTED) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  }
}

void FakeBluetoothDeviceClient::PasskeyCallback(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    ExperimentalBluetoothAgentServiceProvider::Delegate::Status status,
    uint32 passkey) {
  VLOG(1) << "PasskeyCallback: " << object_path.value();

  if (status == ExperimentalBluetoothAgentServiceProvider::Delegate::SUCCESS) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(3 * simulation_interval_ms_));

  } else if (status ==
             ExperimentalBluetoothAgentServiceProvider::Delegate::CANCELLED) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (status ==
             ExperimentalBluetoothAgentServiceProvider::Delegate::REJECTED) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  }
}

void FakeBluetoothDeviceClient::ConfirmationCallback(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    ExperimentalBluetoothAgentServiceProvider::Delegate::Status status) {
  VLOG(1) << "ConfirmationCallback: " << object_path.value();

  if (status == ExperimentalBluetoothAgentServiceProvider::Delegate::SUCCESS) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(3 * simulation_interval_ms_));

  } else if (status ==
             ExperimentalBluetoothAgentServiceProvider::Delegate::CANCELLED) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (status ==
             ExperimentalBluetoothAgentServiceProvider::Delegate::REJECTED) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  }
}

void FakeBluetoothDeviceClient::SimulateKeypress(
    uint16 entered,
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "SimulateKeypress " << entered << ": " << object_path.value();

  FakeBluetoothAgentManagerClient* fake_bluetooth_agent_manager_client =
      static_cast<FakeBluetoothAgentManagerClient*>(
          DBusThreadManager::Get()->
              GetExperimentalBluetoothAgentManagerClient());
  FakeBluetoothAgentServiceProvider* agent_service_provider =
      fake_bluetooth_agent_manager_client->GetAgentServiceProvider();
  agent_service_provider->DisplayPasskey(object_path, 123456, entered);

  if (entered < 7) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::SimulateKeypress,
                   base::Unretained(this),
                   entered + 1, object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  }
}

}  // namespace chromeos
